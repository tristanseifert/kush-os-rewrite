# kush
This is yet another hobby OS, but this one doesn't try to be yet another UNIX clone. In fact, I really have no idea what it's trying to be. It just kind of… is.

**⚠️ Important Note ⚠️**: This branch is a work-in-progress rewrite of the [original version 1](https://github.com/tristanseifert/kush-os/tree/old) codebase. It (obviously, because some of the original design decisions were dumb as hell) breaks compatibility.

## Building
We use CMake to build everything. The single `CMakeLists.txt` in this directory can build the kernel, all userspace components, and tools automatically. You will have to select one of the toolchains to use and pass it to CMake in the `CMAKE_TOOLCHAIN_FILE` flag.

Most likely, you're best served by setting `CMAKE_BUILD_TYPE` to `RelwithDebInfo` to produce optimized, release binaries but with debugging information. Especially a debug kernel will cause significant performance losses.

Try starting with something like this:

```
cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-<arch>-clang.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_VERBOSE_MAKEFILE=NO -DKERNEL_PLATFORM_OPTION=<platform>
cmake --build build -j32
```

### Directory structure
- Core: Basic system components
    - Kernel: Code for the kush-os kernel
- tools: Host side utilities
- sysroot: Base directory of the OS root directory. Automatically built up as kernel/userspace is built.
- toolchain: Created by the `build_toolchain.sh` script.

### Build Dependencies
Build system is made with cmake, and tested to target the Ninja build system. You'll need also the cross compiled LLVM toolchain, and a host toolchain if the host side tools are required. Each of the platforms may have additional build requirements:

- x86, amd64: The [nasm](https://nasm.us) assembler is required.

## LLVM Toolchain
You can use the `build_toolchain.sh` script to build a toolchain to use for compiling the system. It will pull a forked version of llvm (currently, at version 13) that can target kush. If you'd rather build it yourself, [check out the forked llvm repo.](https://github.com/tristanseifert/kush-os-llvm/tree/dev/tseifert/kush-os)

## Documentation
All code should be adequately commented, with Doxygen-style annotations to enable the automatic generation of API documentation.

Design documentation is [available here.](https://wiki.trist.network/books/kush)

Additionally, some blog posts describing the original OS design and motivations are [available here.](https://blraaz.me/tags/kush-os/) These may or may not be correct with regard to this new, rewritten codebase, but are retained here for posterity.
