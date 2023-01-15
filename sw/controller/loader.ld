MEMORY {
    code (rx)   : org = 0x08000000, len = 4k
    ram (rwx)   : org = 0x20000000, len = 8k
}

ENTRY(Reset_Handler)

SECTIONS {
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > code

    .startup : {
        . = ALIGN(4);
        *(.text.Reset_Handler)
        . = ALIGN(4);
    } > code

    .text : {
        _sitext = LOADADDR(.text);
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text*) 
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
        _etext = .;
    } > ram AT > code

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
        _sirodata = LOADADDR(.rodata);
        . = ALIGN(4);
        _srodata = .;
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _erodata = .;
    } > ram AT > code

    _app_header = ORIGIN(code) + LENGTH(code);
    _app_magic = _app_header + 16;
    _estack = ORIGIN(ram) + LENGTH(ram);
}
