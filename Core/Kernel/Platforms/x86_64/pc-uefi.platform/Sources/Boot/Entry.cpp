/**
 * Entry point, called from the bootloader.
 *
 * At this point, we have the following guarantees about the environment:
 *
 * - Stack is properly configured
 * - Virtual address mapped as requested in ELF program headers
 * - All segments are 64-bit disabled.
 * - GDT is loaded with bootloader-provided GDT.
 * - No IDT is specified.
 * - NX bit enabled, paging enabled, A20 gate opened
 * - All PIC and IOAPIC IRQs masked
 * - UEFI boot services exited
 */
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include <Init.h>
#include <Memory/PhysicalAllocator.h>
#include <Logging/Console.h>
#include <Vm/Manager.h>
#include <Vm/Map.h>
#include <Vm/ContiguousPhysRegion.h>

#include <new>

#include "Helpers.h"
#include "Arch/Gdt.h"
#include "Arch/Idt.h"
#include "Arch/Processor.h"
#include "Memory/PhysicalMap.h"
#include "Io/Console.h"
#include "Util/Backtrace.h"
#include "Vm/KernelMemoryMap.h"

using namespace Platform::Amd64Uefi;

static void InitPhysAllocator();
static Kernel::Vm::Map *InitKernelVm();
static void PopulateKernelVm(Kernel::Vm::Map *);
static void MapKernelSections(Kernel::Vm::Map *);
static void MapKernelSection(Kernel::Vm::Map *, const uintptr_t, const uintptr_t, const size_t,
        const Kernel::Vm::Mode);

extern "C" char __kernel_text_size, __kernel_rodata_size, __kernel_data_size;

/**
 * @brief Control backtrace symbolication
 *
 * XXX: This is currently broken (after VM remapping) for some reason or another
 */
static constexpr const bool kEnableSymbolication{false};

/**
 * @brief Whether all memory ranges are logged
 *
 * Useful for debugging; when enabled, the bootloader-provided memory map is dumped.
 */
static constexpr const bool kLogMemMap{false};

/**
 * @brief Whether kernel section initialization is logged
 */
static constexpr const bool kLogSections{true};

/**
 * @brief Base address for framebuffer
 *
 * Virtual memory base address (in platform-specific space) for the framebuffer.
 */
