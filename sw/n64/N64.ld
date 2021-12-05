MEMORY {
    rdram (rwx) : org = 0x80300000, len = 1M
    flash (r)   : org = 0xB0000000, len = 90k
}

ENTRY(entry_handler)

__stack_size = 64k;

SECTIONS {
    .flash : {
        KEEP(*(.text.rom_header));
        KEEP(*(.text.ipl3));
        __ipl3_font = LOADADDR(.flash) + 0xB70;
    } > flash

    .text : {
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.data .data.* .gnu.linkonce.d.*)
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.lit8 .lit4)
        . = ALIGN(4);
    } > rdram AT > flash

    .bss : {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
    } > rdram

    _sheap = .;
    . = ORIGIN(rdram) + LENGTH(rdram) - __stack_size;
    _eheap = .;

    . += __stack_size;
    _sp = .;

    /DISCARD/ : {
        *(.MIPS.*)
    }
}
