#include <arch.h>
#include <stdbool.h>
#include <stdint.h>
#include <printf.h>

#include "vm/PDPTPool.h"

#include <arch/x86_msr.h>

#include "gdt.h"
#include "idt.h"

static bool nxEnabled = false;
static void update_supports_nx();

/// x86 stack frame
struct stackframe {
    struct stackframe *ebp;
    uint32_t eip;
};

/**
 * Performs architecture initialization.
 *
 * Specifically, we set up the descriptor tables (GDT) and interrupt table (IDT) for protected
 * mode operation.
 */
void arch_init() {
    // determine if we support the NX bit; enable the feature if needed
    update_supports_nx();

    if(nxEnabled) {
        uint32_t lo, hi;
        x86_msr_read(X86_MSR_EFER, &lo, &hi);

        lo |= X86_MSR_EFER_NX;

        x86_msr_write(X86_MSR_EFER, lo, hi);
    }

    // initialize descriptors
    gdt_init();
    idt_init();
}

/**
 * Initialize some memory pools for allocating paging structures when VM is available.
 */
void arch_vm_available() {
    arch::vm::PDPTPool::init();

    gdt_setup_tss();
}

/**
 * x86 page size is always 4K. There's also support for 4M (2M in PAE mode) large pages.
 */
size_t arch_page_size() {
    return 4096;
}

/**
 * Whether the processor supports the no-execute bit or not.
 */
bool arch_supports_nx() {
    return nxEnabled;
}



/**
 * Performs a backtrace.
 *
 * The stack parameter should contain the base pointer (%ebp) value, or NULL to start with this
 * function.
 */
int arch_backtrace(void *stack, char *buf, const size_t bufLen) {
    struct stackframe *stk;

    if(!stack) {
        asm volatile("movl %%ebp,%0" : "=r"(stk) ::);
    } else {
        stk = reinterpret_cast<struct stackframe *>(stack);
    }

    // yeet out into the buffer the frames
    char *writePtr = buf;
    int written;

    for(size_t frame = 0; stk && frame < 50; ++frame) {
        const auto bufAvail = bufLen - (writePtr - buf);
        if(!bufAvail) return bufLen;

        written = snprintf(writePtr, bufAvail, "%2zu %08x\n", frame, stk->eip);

        // prepare for next frame
        writePtr += written;
        stk = stk->ebp;
    }

    return 1;
}




/**
 * Determine if the processor supports no-execute.
 *
 * For x86, we check CPUID leaf $80000001; bit 20 in EDX is set if we support NX, clear if not.
 */
static void update_supports_nx() {
    uint32_t eax, edx;
    x86_cpuid(0x80000001, &eax, &edx);

    nxEnabled = (edx & (1 << 20)) ? true : false;
}
