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

        // switch to our page tables
        this->pt.activate();

        // TODO: update mapped bitmap
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

    // find entry and notify we're unmapping
    err = this->findEntry(entry, virtBase);
    if(!err) {
        // TODO: standardized error codes
        return -1;
    } else if(err < 0) {
        return err;
    }

    regionSize = entry->getLength();

    entry->willRemoveFrom(virtBase, *this, this->pt);

    // TODO: unmap its entire range

    // TODO: clean up (remove from list, etc)

    entry->release();

    // invalidate the address range (local + remote)
    err = this->invalidateTlb(virtBase, regionSize, TlbInvalidateHint::InvalidateAll |
            TlbInvalidateHint::Unmapped);
    if(err < 0) {
        // TODO: will this fail the entire operation?
        return err;
    }

    // TODO implement
    return -1;
}

/**
 * @brief Find map entry corresponding to virtual address
 *
 * @param vaddr Virtual address to search for an entry for
 * @param outEntry If found, a pointer to the entry (you _must_ release it when done!)
 *
 * @return 1 if found, 0 if not found, or a negative error code
 *
 * Locates a map entry that corresponds to the specified virtual address. The address is
 * understood to be a single byte.
 */
int Map::getEntryAt(const uintptr_t vaddr, MapEntry* &outEntry) {
    // TODO: implement
    return -1;
}



/**
 * @brief Look up the base address of an entry
 *
 * Search the map to see where in virtual memory the specified entry is mapped, if any.
 *
 * @param entry Entry to search for
 * @param outVirtBase Base virtual address of the entry in this map
 *
 * @return 1 if entry is found in this map, 0 if not found, or a negative error code.
 */
int Map::findEntry(MapEntry *entry, uintptr_t &outVirtBase) {
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
    // test inputs
    if(!TestFlags(hints & TlbInvalidateHint::MaskInvalidate)) {
        // TODO: indicate API misuse?
        return 0;
    }

    // invalidate local caches, if requested
    if(TestFlags(hints & TlbInvalidateHint::InvalidateLocal)) {
        this->pt.invalidateTlb(virtualAddr, length, hints);
    }

    // invalidate remote caches, if requested
    if(TestFlags(hints & TlbInvalidateHint::InvalidateRemote)) {
        this->doTlbShootdown(virtualAddr, length, hints);
    }

    // TODO: implement
    return -1;
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
    return -1;
}
