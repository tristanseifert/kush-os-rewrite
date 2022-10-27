/**
 * @file
 *
 * @brief Virtual page allocator
 *
 * Thie file contains the implementation of the virtual page allocator. Currently, this is an
 * extremely naiive implementation that simply moves a cursor through the virtual memory region
 * reserved for the virtual allocator.
 *
 * Underlying physical memory is allocated directly from the physical memory allocator, and the
 * kernel pagetables are also directly manipualted.
 */
#include "Vm/Manager.h"
#include "Vm/Map.h"
#include "Vm/PageAllocator.h"

#include "Exceptions/Handler.h"
#include "Memory/PhysicalAllocator.h"
#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>
#include <new>

using namespace Kernel::Vm;

/**
 * @brief Allocation cursor
 *
 * This dude points to the start of the region of virtual memory space at which we should begin
 * allocating new pages. The pointer is continuously advanced until it reaches the end of the
 * virtual address region. (At which point we panic, but…)
 */
uintptr_t PageAllocator::gAllocCursor{Platform::KernelAddressLayout::VAllocStart};

/**
 * @brief Total number of allocated pages
 *
 * Counter to keep track of the number of pages we've allocated.
 */
uintptr_t PageAllocator::gPagesAllocated{0};


/**
 * @brief Initialize the virtual page allocator
 */
void PageAllocator::Init() {
    gAllocCursor = Platform::KernelAddressLayout::VAllocStart;
    gPagesAllocated = 0;
}

/**
 * @brief Handle a page fault
 *
 * Any fault inside the page allocator results in a kernel panic.
 */
int PageAllocator::HandleFault(Platform::ProcessorState &state, const uintptr_t address,
        const FaultAccessType access) {
    Exceptions::Handler::AbortWithException(Exceptions::Handler::ExceptionType::PageFault,
            state, reinterpret_cast<void *>(address), "Fault in valloc region");
}



/**
 * @brief Allocate a range of virtual memory for kernel use
 *
 * Returns the starting address of a page aligned, virtually contiguous region of memory. The
 * underlying physical memory is allocated directly from the physical allocator.
 *
 * @param length Length of the allocation, in bytes. Rounded up to the nearest page multiple
 *
 * @return Starting address of the first page in the allocated region, or NULL on failure
 *
 * @remark The maximum size of an allocation through this mechanism is limited.
 *
 * @TODO Add thread safety (locking) support
 */
void *PageAllocator::Alloc(const size_t length) {
    int err;
    uintptr_t virt;

    // check if alloc pointer would overflow
    const auto pageLength = Platform::PageTable::NearestPageSize(length),
        pageLengthWithGuards = pageLength + (kNumGuardPages * Platform::PageTable::PageSize());
    const auto end = gAllocCursor + pageLengthWithGuards;

    REQUIRE(end < Platform::KernelAddressLayout::VAllocEnd &&
            end > gAllocCursor, "PageAllocator internal inconsistency: alloc ptr %p, request %u",
            gAllocCursor, pageLength);

    // allocate physical pages
    uint64_t phys[kMaxAllocPages]{};
    size_t numPages = pageLength / Platform::PageTable::PageSize();

    err = PhysicalAllocator::AllocatePages(numPages, phys);
    if(err < -1) { // allocation failure
        return nullptr;
    } else if(err != numPages) { // partial allocation
        PhysicalAllocator::FreePages(err, phys);
        return nullptr;
    }

    // then map them into the kernel's map
    auto map = Map::Kernel();
    REQUIRE(map, "invalid kernel map? wtf");

    virt = gAllocCursor;

    for(size_t i = 0; i < numPages; i++) {
        err = map->pt.mapPage(phys[i], virt, Kernel::Vm::Mode::KernelRW);
        // TODO: can we handle this error better?
        REQUIRE(!err, "failed to map virtual page: %d", err);

        virt += Platform::PageTable::PageSize();
    }

    // TODO: record the allocation somewhere

    // update allocator state
    auto startPtr = reinterpret_cast<void *>(gAllocCursor);
    gAllocCursor += pageLengthWithGuards;
    gPagesAllocated += numPages;

    if(kLogAlloc) {
        Console::Trace("PageAlloc: ptr=%p, %u pages", gAllocCursor, gPagesAllocated);
    }

    return startPtr;
}

/**
 * @brief Release a previously allocated virtual memory region
 *
 * This returns the underlying physical pages to the physical allocator pool, and unmaps them from
 * the virtual memory.
 *
 * @param ptr Start of the virtual region previously allocated
 * @param length Length of the allocation, in bytes.
 *
 * @TODO Add thread safety (locking) support
 */
void PageAllocator::Free(void *ptr, const size_t length) {
    int err;
    uint64_t phys[kMaxAllocPages]{};
    Vm::Mode mode;

    // validate inputs
    REQUIRE(ptr && length, "invalid arguments (%s)", __FUNCTION__);
    REQUIRE(!(reinterpret_cast<uintptr_t>(ptr) % Platform::PageTable::PageSize()),
            "unaligned start ptr: %p", ptr);

    const auto pageLength = Platform::PageTable::NearestPageSize(length);
    size_t numPages = pageLength / Platform::PageTable::PageSize();

    // read out the corresponding physical page addresses
    auto map = Map::Kernel();
    REQUIRE(map, "invalid kernel map? wtf");

    for(size_t i = 0; i < numPages; i++) {
        const auto virt = reinterpret_cast<uintptr_t>(ptr) + (i * Platform::PageTable::PageSize());

        err = map->pt.getPhysAddr(virt, phys[i], mode);
        REQUIRE(err == 1, "%s failed: %d", "PageTable::getPhysAddr", err);
    }

    // unmap the pages
    err = map->pt.unmap(reinterpret_cast<uintptr_t>(ptr), pageLength);
    REQUIRE(!err, "%s failed: %d", "PageTable::unmap", err);

    // update TLBs (extremely important!)
    err = map->invalidateTlb(reinterpret_cast<uintptr_t>(ptr), pageLength,
            TlbInvalidateHint::InvalidateAll | TlbInvalidateHint::Unmapped);
    REQUIRE(!err, "failed to invalidate tlb: %d", err);

    // release the underlying physical pages
    err = PhysicalAllocator::FreePages(numPages, phys);
    REQUIRE(err == numPages, "failed to release phys pages: %d", err);

    gPagesAllocated -= numPages;

    // TODO: update recorded allocation state
    if(kLogFrees) {
        Console::Trace("PageAlloc: ptr=%p, %u pages", gAllocCursor, gPagesAllocated);
    }
}



/**
 * @brief Allocate contiguous virtual memory
 *
 * @param length Number of bytes to allocate; rounded up to the nearest page size
 *
 * @return Start of virtual address space, or NULL on error
 */
void *Kernel::Vm::VAlloc(const size_t length) {
    return PageAllocator::Alloc(length);
}

/**
 * @brief Free a range of virtual memory
 *
 * @param ptr A pointer previously returned from VAlloc
 * @param length Length of the allocation
 *
 * @remark The length parameter _must_ match the allocation or a panic results.
 */
void Kernel::Vm::VFree(void *ptr, const size_t length) {
    PageAllocator::Free(ptr, length);
}
