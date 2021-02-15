#include "syscall.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <string.h>

/**
 * Builds a syscall flag value.
 *
 * You may notice this is the same as the ones defined in the header; therefore no real translation
 * is taking place here. This allows us flexibility to change the flags without changing the actual
 * programs later.
 */
static uintptr_t BuildSyscallFlags(const uintptr_t inFlags) {
    uintptr_t temp = 0;

    if(inFlags & VM_REGION_READ) {
        temp |= (1 << 10);
    }
    if(inFlags & VM_REGION_WRITE) {
        temp |= (1 << 11);
    }
    if(inFlags & VM_REGION_EXEC) {
        temp |= (1 << 12);
    }
    if(inFlags & VM_REGION_MMIO) {
        temp |= (1 << 13);
    }
    if(inFlags & VM_REGION_WRITETHRU) {
        temp |= (1 << 14);
    }

    return temp;
}


/**
 * Creates a new virtual memory mapping backed by a contiguous range of physical addresses.
 */
int AllocVirtualRegion(const uint64_t physAddr, const uintptr_t virtualAddr, const uintptr_t size,
        const uintptr_t inFlags, uintptr_t *outHandle) {
    int err;

    // build flags
    uintptr_t flags = 0;
    flags = BuildSyscallFlags(inFlags);

    // perform syscall
    err = __do_syscall3(SYS_VM_CREATE | (flags << 16), physAddr, virtualAddr, size);

    // extract handle if desired; return 0 for success, syscall error otherwise
    if(outHandle && err > 0) {
        *outHandle = err;
    }

    return (err < 0) ? err : 0;
}

