#include "init.h"
#include "mem/AnonPool.h"
#include "mem/PhysicalAllocator.h"
#include "mem/StackPool.h"
#include "mem/Heap.h"
#include "vm/Mapper.h"
#include "vm/Map.h"
#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sys/Syscall.h"
#include "handle/Manager.h"

#include <arch.h>
#include <platform.h>
#include <stdint.h>

#include <printf.h>
#include <log.h>

/// root init server
sched::Task *gRootServer = nullptr;

/**
 * Early kernel initialization
 */
void kernel_init() {
    // set up the physical allocator and VM system
    mem::PhysicalAllocator::init();

    vm::Mapper::init();
    mem::PhysicalAllocator::vmAvailable();

    vm::Mapper::loadKernelMap();
    vm::Mapper::lateInit();

    mem::AnonPool::init();
    mem::StackPool::init();
    mem::Heap::init();

    handle::Manager::init();
    sys::Syscall::init();

    // notify architecture we've got VM
    arch_vm_available();

    // set up scheduler (platform code may set up threads)
    sched::Scheduler::init();

    // notify other components of VM availability
    platform_vm_available();

}

/**
 * After all systems have been initialized, we'll jump here. Higher level kernel initialization
 * takes place and we go into the scheduler.
 */
void kernel_main() {
    log("kush time: PC = $%p", get_pc());


    // invoke the scheduler
    sched::Scheduler::get()->run();
    panic("scheduler returned, this should never happen");
}
