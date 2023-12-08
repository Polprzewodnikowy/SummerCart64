MEMORY {
    ram (rwx) : org = 0x80000000, len = 2M
}

ENTRY(entry_handler)

__exception_stack_size = 8k;
__stack_size = 16k;

SECTIONS {
    .text : SUBALIGN(8) {
        KEEP(*(.text.exception_vector));
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.assets .assets.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.lit8 .lit4)
    } > ram

    .bss : {
        . = ALIGN(8);
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(64);
        *(.framebuffer)
        . = ALIGN(8);
    } > ram

    _sheap = .;
    . = ORIGIN(ram) + LENGTH(ram) - __exception_stack_size - __stack_size;
    _eheap = .;

    . += __exception_stack_size;
    _esp = .;

    . += __stack_size;
    _sp = .;

    /DISCARD/ : {
        *(.MIPS.*)
    }
}
