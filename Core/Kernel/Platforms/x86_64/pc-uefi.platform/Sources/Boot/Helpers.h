#ifndef KERNEL_PLATFORM_UEFI_BOOT_HELPERS
#define KERNEL_PLATFORM_UEFI_BOOT_HELPERS

#include <limine.h>

/**
 * \namespace Platform::Amd64Uefi
 * @brief %Platform support for Amd64-based UEFI systems
 *
 * Implements support for booting the kernel on UEFI-based Amd64 machines, using a
 * <a href="https://github.com/limine-bootloader/limine/blob/trunk/PROTOCOL.md">Limine</a>
 * compliant bootloader. Basic IO via the bootloader console or serial port is provided.
 */
namespace Platform::Amd64Uefi {
/**
 * @brief Storage for all Limine headers
 *
 * Use this dude to access any Limine headers that encompass requests that we make to the loader;
 * these are required to access the responses placed by the loader.
 */
struct LimineRequests {
    /**
     * @brief Stack size request (mandatory)
     */
    static limine_stack_size_request gStackSize;

    /**
     * @brief Text IO terminal (mandatory)
     */
    static limine_terminal_request gTerminal;

    /**
     * @brief Graphic IO framebuffer (mandatory)
     */
    static limine_framebuffer_request gFramebuffer;

    /**
     * @brief System physical memory map (mandatory)
     */
    static limine_memmap_request gMemMap;

    /**
     * @brief Kernel load address (mandatory)
     */
    static limine_kernel_address_request gKernelAddress;

    /**
     * @brief EFI system table (mandatory)
     */
    static limine_efi_system_table_request gEfiSystemTable;

    /**
     * @brief Bootloader information (optional)
     */
    static limine_bootloader_info_request gLoaderInfo;

    /**
     * @brief System time at boot-up (optional)
     */
    static limine_boot_time_request gBootTime;
};
}

#endif
