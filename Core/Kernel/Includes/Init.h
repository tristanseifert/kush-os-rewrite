#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

namespace Kernel {
namespace Vm {
class Map;
}

/**
 * @brief Kernel entry point
 *
 * Jump here at the end of platform-specific initialization.
 */
void Start(Kernel::Vm::Map *map);
}

#endif