static constexpr const uintptr_t kFramebufferBase{0xffff'e800'0000'0000};

/**
 * Minimum size of physical memory regions to consider for allocation
 *
 * In some cases, the bootloader may provide a very fragmented memory map to the kernel, in which
 * many small chunks are carved out. Since each physical region comes with some fixed overhead, it
 * does not make sense to add these to the allocator and we just ignore that memory.
 *
 * That is to say, all usable memory regions smaller than this constant are wasted.
 */
constexpr static const size_t kMinPhysicalRegionSize{0x10000};

/**
 * First address for general purpose physical allocation
 *
 * Reserve all memory below this boundary, and do not add it to the general purpose allocator pool;
 * this is used so we can set aside the low 16M of system memory for legacy ISA DMA.
 */
constexpr static const uintptr_t kPhysAllocationBound{0x1000000};

/**
 * VM object corresponding to the kernel image.
 *
 * This is set when the kernel image is mapped into virtual address space, and can be used later to
 * map it into other address spaces or access it.
 */
static Kernel::Vm::MapEntry *gKernelImageVm{nullptr};

/**
 * Entry point from the bootloader.
 */
extern "C" void _osentry() {
    // set up the console (bootloader terminal, serial, etc.) and kernel console
    Console::Init();
    Kernel::Console::Init();

    Backtrace::Init();

    // initialize processor data structures
    Processor::VerifyFeatures();
    Processor::InitFeatures();

    Gdt::Init();
    Idt::InitBsp();

    // initialize the physical allocator, then the initial kernel VM map
    InitPhysAllocator();

    auto map = InitKernelVm();
    PopulateKernelVm(map);

    // prepare a few internal components
    Console::PrepareForVm(map);

    // then activate the map
    map->activate();
    Memory::PhysicalMap::FinishedEarlyBoot();

    // sets up kernel framebuffer, if any
    Console::VmEnabled();

    if(gKernelImageVm) {
        auto ptr = reinterpret_cast<const void *>(KernelAddressLayout::KernelImageStart);
        Backtrace::ParseKernelElf(ptr, gKernelImageVm->getLength());
    }

    // TODO: stash away bookkeeping info for CPUs, so we can launch them later
    auto cpuInfo = LimineRequests::gSmp.response;
    if(cpuInfo) {
        Kernel::Logging::Console::Notice("Total CPUs: %llu", cpuInfo->cpu_count);
    } else {
        Kernel::Logging::Console::Warning("no SMP info provided (forcing uniprocessor mode!)");
    }

    // jump to the kernel's entry point now
    Kernel::Start(map);
    // we should never get here…
    PANIC("Kernel entry point returned!");
}

/**
 * @brief Initialize the physical memory allocator.
 *
 * This initializes the kernel's physical allocator, with our base and extended page sizes. For
 * amd64, we only support 4K and 2M pages, so those are the two page sizes.
 *
 * Once the allocator is initialized, go through each of the memory regions provided by the
 * bootloader that are marked as usable. These are guaranteed to at least be 4K aligned which is
 * required by the physical allocator.
 *
 * @param info Information structure provided by the bootloader
 */
static void InitPhysAllocator() {
    // initialize kernel physical allocator
    static const size_t kExtraPageSizes[]{
        0x200000,
    };
    Kernel::PhysicalAllocator::Init(0x1000, kExtraPageSizes, 1);

    // locate physical memory map and validate it
    auto map = LimineRequests::gMemMap.response;
    REQUIRE(map, "Missing loader info struct %s", "phys mem map");
    REQUIRE(map->entry_count, "Invalid loader info struct %s", "phys mem map");
    REQUIRE(map->entries, "Invalid loader info struct %s", "phys mem map");

    // add each usable region to the physical allocator
    for(size_t i = 0; i < map->entry_count; i++) {
        const auto entry = map->entries[i];

        if(kLogMemMap) {
            Kernel::Logging::Console::Trace("%02u: %016llx - %016llx %010llx %u", i, entry->base,
                    entry->base + entry->length, entry->length, entry->type);
        }

        // ignore non-usable memory
        // XXX: is there reclaimable memory or other stuff?
        if(entry->type != LIMINE_MEMMAP_USABLE) continue;
        // ignore if it's too small
        else if(entry->length < kMinPhysicalRegionSize) continue;
        // if this entire region is below the cutoff, ignore it
        else if((entry->base + entry->length) < kPhysAllocationBound) continue;

        // adjust the base/length (if needed)
        uintptr_t base = entry->base;
        size_t length = entry->length;

        if(base < kPhysAllocationBound) {
            const auto diff = (kPhysAllocationBound - base);
            base += diff;
            length -= diff;
            // TODO: mark the set aside region for legacy use
        }

        // add it to the physical allocator
        Kernel::PhysicalAllocator::AddRegion(base, length);
    }

    Kernel::Logging::Console::Notice("Available memory: %zu K",
            Kernel::PhysicalAllocator::GetTotalPages() * 4);
}

/**
 * @brief Set up kernel VMM and allocate the kernel's virtual memory map.
 *
 * First, this initializes the kernel virtual memory manager.
 *
 * Then, it creates the first virtual memory map, in reserved storage space in the .data segment of
 * the kernel. It is then registered with the kernel VMM for later use.
 */
static Kernel::Vm::Map *InitKernelVm() {
    // set up VMM
    Kernel::Vm::Manager::Init();

    // create the kernel map
    static KUSH_ALIGNED(64) uint8_t gKernelMapBuf[sizeof(Kernel::Vm::Map)];
    auto ptr = reinterpret_cast<Kernel::Vm::Map *>(&gKernelMapBuf);

    new(ptr) Kernel::Vm::Map();

    return ptr;
}

/**
 * @brief Populate the kernel virtual memory map
 *
 * Fill in the kernel's virtual memory map with the sections for the kernel executable, as well as
 * the physical map aperture which is used to access physical pages when building page tables.
 *
 * @param info Information structure provided by the bootloader
 */
static void PopulateKernelVm(Kernel::Vm::Map *map) {
    int err;

    // map the kernel executable sections (.text, .rodata, .data/.bss) and then the full image
    MapKernelSections(map);

    auto kernelFileRes = LimineRequests::gKernelFile.response;
    if(kEnableSymbolication && kernelFileRes && kernelFileRes->kernel_file) {
        auto file = kernelFileRes->kernel_file;

        // get phys size and round up size
        const uintptr_t phys = reinterpret_cast<uintptr_t>(file->address);
        const auto pages = (file->size + (PageTable::PageSize() - 1)) / PageTable::PageSize();
        const auto bytes = pages * PageTable::PageSize();

        REQUIRE(bytes <
                (KernelAddressLayout::KernelImageEnd - KernelAddressLayout::KernelImageStart),
                "Kernel image too large for reserved address region");

        // create and map the VM object
        static KUSH_ALIGNED(64) uint8_t gVmObjectBuf[sizeof(Kernel::Vm::ContiguousPhysRegion)];
        auto vm = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gVmObjectBuf);
        new (vm) Kernel::Vm::ContiguousPhysRegion(phys, bytes, Kernel::Vm::Mode::KernelRead);

        err = map->add(KernelAddressLayout::KernelImageStart, vm);
        REQUIRE(!err, "failed to map %s: %d", "kernel image", err);

        gKernelImageVm = vm;
    } else {
        Kernel::Logging::Console::Warning("failed to get kernel file struct!");
        Backtrace::ParseKernelElf(nullptr, 0);
    }

    // get fb info
    auto fbResponse = LimineRequests::gFramebuffer.response;
    if(!fbResponse || !fbResponse->framebuffer_count) {
        Kernel::Logging::Console::Warning("UEFI provided no framebuffers!");
        return;
    }

    auto fbInfo = fbResponse->framebuffers[0];
    REQUIRE(fbInfo, "failed to get framebuffer info");

    if(fbResponse->framebuffer_count > 1) {
        Kernel::Logging::Console::Warning("got %u framebuffers; using first one!",
                fbResponse->framebuffer_count);
    }

    // framebuffer size, rounded up to page size
    size_t fbLength = PageTable::NearestPageSize(fbInfo->height * fbInfo->pitch);

    // TODO: this is a GIGANTIC hack, lol
    const uintptr_t fbPhysBase = reinterpret_cast<uintptr_t>(fbInfo->address) & 0xffffffff;

    Kernel::Console::Notice("Framebuffer: %016llx %zu bytes", fbPhysBase, fbLength);

    // map framebuffer (if specified by loader)
    static KUSH_ALIGNED(64) uint8_t gFbVmBuf[sizeof(Kernel::Vm::ContiguousPhysRegion)];
    Kernel::Vm::ContiguousPhysRegion *framebuffer{nullptr};

    framebuffer = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gFbVmBuf);
    new(framebuffer) Kernel::Vm::ContiguousPhysRegion(fbPhysBase, fbLength,
            Kernel::Vm::Mode::KernelRW);

    err = map->add(kFramebufferBase, framebuffer);
    REQUIRE(!err, "failed to map %s: %d", "framebuffer", err);

    // initialize it in the console boi
    if(framebuffer) {
        Console::SetFramebuffer(fbInfo, framebuffer, reinterpret_cast<void *>(kFramebufferBase));
    }

    // last, remap the physical allocator structures
    Kernel::PhysicalAllocator::RemapTo(map);
}

