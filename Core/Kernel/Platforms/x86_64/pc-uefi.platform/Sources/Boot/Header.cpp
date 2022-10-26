/**
 * Define the stivale2 information structure.
 *
 * It is placed in a dedicated section, which the linker script will keep.
 */
#include <stdint.h>
#include <stddef.h>

#include <limine.h>

#include <Intrinsics.h>

#include "Boot/Helpers.h"

using namespace Platform::Amd64Uefi;

/**
 * Size of the initialization stack
 */
constexpr static const size_t kBootStackSize{8 * 1024};

// Ignore warnings from unused tags.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

limine_stack_size_request LimineRequests::gStackSize{
    .id         = LIMINE_STACK_SIZE_REQUEST,
    .revision   = 0,
    .response   = nullptr,
    .stack_size = kBootStackSize,
};
limine_terminal_request LimineRequests::gTerminal{
    .id         = LIMINE_TERMINAL_REQUEST,
    .revision   = 0,
    .response   = nullptr,
    .callback   = nullptr,
};

limine_framebuffer_request LimineRequests::gFramebuffer{
    .id         = LIMINE_FRAMEBUFFER_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};

limine_memmap_request LimineRequests::gMemMap{
    .id         = LIMINE_MEMMAP_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};
// TODO: higher half direct map?

limine_kernel_address_request LimineRequests::gKernelAddress{
    .id         = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};

limine_efi_system_table_request LimineRequests::gEfiSystemTable{
    .id         = LIMINE_EFI_SYSTEM_TABLE_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};

limine_bootloader_info_request LimineRequests::gLoaderInfo{
    .id         = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};

limine_boot_time_request LimineRequests::gBootTime{
    .id         = LIMINE_BOOT_TIME_REQUEST,
    .revision   = 0,
    .response   = nullptr,
};

/**
 * Slide higher half: Have the bootloader apply a slide to the base address of the kernel, with
 * a 2MB slide alignment.
 */
/*
static struct stivale2_header_tag_slide_hhdm gSlideTag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_SLIDE_HHDM_ID,
        .next = reinterpret_cast<uintptr_t>(&gUnmapNullTag),
    },
    // reserved
    .flags = 0,
    // alignment of the slide
    .alignment = 0x200000,
};
*/

/**
 * Framebuffer tag: request that the bootloader places the system's graphics hardware into a
 * graphical mode, rather than text mode.
 */
/*
static struct stivale2_header_tag_framebuffer gFramebufferTag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next = reinterpret_cast<uintptr_t>(&gTerminalTag),
    },
    // the bootloader shall pick the best resolution/bpp
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0
};
*/

#pragma GCC diagnostic pop

/**
 * This is the main bootloader information block. This has to live in its own section so that the
 * bootloader can read it.
 */
__attribute__((section(".limine_reqs"), used))
static void *gLimineHeaders[] = {
    // required
    &LimineRequests::gStackSize,
    &LimineRequests::gTerminal,
    &LimineRequests::gFramebuffer,
    &LimineRequests::gKernelAddress,
    &LimineRequests::gMemMap,
    &LimineRequests::gEfiSystemTable,
    // optional
    &LimineRequests::gLoaderInfo,
    &LimineRequests::gBootTime,
    // must be NULL terminated
    nullptr,
};

    // use ELF entry point
    // .entry_point = 0,
    /*
     * Specify the bottom of the stack. This is only used for the boot processor, and even then,
     * only until the scheduler is started.
     */
    // .stack = reinterpret_cast<uintptr_t>(gBspStack) + sizeof(gBspStack),
