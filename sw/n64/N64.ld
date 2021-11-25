MEMORY {
    rdram (rwx) : org = 0x80300000, len = 1M
    flash (r)   : org = 0xB0000000, len = 90k
}

ENTRY(entry_handler)

SECTIONS {
    .flash : {
        KEEP(*(.text.bootcode));
    } > flash

    .text : {
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.rdata .rodata .rodata.* .gnu.linkonce.r.*)
    } > rdram AT > flash

    .data : {
        . = ALIGN(8);
        _sdata = .;
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
        _ssdata = .;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.lit8)
        *(.lit4)
    } > rdram AT > flash

    .bss : {
        . = ALIGN(8);
        _sbss = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > rdram

    /DISCARD/ : {
        *(.MIPS.*)
    }

    _gp = MIN(_ssdata + 0x8000, MAX(_sdata + 0x8000, _ebss - 0x8000));
    _sp = ORIGIN(rdram) + LENGTH(rdram);
}
