ENTRY(_osentry)
OUTPUT_FORMAT(elf64-x86-64)

/**
 * Define the different protected segments
 */
PHDRS {
    null    PT_NULL    FLAGS(0) ;                   /* Null segment */
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS {
    /* base address for the kernel; it is slid within this 2GB region */
    . = 0xffffffff80000000;

    /* kernel code (immutable) section */
    .text : ALIGN(4K) {
        __kernel_text_start = .;
        *(.text .text.*)
    } :text
    __kernel_text_end = .;
    __kernel_text_size = SIZEOF(.text);

    /* Place the limine header into rodata */
    .limine_reqs : ALIGN(4K) {
        __kernel_rodata_start = .;
        KEEP(*(.limine_reqs *.limine_reqs*));
    } :rodata

    /* kernel read-only data (immutable) section */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    . = ALIGN(8);
    __kernel_rodata_size = . - __kernel_rodata_start;

    /* kernel data (mutable) section */
    .data : ALIGN(4K) {
        __kernel_data_start = .;
        *(*.data .data.*)
    } :data

    /* kernel data (uninitialized, mutable) and stack */
    .bss (NOLOAD) : ALIGN(64) {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    . = ALIGN(8);
    __kernel_data_size = . - __kernel_data_start;

    /* discard some stuff */
    /DISCARD/ : {
        /* exception handling */
        *(.eh_frame)
    }
}

