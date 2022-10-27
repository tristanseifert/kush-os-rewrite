#include "PageTable.h"
#include "KernelMemoryMap.h"
#include "Memory/PhysicalMap.h"

#include <Logging/Console.h>
#include <Memory/PhysicalAllocator.h>
#include <Runtime/String.h>
#include <Vm/Map.h>

using Kernel::Logging::Console;
using namespace Platform::Amd64Uefi;

/**
 * @brief Flag indicating whether this is the first allocated page table.
 *
 * This is used to "hack in" the physical aperture: it won't have a backing VM object so that we
 * can just spam some 1G sized pages.
 */
static bool isFirstPageTable{false};

/**
 * @brief Initialize a new amd64 page table.
 *
 * We use 4 level paging, giving us 48-bit virtual addresses. We'll copy every PML4 entry in the
 * parent map above the kernel split into this one, so that kernel addresses are always mapped.
 *
 * @param parent Page table to take kernel mappings from (specify `nullptr` to skip)
 */
PageTable::PageTable(PageTable *parent) {
    // allocate PML4
    this->pml4Phys = AllocPage();

    // copy PML4 entries from parent
    if(parent) {
        this->copyPml4Upper(parent);
    }

    // add phys aperture if needed
    bool yes{true}, no{false};
    if(__atomic_compare_exchange(&isFirstPageTable, &no, &yes, false, __ATOMIC_ACQ_REL,
                __ATOMIC_RELAXED)) {
        this->mapPhysAperture();
    }
}

/**
 * @brief Release all physical memory used by this page table.
 *
 * This will recurse through the entire page table, down to the page directory, and freeing every
 * page table; then freeing the page directories themselves, and so on until only PML4 is left,
 * which is then freed also.
 */
PageTable::~PageTable() {
    REQUIRE(false, "not implemented");

    // release PML4
    Kernel::PhysicalAllocator::FreePages(1, &this->pml4Phys);
}

/**
 * @brief Copies all PML4 entries above 0x8000'0000'0000'0000 in the specified page table.
 */
void PageTable::copyPml4Upper(PageTable *parent) {
    for(size_t j = 0x100; j < 512; j++) {
        auto pml4e = ReadTable(parent->pml4Phys, j);
        WriteTable(this->pml4Phys, j, pml4e);
    }
}

/**
 * @brief Create an aperture into physical memory.
 *
 * This manually creates enough PDPTs to fit the entire region, then uses 1G pages to map all of
 * the physical memory. For the default 2TB of physical aperture, we'll need four PDPTs.
 */
void PageTable::mapPhysAperture() {
    constexpr const auto len{KernelAddressLayout::PhysApertureEnd+1-KernelAddressLayout::PhysApertureStart};

    for(size_t i = 0; i < (len / 0x8000000000ULL); i++) {
        // allocate the PDPT
        uint64_t pdpt = AllocPage();
        REQUIRE(pdpt, "failed to allocate %s", "PDPT");

        const uint64_t physBase = (i * 0x8000000000ULL);

        // fill in all 512 entries
        for(size_t j = 0; j < 512; j++) {
            uint64_t val = (physBase + (j * 0x40000000ULL));

            val |= 0b10000011; // present, RW, supervisor, 1G
            if(kNoExecuteEnabled) {
                val |= (1ULL << 63); // execute disable
            }

            // mark it as global (shared between all processes)
            val |= (1ULL << 8);

            // write entry
            WriteTable(pdpt, j, val);
        }

        // store PDPT in the PML4
        uint64_t pml4e = (pdpt & ~0xFFF);
        pml4e |= 0b00000011; // present, RW, supervisor

        if(kNoExecuteEnabled) {
            pml4e |= (1ULL << 63); // execute disable
        }

        WriteTable(this->pml4Phys, 256 + i, pml4e);
    }
}


/**
 * @brief Maps a single page into the page table, allocating any intermediary paging structures as needed.
 *
 * @param phys Physical address to map to
 * @param _virt Virtual address to map
 * @param mode Page access mode
 *
 * @return 0 on success or a negative error code
 */
