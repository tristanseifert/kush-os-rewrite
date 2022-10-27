#include "Vm/Map.h"
#include "Vm/Manager.h"
#include "Vm/MapEntry.h"

#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>

using namespace Kernel::Vm;

/**
 * Kernel virtual memory map
 *
 * The first VM object created is assigned to this variable. Any subsequently created maps which do
 * not explicitly specify a parent value will use this map as their parent, so that any shared
 * kernel data can be provided to them all.
 */
Map *Map::gKernelMap{nullptr};

/**
 * @brief Initialize a new map.
 *
 * @param parent A map to use as its parent; if `nullptr` is specified, the kernel default map is
 *        assumed to be the parent.
 */
Map::Map(Map *parent) : parent(parent ? parent->retain() :
        (gKernelMap ? gKernelMap->retain() : nullptr)),
    pt(Platform::PageTable(this->parent ? &this->parent->pt : nullptr)) {
}

/**
 * @brief Release the memory map
 *
 * The underlying physical memory for the page tables will be released back to the system, and
 * any references to other maps are dropped.
 */
Map::~Map() {
    // release reference to parent
    if(this->parent) {
        this->parent->release();
    }
}

/**
 * @brief Activates this virtual memory map on the calling processor.
 *
 * This just thunks directly to the platform page table handler, which will invoke some sort of
 * processor-specific stuff to actually load the page tables.
 */
void Map::activate() {
    {
        // TODO: critical section

        // TODO: invoke unmap callback of previous map
        auto last = Map::Current();
        if(last) {
            last->deactivate();
        }

        // TODO: update mapped bitmap

        // switch to our page tables
        this->pt.activate();
        Platform::ProcessorLocals::GetKernelData()->map = this;
    }

    // TODO: invoke post-mapping callback
}

/**
 * @brief Perform bookkeeping when unmapping
 *
 * Invoked immediately before the recipient is unmapped on the calling processor. At the time of
 * the call, it's still mapped, however, and implies it's active.
 *
 * @remark This is invoked under critical section.
 */
void Map::deactivate() {
    // TODO: update mapped bitmap
}

/**
 * @brief Adds the given map entry to this map.
 *
 * @param base Base address for the mapping. The entire region of `[base, base+length]` must be
 *        available in the map.
 * @param entry The memory map entry to map. It will be retained.
 *
 * @return 0 on success or a negative error code.
 *
 * We do not have to worry about notifying other cores, or invalidating TLBs here. It's assumed
 * that when a new mapping is added, and we've ascertained that it doesn't conflict with any
 * existing ranges, that all processors do not have TLB entries for that range.
 */
int Map::add(const uintptr_t base, MapEntry *entry) {
    if(!base || !entry) {
        // TODO: standardized error codes
        return -1;
    }

    // TODO: acquire write lock

    // TODO: check for overlap

    // TODO: add it to a list
    entry = entry->retain();

    // notify the entry (so it may update the pagetable)
    entry->addedTo(base, *this, this->pt);

    return 0;
}

/**
 * @brief Remove a map entry from this map
 *
 * @param entry Map entry to remove; its entire address range is unmapped.
 *
 * @return 0 on success or a negative error code.
 *
 * This call will invalidate the local/calling processor's TLB for the entire address range the
 * map entry covers. Additionally, any other processors that have this entry mapped will likewise
 * be notified (TLB shootdown)
 */
int Map::remove(MapEntry *entry) {
    int err;
    uintptr_t virtBase{0};
    size_t regionSize{0};

    if(!entry) {
        // TODO: standardized error codes
        return -1;
    }

    // TODO: acquire write lock (naiive, but whatever)

    // find entry
    err = this->findEntry(entry, virtBase, regionSize);
    if(!err) {
        // TODO: standardized error codes
        return -1;
    } else if(err < 0) {
        return err;
    }

    // invoke its callback: this shall unmap pages and invalidate TLBs
    entry->willRemoveFrom(virtBase, regionSize, *this, this->pt);

    // TODO: clean up (remove from list, etc)

    entry->release();

    // TODO implement
    return -1;
}

