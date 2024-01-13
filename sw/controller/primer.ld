MEMORY {
    ram (rwx)   : org = 0x20000000, len = 4k
    code (rwx)  : org = 0x20001000, len = 4k
}

ENTRY(Reset_Handler)

SECTIONS {
    .isr_vector : {
        . = ALIGN(4);
        _header = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > code

    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
    } > code

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > ram

    .data : {
        . = ALIGN(4);
        *(.data)
        *(.data*)
        . = ALIGN(4);
    } > code

    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > code

    _estack = ORIGIN(ram) + LENGTH(ram);
}
