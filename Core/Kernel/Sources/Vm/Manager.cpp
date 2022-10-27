#include "Vm/Manager.h"
#include "Vm/Map.h"
#include "Vm/MapEntry.h"
#include "Vm/PageAllocator.h"

#include "Exceptions/Handler.h"
#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>
#include <new>

using namespace Kernel::Vm;

Manager *Manager::gShared{nullptr};
// space in .bss segment for the VM manager
static KUSH_ALIGNED(64) uint8_t gVmmBuf[sizeof(Manager)];

/**
 * @brief Initialize the global VM manager instance.
 */
void Manager::Init() {
    REQUIRE(!gShared, "cannot re-initialize VM manager");

    auto ptr = reinterpret_cast<Manager *>(&gVmmBuf);
    new (ptr) Manager();

    gShared = ptr;
}

/**
 * @brief Handles page faults.
 *
 * This looks up the corresponding VM object (if any) in the current address space for the
 * faulting address.
 *
 * @param state Processor state at the time of the fault
 * @param faultAddr Faulting virtual address
 */
void Manager::HandleFault(Platform::ProcessorState &state, const uintptr_t faultAddr) {
    int err;
    FaultAccessType type{0};

    // translate fault type
    Platform::PageTable::DecodePageFault(state, type);

    // is fault in the virtual page allocator or heap region?
    if(faultAddr >= Platform::KernelAddressLayout::VAllocStart &&
            faultAddr <= Platform::KernelAddressLayout::VAllocEnd) {
        err = PageAllocator::HandleFault(state, faultAddr, type);
        if(err == 1) {
            return;
        } else if(err < 0) {
            // TODO: this shouldn't happen (?)
            PANIC("PageAllocator::HandleFault (%p) failed: %d", faultAddr, err);
        }
    }

    // check the map entry corresponding to this fault
    auto map = Map::Current();
    if(map) {
        err = map->handleFault(state, faultAddr, type);

        // fault was handled
        if(err == 1) {
            return;
        }
        // forward to task fault handler
        else if(err == 0) {
            // TODO: implement this
        }
        // error while handling fault
        else if(err < 0) {
            // TODO: this shouldn't happen (?)
            PANIC("Map::handleFault (%p) failed: %d", faultAddr, err);
        }
    }


    // if the fault was caused by kernel code and is yet unhandled, abort
    if(state.getPc() >= Platform::KernelAddressLayout::KernelBoundary) {
        Exceptions::Handler::AbortWithException(Exceptions::Handler::ExceptionType::PageFault,
                state, reinterpret_cast<void *>(faultAddr));
    }
    // otherwise, forward the fault to the task
    else {
        // TODO: implement
    }
}

