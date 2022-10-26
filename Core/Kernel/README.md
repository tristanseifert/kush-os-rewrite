# kush-os Kernel
This directory contains the kernel of the system.

## Building
We assume you've already built a proper native toolchain for kush, and you must use one of the toolchain CMake config files as specified. It's built automatically as part of the core system build (unless `BUILD_KERNEL` is disabled) but you will need to set some kernel-specific configuration options:

- KERNEL_PLATFORM_OPTIONS: Name of the architecture-specific (as defined by the toolchain file, e.g. x86_64, aarch64, etc.) platform backend to build into the kernel
