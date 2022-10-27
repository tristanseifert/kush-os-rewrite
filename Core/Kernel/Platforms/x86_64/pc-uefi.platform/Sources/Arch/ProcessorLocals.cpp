#include "Processor.h"
#include "ProcessorLocals.h"
#include "Idt.h"

#include <Logging/Console.h>

#include <new>

using namespace Platform::Amd64Uefi;

/**
 * @brief Initialize the processor locals for the bootstrap processor
 *
 * Allocate processor locals from static storage for the bootstrap processor.
 */
void ProcessorLocals::InitBsp() {
    // allocate the structure and initialize some fields
    static uint8_t gBuffer[sizeof(Info)] KUSH_ALIGNED(64);
    auto info = new (gBuffer) Info;

    info->idt = Idt::gBspIdt;

    // set it up in the calling processor's %gs
    Set(info);
}

/**
 * @brief Update MSRs for %gs base
 *
 * @param proc Base address of an `Info` structure
 */
void ProcessorLocals::Set(Info *proc) {
    const auto procAddr = reinterpret_cast<uintptr_t>(proc);
    Processor::WriteMsr(Processor::Msr::GSBase, procAddr, procAddr >> 32);
    Processor::WriteMsr(Processor::Msr::KernelGSBase, procAddr, procAddr >> 32);
}
