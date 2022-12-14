###################################################################################################
# Kernel CMake list
#
# Defines the Kernel target, and provides options to define which platform we will build for.
###################################################################################################

##### Kernel build options; can be modified by platform code
set(KERNEL_COMPILE_OPTS -static -ffreestanding -nodefaultlibs -nostdlib -fno-exceptions -fno-rtti
    -nostdlib)

##### Provide a selection of platforms for the current architecture
file(GLOB KERNEL_PLATFORM_DIRS "${CMAKE_CURRENT_LIST_DIR}/Platforms/${CMAKE_SYSTEM_PROCESSOR}/*.platform"
    INCLUDE_DIRECTORIES)
cmake_path(GET KERNEL_PLATFORM_DIRS STEM KERNEL_PLATFORM_OPTIONS)

list(GET KERNEL_PLATFORM_OPTIONS 0 KERNEL_PLATFORM_FIRST)

set(KERNEL_PLATFORM ${KERNEL_PLATFORM_FIRST} CACHE STRING "Kernel platform")
set_property(CACHE KERNEL_PLATFORM PROPERTY STRINGS ${KERNEL_PLATFORM_OPTIONS})

##### Generate a version file
configure_file(${CMAKE_CURRENT_LIST_DIR}/Sources/BuildInfo.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/BuildInfo.cpp)
set(BuildInfoFile "${CMAKE_CURRENT_BINARY_DIR}/BuildInfo.cpp")

##### Define the kernel target
add_executable(Kernel
    Sources/Init.cpp
    Sources/Logging/Console.cpp
    Sources/Runtime/CppSupport.cpp
    Sources/Runtime/StackGuard.cpp
    Sources/Runtime/String.cpp
    Sources/Exceptions/Handler.cpp
    Sources/Memory/PhysicalAllocator.cpp
    Sources/Memory/Pool.cpp
    Sources/Memory/Region.cpp
    Sources/Vm/Manager.cpp
    Sources/Vm/Map.cpp
    Sources/Vm/MapEntry.cpp
    Sources/Vm/ContiguousPhysRegion.cpp
    Sources/Vm/PageAllocator.cpp
    ${BuildInfoFile}
)

target_include_directories(Kernel PRIVATE Sources)
target_include_directories(Kernel PRIVATE Includes)
target_include_directories(Kernel PRIVATE ${KUSH_TOOLCHAIN_HEADERS})

target_compile_options(Kernel PRIVATE ${KERNEL_COMPILE_OPTS} ${ARCH_COMPILE_OPTS} -flto)
target_link_options(Kernel PRIVATE ${KERNEL_COMPILE_OPTS} --static -nostartfiles -flto)

##### Include selected platform code
set(KERNEL_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(platforms/${CMAKE_SYSTEM_PROCESSOR}/${KERNEL_PLATFORM_OPTIONS}.platform)

# Link the platform code
target_link_libraries(Kernel PRIVATE ${KERNEL_PLATFORM_LIB})
target_compile_options(Kernel PRIVATE ${KERNEL_PLATFORM_COMPILE_OPTS})
target_link_options(Kernel PRIVATE ${KERNEL_PLATFORM_COMPILE_OPTS})

# Update kernel filename
set_target_properties(Kernel PROPERTIES OUTPUT_NAME "kernel-${CMAKE_SYSTEM_PROCESSOR}-${KERNEL_PLATFORM}")
set_target_properties(Kernel PROPERTIES SUFFIX "${KERNEL_PLATFORM_FILE_EXTENSION}")

# Use the appropriate linker script
set_target_properties(Kernel PROPERTIES LINK_DEPENDS ${KERNEL_PLATFORM_LINKER_SCRIPT})
target_link_options(Kernel PRIVATE ${KERNEL_PLATFORM_LINKER_OPTS})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_options(Kernel PRIVATE "-Wl,-T${KERNEL_PLATFORM_LINKER_SCRIPT}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_options(Kernel PRIVATE "-T${KERNEL_PLATFORM_LINKER_SCRIPT}")
endif()

##### Include various external libraries
# printf: without floating point support
add_library(printf OBJECT
    External/printf/src/printf/printf.c
)

target_include_directories(printf PUBLIC External/printf/src)
target_compile_options(printf PRIVATE ${KERNEL_COMPILE_OPTS} ${KERNEL_PLATFORM_COMPILE_OPTS} -flto)

# disable floating point, writeback specifier (security issue)
target_compile_definitions(printf PRIVATE -DPRINTF_ALIAS_STANDARD_FUNCTION_NAMES=0
    -DPRINTF_SUPPORT_DECIMAL_SPECIFIERS=0 -DPRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS=0
    -DPRINTF_SUPPORT_WRITEBACK_SPECIFIER=0)

target_link_libraries(Kernel PRIVATE printf)

##### Specify install location of kernel

##### Kernel documentation

# create the doxyfile for core docs and create the target
set(DOXYGEN_KERNEL_IN ${CMAKE_CURRENT_LIST_DIR}/Doxyfile.in)
set(DOXYGEN_KERNEL_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
configure_file(${DOXYGEN_KERNEL_IN} ${DOXYGEN_KERNEL_OUT} @ONLY)

add_custom_target(KernelDocs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_KERNEL_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating kernel documentation"
    VERBATIM
)
add_dependencies(docs KernelDocs)
