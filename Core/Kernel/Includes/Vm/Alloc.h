/**
 * @file
 *
 * @brief Virtual memory allocators
 */
#ifndef KERNEL_VM_ALLOC_H
#define KERNEL_VM_ALLOC_H

#include <stddef.h>
#include <stdint.h>

namespace Kernel::Vm {
/// @{ @name Page Allocator
[[nodiscard]] void *VAlloc(const size_t length);
void VFree(void *ptr, const size_t length);
/// @}
}

#endif
