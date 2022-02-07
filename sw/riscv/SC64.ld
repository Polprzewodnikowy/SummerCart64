MEMORY {
    rom (rx)    : org = 0x00010000, len = 26k
    ram (rwx)   : org = 0x10000000, len = 16k
}

ENTRY(reset_handler)

SECTIONS {
    .text.reset_handler : {
        *(.text.reset_handler)
    } > rom

    .text : {
        _sitext = LOADADDR(.text);
        . = ALIGN(4);
        _stext = .;
        *(.text .text.* .gnu.linkonce.t.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(4);
        _etext = .;
    } > ram AT > rom

    .data : {
        _sidata = LOADADDR(.data);
        . = ALIGN(4);
        _sdata = .;
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(4);
        _ssdata = .;
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata .srodata.*)
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(4);
        _edata = .;
    } > ram AT > rom

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > ram

    __global_pointer$ = MIN(_ssdata + 0x800, MAX(_sdata + 0x800, _ebss - 0x800));
    __stack_pointer$ = ORIGIN(ram) + LENGTH(ram);
}
