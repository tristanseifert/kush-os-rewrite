#ifndef KERNEL_VM_PAGEALLOCATOR_H
#define KERNEL_VM_PAGEALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include <Vm/Alloc.h>
#include <platform/Processor.h>

namespace Kernel::Vm {
/**
 * @brief Virtual page allocator
 *
 * This dude dispenses blocks of consecutive virtual address pages.
 */
class PageAllocator {
    public:
        static void Init();
        static int HandleFault(Platform::ProcessorState &state,
                const uintptr_t address, const FaultAccessType access);

        [[nodiscard]] static void *Alloc(const size_t length);
        static void Free(void *ptr, const size_t length);

    private:
        constexpr static const bool kLogAlloc{true};
        constexpr static const bool kLogFrees{true};

        /**
         * @brief Number of guard pages to insert between allocations
         *
         * Guard pages are used to catch accesses beyond the allocated region and trap them as
         * page faults.
         */
        constexpr static const size_t kNumGuardPages{2};

        /**
         * @brief Maximum number of pages that can be allocated in one call
         */
        constexpr static const size_t kMaxAllocPages{16};

        static uintptr_t gAllocCursor;
        static size_t gPagesAllocated;
};
}

#endif