/**
 * @brief Find map entry corresponding to virtual address
 *
 * @param vaddr Virtual address to search for an entry for
 * @param outEntry If found, a pointer to the entry (you _must_ release it when done!)
 * @param outEntryBase Base address of the entry in this map
 * @param outEntrySize Size of the entry (in bytes)
 *
 * @return 1 if found, 0 if not found, or a negative error code
 *
 * Locates a map entry that corresponds to the specified virtual address. The address is
 * understood to be a single byte.
 */
int Map::getEntryAt(const uintptr_t vaddr, MapEntry* &outEntry, uintptr_t &outEntryBase,
        size_t &outEntrySize) {
    // TODO: implement
    return -1;
}

/**
 * @brief Service a page fault
 *
 * @param state Current processor state
 * @param address Virtual address of the fault
 * @param accessType Type of memory access that triggered the fault
 *
 * @return 1 if fault is handled, 0 if the next handler should be tried, or a negative error.
 */
int Map::handleFault(Platform::ProcessorState &state, const uintptr_t address,
        const FaultAccessType accessType) {
    int err;
    MapEntry *entry{nullptr};
    size_t entrySize{0};
    uintptr_t entryBase{0};

    // get the corresponding VM entry
    err = this->getEntryAt(address, entry, entryBase, entrySize);

    if(err == 0) { // not found
        return 0;
    } else if(err < 0) { // error
        return err;
    }

    // invoke the entry's handler
    const uintptr_t offset = entryBase - address;
    REQUIRE(offset <= entrySize, "invalid fault offset: base %p fault %p", entryBase, address);

    err = entry->handleFault(*this, offset, accessType);
    entry->release();

    if(err == 0) { // successfully handled
        return 1;
    } else if(err < 0) { // error
        return err;
    } else { // otherwise, try the next handler
        return 0;
    }
}



/**
 * @brief Look up the base address of an entry
 *
 * Search the map to see where in virtual memory the specified entry is mapped, if any.
 *
 * @param entry Entry to search for
 * @param outVirtBase Base virtual address of the entry in this map
 * @param outSize Number of bytes of outSize that are mapped
 *
 * @return 1 if entry is found in this map, 0 if not found, or a negative error code.
 */
int Map::findEntry(MapEntry *entry, uintptr_t &outVirtBase, size_t &outSize) {
    // TODO: implement
    return -1;
}



/**
 * @brief Invalidate local and/or remote TLBs
 *
 * @param virtualAddr Start of the virtual address range that was invalidated
 * @param length Length of the virtual address range, in bytes; must be page multiple
 * @param hints How to process the TLB invalidation
 *
 * @return 0 on success, or a negative error code.
 */
int Map::invalidateTlb(const uintptr_t virtualAddr, const size_t length,
        const TlbInvalidateHint hints) {
    int err;

    // test inputs
    if(!TestFlags(hints & TlbInvalidateHint::MaskInvalidate)) {
        // TODO: indicate API misuse?
        return 0;
    }

    // invalidate local caches, if requested
    if(TestFlags(hints & TlbInvalidateHint::InvalidateLocal)) {
        err = this->pt.invalidateTlb(virtualAddr, length, hints);
        if(err < 0) {
            return err;
        }
    }

    // invalidate remote caches, if requested
    if(TestFlags(hints & TlbInvalidateHint::InvalidateRemote)) {
        err = this->doTlbShootdown(virtualAddr, length, hints);
        if(err < 0) {
            return err;
        }
    }

    return 0;
}

/**
 * @brief Invalidate TLB on remote processors
 *
 * Perform a TLB shootdown on any remote processors that have this map active.
 *
 * @param virtualAddr Start of the virtual address range that was invalidated
 * @param length Length of the virtual address range, in bytes; must be page multiple
 * @param hints How to process the TLB invalidation (only the changes are considered)
 *
 * @return 0 on success, or a negative error code
 */
int Map::doTlbShootdown(const uintptr_t virtualAddr, const size_t length,
        const TlbInvalidateHint hints) {
    // TODO: if nobody else has this map active, bail

    // TODO: implement
    return 0;
}