/**
 * @brief Create VM objects for all of the kernel's segments.
 *
 * This will create VM objects for the virtual memory segments (based off the program headers, as
 * loaded by the bootloader) for the kernel. This roughly corresponds to the RX/R/RW regions that
 * hold .text, .rodata, and .data/.bss respectively.
 */
static void MapKernelSections(Kernel::Vm::Map *map) {
    size_t roundedSize;

    // get the physical and virtual base of the kernel image
    uint64_t kernelPhysBase{0}, kernelVirtBase{0xffffffff80000000};

    auto base = LimineRequests::gKernelAddress.response;
    if(base) {
        kernelPhysBase = base->physical_base;
        kernelVirtBase = base->virtual_base;
    }

    REQUIRE(!!kernelPhysBase, "failed to get kernel %s base", "phys");
    REQUIRE(!!kernelVirtBase, "failed to get kernel %s base", "virt");

    Kernel::Logging::Console::Trace("Kernel: phys=%p, virt=%p", kernelPhysBase, kernelVirtBase);

    /*
     * This here has the potential to be rather… flaky and fragile, because we make assumptions on
     * how the sections are laid out inside the ELF. These assumptions _should_ hold true always
     * with our linker script, though.
     */
    struct SectionInfo {
        const char *name;
        const size_t size;
        const Kernel::Vm::Mode mode;
    };

    constexpr static const size_t kNumSections{3};
    static const SectionInfo kSectionInfo[kNumSections]{
        {
            .name                       = ".text",
            .size                       = reinterpret_cast<size_t>(&__kernel_text_size),
            .mode                       = Kernel::Vm::Mode::KernelExec,
        },
        {
            .name                       = ".rodata",
            .size                       = reinterpret_cast<size_t>(&__kernel_rodata_size),
            .mode                       = Kernel::Vm::Mode::KernelRead,
        },
        {
            .name                       = ".data",
            .size                       = reinterpret_cast<size_t>(&__kernel_data_size),
            .mode                       = Kernel::Vm::Mode::KernelRW,
        },
    };

    for(size_t i = 0; i < kNumSections; i++) {
        const auto &info = kSectionInfo[i];

        roundedSize = PageTable::NearestPageSize(info.size);
        if(kLogSections) {
            Kernel::Logging::Console::Trace("%8s: phys=%016llx, virt=%016llx %06llx %08lx",
                    info.name, kernelPhysBase, kernelVirtBase, roundedSize,
                    static_cast<size_t>(info.mode));
        }

        MapKernelSection(map, kernelPhysBase, kernelVirtBase, roundedSize, info.mode);

        kernelPhysBase += roundedSize;
        kernelVirtBase += roundedSize;
    }
}

