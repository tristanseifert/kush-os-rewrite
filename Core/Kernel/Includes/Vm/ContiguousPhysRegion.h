#ifndef KERNEL_VM_CONTIGUOUSPHYSREGION_H
#define KERNEL_VM_CONTIGUOUSPHYSREGION_H

#include <Vm/MapEntry.h>
#include <Vm/ZoneAllocator.h>

namespace Kernel::Vm {
constexpr static const char kPhysRegionAllocatorName[] = "ContiguousPhysRegion";

/**
 * @brief A contiguous region of physical memory
 *
 * This is a VM object subclass that represents a contiguous region of physical memory as a
 * region of virtual address space. This is particularly useful for peripheral devices, MMIO, and
 * actually mapping large swaths of physical address space.
 */
class ContiguousPhysRegion: public WithZoneAllocation<ContiguousPhysRegion, kPhysRegionAllocatorName>,
    public MapEntry {
    public:
        ContiguousPhysRegion(const uint64_t physBase, const size_t length, const Mode mode);

        void addedTo(const uintptr_t base, Map &map, Platform::PageTable &pt) override;
        void willRemoveFrom(const uintptr_t base, const size_t size, Map &map,
                Platform::PageTable &pt) override;

    private:
        /**
         * Physical base address
         *
         * This is the address at which the physical region begins.
         */
        uint64_t physBase;
};
}

#endif
