MEMORY {
    framebuffer (rw)    : org = 0x8026A000, len = 600k
    ram (rwx)           : org = 0x80300000, len = 1M
    rom (r)             : org = 0xB0000000, len = 1028k
}

ENTRY(entry_handler)

__exception_stack_size = 8k;
__stack_size = 16k;

SECTIONS {
    .boot : {
        KEEP(*(.text.rom_header));
        KEEP(*(.text.ipl3));
    } > rom

    .framebuffer (NOLOAD) : SUBALIGN(64) {
        *(.framebuffer .framebuffer.*)
    } > framebuffer

    .text : SUBALIGN(8) {
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.assets .assets.*)
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        *(.lit8 .lit4)
    } > ram AT > rom

    .bss : {
        . = ALIGN(8);
        _sbss = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
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
