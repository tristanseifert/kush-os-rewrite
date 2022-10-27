#ifndef KERNEL_SMP_CPULOCALS_H
#define KERNEL_SMP_CPULOCALS_H

namespace Kernel::Vm {
class Map;
}

namespace Kernel::Smp {
/**
 * @brief Kernel per processor data structure
 *
 * Each CPU core has precisely one of these structs allocated, which will hold any per processor
 * data structures needed.
 */
struct CpuLocals {
    /**
     * @brief Currently active memory map
     */
    Vm::Map *map{nullptr};
};
}

#endif