int PageTable::mapPage(const uint64_t phys, const uintptr_t _virt, const Kernel::Vm::Mode mode) {
    uintptr_t ptAddr{0};
    bool write, global{false}, user, execute;

    user = TestFlags(mode & Kernel::Vm::Mode::UserMask);
    write = TestFlags(mode & Kernel::Vm::Mode::Write);
    execute = TestFlags(mode & Kernel::Vm::Mode::Execute);

    // ensure virtual address is canonical
    if(_virt > 0x00007FFFFFFFFFFF && _virt < 0xFFFF800000000000) {
        // TODO: error code enum
        return -1000;
    }

    // TODO: redirect all upper mapping requests to kernel map

     /*
     * Step through the PML4, PDPT, PDT, and eventually locate the address of the page table in
     * physical memory. If needed, we'll allocate pages for all of these. If there exists a mapping
     * for a larger page (for example, a 2M page in place of a page table pointer) we'll fail out.
     */
    const auto virt = _virt & 0xFFFFFFFFFFFF;

    if(kLogMapAdd) {
        Console::Trace("Adding mapping: virt $%016llx -> phys $%016llx r%s%s %s%s", _virt, phys,
                write ? "w" : "", execute ? "x" : "", global ? "global " : "",
                user ? "user" : "");
    }

    // read the PML4 entry
    const auto pml4eIdx = (virt >> 39) & 0x1FF;
    auto pml4e = ReadTable(this->pml4Phys, pml4eIdx);

    // PDPT not present; allocate one
    if(!(pml4e & (1 << 0))) {
        auto page = AllocPage();
        if(!page) {
            // TODO: error code enum
            return -1001;
        }

        // update the PML4 to point at this page directory pointer table
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(_virt < KernelAddressLayout::KernelBoundary) {
            // allow userspace accesses below kernel cutoff
            entry |= (1 << 2);
        }

        WriteTable(this->pml4Phys, pml4eIdx, entry);
        pml4e = entry;

        if(kLogAlloc) {
            Console::Trace("Allocated %s: %016llx", "PDPT", entry);
        }

        // TODO: do we need INVLPG? changing present 0 -> 1 does not require (Intel 4.10.4.3)
        // TODO: broadcast PML4 update to other maps that map the kernel map
    }

    // read the PDPT entry
    const auto pdptAddr = (pml4e & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    auto pdpte = ReadTable(pdptAddr, (virt >> 30) & 0x1FF);

    if(!(pdpte & (1 << 0))) { // allocate a PDT
        auto page = AllocPage();
        if(!page) {
            // TODO: error code enum
            return -1001;
        }

        // update the page directory pointer table to point at this page directory
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(_virt < KernelAddressLayout::KernelBoundary) {
            // allow userspace accesses below kernel cutoff
            entry |= (1 << 2);
        }

        WriteTable(pdptAddr, (virt >> 30) & 0x1FF, entry);
        pdpte = entry;

        if(kLogAlloc) {
            Console::Trace("Allocated %s: %016llx", "PDT", entry);
        }

        // TODO: do we need INVLPG? changing present 0 -> 1 does not require (Intel 4.10.4.3)
    } else if(pdpte & (1 << 7)) { // present, 1G page
        // TODO: error code enum
        return -1002;
    }

    // read the page directory entry to find page table
    const auto pdtAddr = (pdpte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdte = ReadTable(pdtAddr, (virt >> 21) & 0x1FF);

    if(!(pdte & (1 << 0))) { // allocate a page table
        auto page = AllocPage();
        if(!page) {
            // TODO: error code enum
            return -1001;
        }

        // update page directory to point at this page table
        uint64_t entry = page;
        entry |= 0b11; // present, writable
        if(_virt < KernelAddressLayout::KernelBoundary) {
            // allow userspace accesses below kernel cutoff
            entry |= (1 << 2);
        }

        WriteTable(pdtAddr, (virt >> 21) & 0x1FF, entry);
        pdte = entry;

        if(kLogAlloc) {
            Console::Trace("Allocated %s: %016llx", "PT", entry);
        }
        // TODO: do we need INVLPG? changing present 0 -> 1 does not require (Intel 4.10.4.3)
    } else if(pdte & (1 << 7)) { // present, 2M page
        // TODO: error code enum
        return -1003;
    }

    ptAddr = (pdte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    /*
     * Write the page table entry, into the page table whose physical address we've found above. We
     * need to build up the flags appropriately as well.
     */
    uint64_t pte = (phys & ~0xFFF) & ~static_cast<uint64_t>(PageFlags::FlagsMask);

    pte |= static_cast<uint64_t>(PageFlags::Present);

    if(write) {
        pte |= static_cast<uint64_t>(PageFlags::Writable);
    }
    if(global) {
        pte |= static_cast<uint64_t>(PageFlags::Global);
    }
    if(user) {
        pte |= static_cast<uint64_t>(PageFlags::UserAccess);
    }
    if(!execute && kNoExecuteEnabled) {
        pte |= static_cast<uint64_t>(PageFlags::NoExecute);
    }

    WriteTable(ptAddr, (virt >> 12) & 0x1FF, pte);
    return 0;
}



/**
 * @brief Resolve a virtual address to physical
 *
 * Get the physical address that corresponds to the given virtual address by traversing the page
 * tables the same way the processor's MMU would. The provided physical address _does_ have any
 * in-page offsets applied.
 *
 * @param _virt Virtual address to look up
 * @param outPhys Variable to receive the corresponding physical address
 * @param outMode Access permissions of the page
 *
 * @return 0 if the address is unmapped, 1 if it was mapped, or a negative error code.
 */
int PageTable::getPhysAddr(const uintptr_t _virt, uint64_t &outPhys, Kernel::Vm::Mode &outMode) {
    // ensure virtual address is canonical
    if(_virt > 0x00007FFFFFFFFFFF && _virt < 0xFFFF800000000000) {
        // TODO: error code enum
        return -1;
    }

     /*
     * Step through the PML4, PDPT, PDT, and eventually locate the address of the page table in
     * physical memory. If needed, we'll allocate pages for all of these. If there exists a mapping
     * for a larger page (for example, a 2M page in place of a page table pointer) we'll fail out.
     */
    const auto virt = _virt & 0xFFFFFFFFFFFF;

    // read the PML4 entry
    const auto pml4eIdx = (virt >> 39) & 0x1FF;
    auto pml4e = ReadTable(this->pml4Phys, pml4eIdx);

    // PDPT not present
    if(!(pml4e & (1 << 0))) {
        return 0;
    }

    // read the PDPT entry
    const auto pdptAddr = (pml4e & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    auto pdpte = ReadTable(pdptAddr, (virt >> 30) & 0x1FF);

    // no page mapped here
    if(!(pdpte & (1 << 0))) {
        return 0;
    }
    // a 1G page is mapped
    else if(pdpte & (1 << 7)) {
        DecodePTE(pdpte, outPhys, outMode);
        outPhys &= ~0x3FFFFFFF;
        outPhys += (_virt & 0x3FFFFFFF);
        return 0;
    }

    // read the page directory entry to find page table
    const auto pdtAddr = (pdpte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);
    auto pdte = ReadTable(pdtAddr, (virt >> 21) & 0x1FF);

    // no page table mapped
    if(!(pdte & (1 << 0))) {
        return 0;
    }
    // a 2M page is mapped
    else if(pdte & (1 << 7)) {
        DecodePTE(pdte, outPhys, outMode);
        outPhys &= ~0x1FFFFF;
        outPhys += (_virt & 0x1FFFFF);
        return 0;
    }

    const auto ptAddr = (pdte & ~0xFFF) & ~static_cast<uintptr_t>(PageFlags::FlagsMask);

    // lastly, read the page table entry
    auto pte = ReadTable(ptAddr, (virt >> 12) & 0x1FF);

    if(!(pte & (1 << 0))) {
        return 0;
    }

    // decode the page information
    DecodePTE(pte, outPhys, outMode);
    outPhys += (_virt & 0xFFF);

    return 1;
}


/**
 * @brief Invalidate a range of virtual memory
 *
 * Invalidate the TLB for all addresses in the specified range.
 *
 * @TODO Benchmark and optimize if this naiive approach is too slow
 */
int PageTable::invalidateTlb(const uintptr_t virt, const size_t length,
        const Kernel::Vm::TlbInvalidateHint) {
    const size_t numPages = NearestPageSize(length) / PageSize();

    for(size_t i = 0; i < numPages; i++) {
        const uintptr_t address = virt + (i * PageSize());
        asm volatile("invlpg (%0)" : : "b"(address) : "memory");
    }

    return 0;
}



/**
 * @brief Translate the physical address of a paging structure to a virtual address.
 *
 * This just converts the address to one in the physical aperture.
 *
 * @param base Physical base address
 */
uint64_t *PageTable::GetTableVmAddr(const uint64_t phys) {
    int err;

    // fast path: directly into the aperture
    if(!Memory::PhysicalMap::IsEarlyBoot()) [[likely]] {
        constexpr const auto len{KernelAddressLayout::PhysApertureEnd+1-KernelAddressLayout::PhysApertureStart};
        REQUIRE(phys < (len - PageSize()), "phys addr out of range of aperture: %016llx", phys);

        return reinterpret_cast<uint64_t *>(KernelAddressLayout::PhysApertureStart + phys);
    } 
    // slow path
    else {
        void *out{nullptr};
        err = Memory::PhysicalMap::Add(phys, PageSize(), &out);
        REQUIRE(!err, "failed to map page table: %d", err);

        return reinterpret_cast<uint64_t *>(out);
    }
}

/**
 * @brief Allocate a fresh page for use as a paging structure. Its contents are zeroised.
 *
 * @return Address of a physical memory page, or 0 on error.
 */
uint64_t PageTable::AllocPage() {
    // try to allocate page
    uint64_t page;
    int err = Kernel::PhysicalAllocator::AllocatePage(page);
    REQUIRE(err == 1, "failed to allocate page: %d", err);

    // zeroize it
    // TODO: physical allocator should zero memory for us!
    auto ptr = GetTableVmAddr(page);
    memset(ptr, 0, PageSize());

    return page;
}

/**
 * @brief Reads the nth entry of the paging table, with the given physical base address.
 *
 * @param tableBase Physical address of the first entry of the table
 * @param offset Index into the table [0, 512)
 *
 * @return The 64-bit value in the table at the provided offset.
 */
uint64_t PageTable::ReadTable(const uintptr_t tableBase, const size_t offset) {
    REQUIRE(offset <= 511, "table offset out of range: %zu", offset);

    auto ptr = GetTableVmAddr(tableBase);
    return ptr[offset];
}

/**
 * @brief Writes the nth entry of the specified paging table.
 *
 * @param tableBase Physical address of the first entry of the table
 * @param offset Index into the table [0, 512)
 * @param val Value to write at the index
 */
void PageTable::WriteTable(const uintptr_t tableBase, const size_t offset, const uint64_t val) {
    REQUIRE(offset <= 511, "table offset out of range: %zu", offset);

    auto ptr = GetTableVmAddr(tableBase);
    ptr[offset] = val;
}

/**
 * @brief Decode a page table entry
 *
 * All PTE's on x86 are symmetric, e.g. the physical address and flags are stored in the same
 * general area of the descriptor.
 *
 * @param pte Page table entry
 * @param outPhys Output for physical address
 * @param outMode Output for access mode
 */
void PageTable::DecodePTE(const uint64_t pte, uint64_t &outPhys, Kernel::Vm::Mode &outMode) {
    using Mode = Kernel::Vm::Mode;
    Mode temp{Mode::None};

    outPhys = (pte & 0xFFFFFFFFFF000);

    if(pte & (1 << 2)) { // user mode
        if(pte & (1 << 1)) { // writes allowed
            temp |= Mode::UserRW;
        } else {
            temp |= Mode::UserRead;
        }
        if(!(pte & (1ULL << 63ULL))) {
            temp |= Mode::UserExec;
        }
    } else { // kernel mode
        if(pte & (1 << 1)) { // writes allowed
            temp |= Mode::KernelRW;
        } else {
            temp |= Mode::KernelRead;
        }
        if(!(pte & (1ULL << 63ULL))) {
            temp |= Mode::KernelExec;
        }
    }

    outMode = temp;
}



/**
 * @brief Decode a page fault exception frame
 *
 * Read the error code from the stack frame and decode it per the page fault exception as specified
 * in the Intel manual, section 3A 4.7.
 */
void PageTable::DecodePageFault(const ProcessorState &state,
        Kernel::Vm::FaultAccessType &outMode) {
    using Mode = Kernel::Vm::FaultAccessType;

    Mode m{0};

    // page present flag
    if(state.errorCode & (1 << 0)) {
        m |= Mode::ProtectionViolation;
    } else {
        m |= Mode::PageNotPresent;
    }

    // read/write
    if(state.errorCode & (1 << 1)) {
        m |= Mode::Write;
    } else {
        m |= Mode::Read;
    }

    // privilege
    if(state.errorCode & (1 << 2)) {
        m |= Mode::User;
    } else {
        m |= Mode::Supervisor;
    }

    // reserved bit violation -> invalid descriptor
    if(state.errorCode & (1 << 3)) {
        m |= Mode::InvalidPTE;
    }

    // instruction fetch bit
    if(state.errorCode & (1 << 4)) {
        m |= Mode::InstructionFetch;
    }

    outMode = m;
}
