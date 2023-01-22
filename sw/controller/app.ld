MEMORY {
    loader (rx) : org = 0x08000000, len = 4k
    code (rx)   : org = 0x08001000, len = 28k
    ram (rwx)   : org = 0x20000000, len = 8k
}

ENTRY(Reset_Handler)

__bin_align = 8;

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
    } > code

    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*) 
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
        _etext = .;
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
        _sidata = LOADADDR(.data);
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > ram AT > code

    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        FILL(0xFF);
        . = ALIGN(__bin_align);
    } > code

    _estack = ORIGIN(ram) + LENGTH(ram);
}
