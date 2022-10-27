#ifndef KERNEL_PLATFORM_UEFI_VM_PAGETABLE_H
#define KERNEL_PLATFORM_UEFI_VM_PAGETABLE_H

#include <stddef.h>
#include <stdint.h>

#include <Bitflags.h>
#include <Vm/Types.h>
#include <Platform/Processor.h>

namespace Platform::Amd64Uefi {
/**
 * @brief Flags for a mapping on x86_64
 *
 * These bits correspond to bits in page table entries. These bits may not be valid for all types
 * of paging structures.
 */
enum class PageFlags: uint64_t {
    None                                = 0,

    /// Mapping present
    Present                             = (1 << 0),
    /// Write allowed
    Writable                            = (1 << 1),
    /// User-mode access allowed
    UserAccess                          = (1 << 2),

    PWT                                 = (1 << 3),
    PCD                                 = (1 << 4),
    PAT                                 = (1 << 7),

    /// Whether this region has been accessed
    Accessed                            = (1 << 5),
    /// Whether we've written to this region before
    Dirty                               = (1 << 6),

    /// Mapping is global
    Global                              = (1 << 8),

    /**
     * Mapping is not executable. Note that this bit will NOT be present, even if the mapping was
     * originally created with the no-execute flag, if the underlying hardware doesn't support
     * no-execute. (It's a reserved bit that must be zero otherwise.)
     */
    NoExecute                           = (1ULL << 63),

    /// Mask of all bits corresponding to flags in a page table entry
    FlagsMask = (Present | Writable | UserAccess | PWT | PCD | PAT | Accessed | Dirty | Global |
            NoExecute),
};
ENUM_FLAGS_EX(PageFlags, uint64_t);

/**
 * @brief A single PML4 and all its descendant page tables
 *
 * This class is used by the kernel VM manager to manipulate the underlying page tables.
 *
 * @remark Page table modifications made via this class do not automatically flush TLBs for any
 *         purpose other than ensuring that page table modifications (i.e. when a new PTE is set up
 *         and linked in) will use correct mappings.
 * @remark Therefore, it is the caller's responsibility to make sure the TLBs are managed.
 */
class PageTable {
    public:
        PageTable(PageTable *parent);
        ~PageTable();

        /**
         * @brief Load this page table into the processor's MMU.
         */
        void activate() {
            asm volatile("mov %0, %%cr3" :: "r"(this->pml4Phys) : "memory");
        }

        /**
         * @brief Get the system page size
         *
         * @return Page size, in bytes
         */
        constexpr static inline size_t PageSize() {
            return 4096;
        }

        /**
         * @brief Round up a size to the nearest page multiple
         */
        constexpr static inline size_t NearestPageSize(const size_t in) {
            if(in % PageSize()) {
                return in + (PageSize() - (in % PageSize()));
            } else {
                return in;
            }
        }

        [[nodiscard]] int mapPage(const uint64_t phys, const uintptr_t virt,
                const Kernel::Vm::Mode mode);
        [[nodiscard]] int unmapPage(const uintptr_t virt);

        [[nodiscard]] int getPhysAddr(const uintptr_t virt, uint64_t &outPhys,
                Kernel::Vm::Mode &outMode);
        [[nodiscard]] int invalidateTlb(const uintptr_t virt, const size_t length,
                const Kernel::Vm::TlbInvalidateHint hints);

        static void DecodePageFault(const ProcessorState &state,
                Kernel::Vm::FaultAccessType &outMode);

    private:
        void copyPml4Upper(PageTable *);
        void mapPhysAperture();

        [[nodiscard]] static uint64_t AllocPage();

        static uint64_t ReadTable(const uintptr_t tableBase, const size_t offset);
        static void WriteTable(const uintptr_t tableBase, const size_t offset, const uint64_t val);

        static uint64_t *GetTableVmAddr(const uint64_t base);

        static void DecodePTE(const uint64_t pte, uint64_t &outPhys, Kernel::Vm::Mode &outMode);

    private:
        /// Physical address of PML4
        uint64_t pml4Phys{0};

        /// Whether the no execute bit is used
        constexpr static const bool kNoExecuteEnabled{false};
        /// Whether page table additions are logged
        constexpr static const bool kLogMapAdd{false};
        /// Whether page table allocations are logged
        constexpr static const bool kLogAlloc{false};
};
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Platform {
using PageTable = Platform::Amd64Uefi::PageTable;
}
#endif

#endif
