MEMORY {
    rdram (rwx) : org = 0x80300000, len = 1M
    flash (r)   : org = 0xB0000000, len = 1028k
}

ENTRY(entry_handler)

__exception_stack_size = 8k;
__stack_size = 16k;

SECTIONS {
    .flash : {
        KEEP(*(.text.rom_header));
        KEEP(*(.text.ipl3));
    } > flash

    .text : SUBALIGN(4) {
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.data .data.* .gnu.linkonce.d.*)
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.lit8 .lit4)
        . = ALIGN(8);
    } > rdram AT > flash

    .bss : {
        _sbss = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > rdram

    _sheap = .;
    . = ORIGIN(rdram) + LENGTH(rdram) - __exception_stack_size - __stack_size;
    _eheap = .;

    . += __exception_stack_size;
    _esp = .;

    . += __stack_size;
    _sp = .;

    /DISCARD/ : {
        *(.MIPS.*)
    }
}
