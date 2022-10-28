/*
 * Initialization function for the kernel.
 */
#include <BuildInfo.h>
#include <Init.h>
#include <Logging/Console.h>
#include <Vm/Map.h>

#include "Vm/ContiguousPhysRegion.h"
#include "Vm/PageAllocator.h"

using namespace Kernel;

/**
 * @brief Print the startup banner
 */
static void PrintBanner() {
    Console::Notice("Welcome to \x1b[31mk\x1b[33mu\x1b[93ms\x1b[32mh\x1b[34m-"
            "\x1b[35mo\x1b[31ms\x1b[0m! Copyright 2022: Tristan Seifert");
    Console::Notice("Rev %s@%s, built %s for platform %s-%s\n", gBuildInfo.gitHash,
            gBuildInfo.gitBranch, gBuildInfo.buildDate, gBuildInfo.arch, gBuildInfo.platform);
}

/**
 * @Brief Initialize memory allocators
 */
static void InitAllocators() {
    Vm::PageAllocator::Init();

    Vm::Map::InitZone();
    Vm::ContiguousPhysRegion::InitZone();
}

/**
 * Kernel entry point
 *
 * This is where the kernel takes control from the platform-specific initialization code. The
 * machine state should be reasonably consistent, with a functional virtual memory map. We'll do
 * initialization here, roughly in two phases:
 *
 * 1. Initialize memory allocators, and bootstrap the virtual memory subsystem. The kernel will
 *    switch over to the newly generated memory map here.
 * 2. Set up the remainder of the system.
 *
 * Once initialization is complete, we attempt to load the initialization process from the boot
 * image, set up its main thread, and start the scheduler.
 *
 * @remark We expect the caller to set up the physical memory allocator, as well as setting up the
 *         initial memory map for the kernel.
 *
 * @param map Kernel virtual memory map object
 */
void Kernel::Start(Kernel::Vm::Map *map) {
    PrintBanner();

    // finish setting up virtual memory system
    REQUIRE(map, "invalid kernel memory map");
    Vm::Map::gKernelMap = map;

    InitAllocators();


    // TODO: initialize handle, object and syscall managers

    // TODO: do stuff

    // TODO: set up scheduler

    // TODO: call out into platform code to perform late initialization

    // TODO: start scheduler
}
