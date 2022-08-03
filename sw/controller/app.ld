MEMORY {
    loader (rx) : org = 0x08000000, len = 4k
    rom (rx)    : org = 0x08001000, len = 28k
    ram (rwx)   : org = 0x20000000, len = 8k
}

ENTRY(Reset_Handler)

SECTIONS {
    .loader : {
        . = ALIGN(4);
        KEEP(*(.loader))
        . = ALIGN(4);
    } > loader

    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > rom

    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*) 
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
        _etext = .;
    } > rom

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > ram

    .data : {
        _sidata = LOADADDR(.data);
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > ram AT > rom

    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(8);
    } > rom

    _estack = ORIGIN(ram) + LENGTH(ram);
}