/**
 * @brief Create VM object for a single kernel section
 *
 * @param map VM map to add the object to
 * @param physBase Physical base address of this section
 * @param virtBase Virtual base address of this section
 * @param length Length of the section, in bytes (must be page aligned)
 * @param mode Protection mode for the section
 */
static void MapKernelSection(Kernel::Vm::Map *map, const uintptr_t physBase,
        const uintptr_t virtBase, const size_t length, const Kernel::Vm::Mode mode) {
    int err;

    // static storage for the associated VM objects
    constexpr static const size_t kMaxSections{4};
    static KUSH_ALIGNED(64) uint8_t gVmObjectAllocBuf[kMaxSections]
        [sizeof(Kernel::Vm::ContiguousPhysRegion)];
    static size_t gVmObjectAllocNextFree{0};

    // create VM object
    const auto vmIdx = gVmObjectAllocNextFree;
    REQUIRE(++gVmObjectAllocNextFree < kMaxSections, "exceeded max kernel sections");

    auto vm = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gVmObjectAllocBuf[vmIdx]);
    new (vm) Kernel::Vm::ContiguousPhysRegion(physBase, length, mode);

    err = map->add(virtBase, vm);
    REQUIRE(!err, "failed to map kernel section (virt %016zx phys %016zx len %zx mode %02zx): %d",
            virtBase, physBase, length, static_cast<size_t>(mode), err);
}
