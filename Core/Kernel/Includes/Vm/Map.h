#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include <stddef.h>
#include <stdint.h>

#include <Init.h>
#include <Runtime/RefCountable.h>
#include <Vm/Types.h>
#include <platform/PageTable.h>
#include <platform/Processor.h>
#include <platform/ProcessorLocals.h>

namespace Kernel::Vm {
class MapEntry;

/**
 * @brief Virtual memory map
 *
 * These memory maps have a 1:1 correspondence to a set of hardware page tables. Each map consists
 * of multiple map entries.
 *
 * Internally, each map is backed by a platform-specific page table structure. This structure is
 * directly manipulated by VM objects (in order to add, modify or remove individual page mappings
 * to physical addresses) to change the page table. Outside of the VM object implementation, you
 * should always prefer to interact with maps through the higher level API.
 *
 * \section{Initialization}
 * Maps may be freely created as more unique memory spaces are required, with only a few caveats:
 *
 * 1. The first map that is created is automatically registered as the kernel's memory map. This
 *    means that any subsequently created maps will have this map as its "parent."
 *
 *    This behavior isn't set up until the kernel entry point is invoked (that's where the parent
 *    map is set up) so this caveat does not apply to early platform/arch init code; though that
 *    code should really only ever be creating one instance (the initial kernel map) anyways.
 */
class Map: public Runtime::RefCountable<Map> {
    friend class MapEntry;
    friend class PageAllocator;
    friend void ::Kernel::Start(Kernel::Vm::Map *);


    public:
        Map(Map *parent = nullptr);
        ~Map();

        void activate();

        [[nodiscard]] int add(const uintptr_t base, MapEntry *entry);
        [[nodiscard]] int remove(MapEntry *entry);

        [[nodiscard]] int getEntryAt(const uintptr_t vaddr, MapEntry* &outEntry);

        /**
         * @brief Get currently active map
         *
         * Retrieve the map that's currently mapped on the calling processor. This information
         * is retrieved from the processor's local storage.
         *
         * @return Map object currently active, if any
         */
        inline static Map *Current() {
            return Platform::ProcessorLocals::GetKernelData()->map;
        }

        /**
         * @brief Get kernel map
         *
         * Retrieve the map corresponding to the kernel's virtual address space. All subsequently
         * allocated maps will use the kernel map for the upper (kernel) part of its address
         * space.
         */
        inline static Map *Kernel() {
            return gKernelMap;
        }

        [[nodiscard]] int invalidateTlb(const uintptr_t virtualAddr, const size_t length,
                const TlbInvalidateHint hints);

        [[nodiscard]] int handleFault(Platform::ProcessorState &state, const uintptr_t faultAddr,
                const FaultAccessType accessType);

    private:
        void deactivate();

        [[nodiscard]] int findEntry(MapEntry *entry, uintptr_t &outVirtBase,
                size_t &outSize);
        [[nodiscard]] int getEntryAt(const uintptr_t vaddr, MapEntry* &outEntry,
                uintptr_t &outEntryBase, size_t &outEntrySize);

        [[nodiscard]] int doTlbShootdown(const uintptr_t virtualAddr, const size_t length,
                const TlbInvalidateHint hints);

    private:
        /// Map object for the kernel map
        static Map *gKernelMap;

        /**
         * @brief Parent map
         *
         * The parent map is used for the kernel space mappings, if the platform has a concept of
         * separate kernel and userspace address spaces.
         */
        Map *parent{nullptr};

        /**
         * @brief Bitmap for active processors
         *
         * Indicates which processors currently have this mapped. This is used to send TLB
         * shootdowns to other processors.
         */
        uint64_t mappedCpus{0};

        /**
         * @brief Platform page table instance
         *
         * This is the platform-specific wrapper to actually write out page tables, which can be
         * understood by the processor. Whenever a VM object wishes to change the page mappings, it
         * calls into methods on this object.
         */
        Platform::PageTable pt;
};
}

#endif
