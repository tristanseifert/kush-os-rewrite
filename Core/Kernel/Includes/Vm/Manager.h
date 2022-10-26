#ifndef KERNEL_VM_MANAGER_H
#define KERNEL_VM_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#include <Bitflags.h>
#include <platform/Processor.h>
#include <Vm/Types.h>

/// Virtual memory subsystem
namespace Kernel::Vm {
/**
 * @brief Virtual memory manager
 *
 * The virtual memory manager is primarily responsible for satisfying page faults.
 */
class Manager {
    public:
        static void Init();

        static void HandleFault(Platform::ProcessorState &state, const uintptr_t faultAddr);

    private:
        /// Shared (global) VM manager instance
        static Manager *gShared;
};
}

#endif
