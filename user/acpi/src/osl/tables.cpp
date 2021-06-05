/*
 * Implementations of the ACPICA OS layer to implement table overrides.
 *
 * None of these currently do anything.
 */
#include "osl.h"

#include <acpi.h>

/**
 * Override an object in ACPI namespace.
 */
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject,
        ACPI_STRING *NewValue) {
    *NewValue = NULL;
    return AE_OK;
}

/**
 * Overwrite an entire ACPI table.
 */
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
        ACPI_TABLE_HEADER **NewTable) {
    *NewTable = NULL;
    return AE_OK;
}

/**
 * Overwrite an ACPI table with a differing physical address.
 */
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
        ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength) {
    *NewAddress = 0;
    return AE_OK;
}

/**
 * Locates the ACPI table root pointer (RSDP).
 *
 * The way this works is platform specific:
 *
 * - x86: Use the built-in ACPI function to find the root pointer in the first 1M of memory.
 * - amd64: Receive root pointer from BOOTBOOT loader
 */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
#if defined(__i386__)
    ACPI_PHYSICAL_ADDRESS  Ret;
    Ret = 0;
    AcpiFindRootPointer(&Ret);
    return Ret;
#else
#error AcpiOsGetRootPointer unimplemented for current arch
#endif
}
