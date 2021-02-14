/*
 * Error codes for system calls
 */
#ifndef KERNEL_SYS_ERRORS_H
#define KERNEL_SYS_ERRORS_H

namespace sys {
enum Errors: int {
    /// System call succeeded (not an error)
    Success                             = 0,
    /// Unspecified error
    GeneralError                        = -1,
    /// Invalid memory address/range provided
    InvalidPointer                      = -2,
    /// The provided handle was invalid
    InvalidHandle                       = -3,
    /// A provided argument was invalid
    InvalidArgument                     = -4,
    /// The syscall requested is unavailable
    InvalidSyscall                      = -5,
};
}

#endif