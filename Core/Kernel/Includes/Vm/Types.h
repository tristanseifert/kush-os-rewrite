#ifndef KERNEL_VM_TYPES_H
#define KERNEL_VM_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include <Bitflags.h>

namespace Kernel::Vm {
/**
 * @brief Virtual memory access mode
 *
 * You can OR most bits in this enumeration together to combine protection modes for a
 * particular page.
 */
enum class Mode: uintptr_t {
    /// No access is permitted
    None                                = 0,

    /// Kernel can read from this region
    KernelRead                          = (1 << 0),
    /// Kernel may write to this region
    KernelWrite                         = (1 << 1),
    /// Kernel can execute code out of this region
    KernelExec                          = (1 << 2),
    /// Kernel may read and write
    KernelRW                            = (KernelRead | KernelWrite),

    /// Userspace can read from this region
    UserRead                            = (1 << 8),
    /// Userspace may write to this region
    UserWrite                           = (1 << 9),
    /// Userspace may execute code out of this region
    UserExec                            = (1 << 10),
    /// Userspace may read and write
    UserRW                              = (UserRead | UserWrite),
    /// Mask for all user bits (any set = the mapping is user accessible)
    UserMask                            = (UserRead | UserWrite | UserExec),

    /// Mask indicating the read bits for kernel/userspace
    Read                                = (KernelRead | UserRead),
    /// Mask indicating the write bits for kernel/userspace
    Write                               = (KernelWrite | UserWrite),
    /// Mask indicating the exec bits for kernel/userspace
    Execute                             = (KernelExec | UserExec),
};
ENUM_FLAGS_EX(Mode, uintptr_t);

/**
 * @brief Page fault access type
 *
 * Defines the kind of access that led to a page fault.
 *
 * @remark Only the present, read/write, user/supervisor flags are guaranteed to be available in
 *         all situations.
 */
enum class FaultAccessType: uintptr_t {
    /// Mask for operation type
    MaskType                            = (0x03 << 0),
    /// A read access triggered the fault
    Read                                = (0x01 << 0),
    /// A write access triggered the fault
    Write                               = (0x02 << 0),

    /// Mask for access privilege
    MaskPrivilege                       = (0x03 << 4),
    /// Supervisor access caused the fault
    Supervisor                          = (0x01 << 4),
    /// User access caused the fault
    User                                = (0x02 << 4),

    /// Mask for the fault source
    MaskSource                          = (0xff << 8),
    /// Page is not present
    PageNotPresent                      = (1 << 8),
    /// Protection mode violation
    ProtectionViolation                 = (1 << 9),
    /// Invalid page table entry
    InvalidPTE                          = (1 << 10),
    /// Fault encountered during code access
    InstructionFetch                    = (1 << 11),
};
ENUM_FLAGS_EX(FaultAccessType, uintptr_t);

/**
 * @brief Hints for a TLB invalidation operation
 *
 * Hints may be combined (via bitwise OR) to affect the behavior of a TLB invalidation. They
 * specify which TLBs to invalidate, as well as the reason a TLB invalidate is being requested.
 *
 * Such hints can be used to optimize the underlying TLB flushes with more specific
 * instructions, if the platform supports it.
 */
enum class TlbInvalidateHint: uintptr_t {
    /**
     * @brief Bit mask for invalidation type
     */
    MaskInvalidate                      = (0b11111111 << 0),
    /**
     * @brief Invalidate local TLB
     */
    InvalidateLocal                     = (1 << 0),
    /**
     * @brief Invalidate remote TLBs
     *
     * Performs a TLB shootdown on _all_ remote processors that the map is currently mapped by.
     */
    InvalidateRemote                    = (1 << 1),
    /**
     * @brief Invalidate all TLBs
     *
     * TLBs in local and remote processors, as well as any additional caches (which may be defined
     * in the future) are cleared.
     */
    InvalidateAll                       = (InvalidateLocal | InvalidateRemote),

    /**
     * @brief Bit mask for change type
     *
     * Change types indicate what changed about the region that's to be invalidated.
     */
    MaskType                            = (0b11111111 << 8),
    /**
     * @brief Region was unmapped
     *
     * The address range covered by this request was unmapped.
     */
    Unmapped                            = (1 << 8),
    /**
     * @brief Region was re-mapped
     *
     * The physical address of one or more pages in the specified range has been updated.
     */
    Remapped                            = (1 << 9),
    /**
     * @brief Protection changed (tightened)
     *
     * Page protection settings for one or more page were tightened: e.g. going from a less
     * restrictive mode to a more restrictive mode, such as RW -> RO.
     */
    ProtectionTightened                 = (1 << 10),
    /**
     * @brief Protection changed (loosened)
     *
     * Page protection settings for one or more pages were loosened: e.g. going from a more
     * restrictive mode to a less restrictive mode, such as RO -> RW.
     */
    ProtectionLoosened                  = (1 << 11),
    /**
     * @brief Executable state changed
     *
     * One or more pages in the specified range had their execute bit changed.
     *
     * @remarks
     * This is ignored if the underlying processor doesn't implement NX support.
     */
    ExecuteChanged                      = (1 << 12),
    /**
     * @brief Permission changed
     *
     * The supervisor/user flag of one or more pages has been changed.
     */
    PermissionChanged                   = (1 << 13),
};

ENUM_FLAGS_EX(TlbInvalidateHint, uintptr_t);
}

#endif
