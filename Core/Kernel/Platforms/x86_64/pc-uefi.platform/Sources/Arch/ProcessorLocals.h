#ifndef KERNEL_PLATFORM_UEFI_ARCH_PROCESSORLOCALS
#define KERNEL_PLATFORM_UEFI_ARCH_PROCESSORLOCALS

#include <stddef.h>
#include <stdint.h>

#include <Smp/CpuLocals.h>

namespace Platform::Amd64Uefi {
class Idt;

/**
 * @brief Processor local storage support
 *
 * Implements processor local storage by means of the %gs register.
 */
class ProcessorLocals {
    private:
        /**
         * @brief Per processor information structure
         */
        struct Info {
            /**
             * @brief Self pointer
             *
             * Holds the memory address of the processor info structure; this works around a quirk
             * in the x86 ISA that doesn't allow us to easily read out the base of %gs later.
             *
             * @note This must be the first field in the struct!
             */
            Info *selfPtr{this};

            /// Pointer to our IDT
            Idt *idt{nullptr};

            /// Kernel-specific information (generic)
            Kernel::Smp::CpuLocals kernel;
        } KUSH_ALIGNED(64);

    public:
        static void InitBsp();

        /**
         * @brief Get platform processor locals
         */
        static inline Info *Get() {
            Info *info;
            asm volatile("movq %%gs:0, %0" : "=r"(info) :: "memory");
            //asm volatile("movq %%gs:%1, %0" : "=r"(info) : "n"(offsetof(Info, selfPtr)));
            return info;
        }

        /**
         * @brief Get kernel processor local data
         *
         * Return a pointer to the kernel's processor local data structure.
         */
        static inline Kernel::Smp::CpuLocals *GetKernelData() {
            return &(Get()->kernel);
        }

    private:
        static void Set(Info *);
};
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Platform {
using ProcessorLocals = Platform::Amd64Uefi::ProcessorLocals;
}
#endif

#endif
