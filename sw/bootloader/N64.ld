MEMORY {
    exceptions (rx)     : org = 0x80000000, len = 0x400
    ram (rwx)           : org = 0x80000400, len = 1M - 0x400
    framebuffer (rw)    : org = 0x80100000, len = 600k
}

ENTRY(entry_handler)

__stack_size = 16k;
__exception_stack_size = 8k;

SECTIONS {
    .exceptions : {
        KEEP(*(.text.exception_vector));
        . = ALIGN(8);
    } > exceptions

    .text : {
        *(.text.entry_handler)
        . = ALIGN(8);
        *(.text .text.* .gnu.linkonce.t.*)
        . = ALIGN(8);
    } > ram

    .assets : {
        *(.assets .assets.*)
        . = ALIGN(8);
    } > ram

    .rodata : {
        *(.rdata .rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(8);
    } > ram

    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
    } > ram

    .sdata : {
        _gp = . + 0x8000;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(8);
    } > ram

    .lit8 : {
        *(.lit8)
        . = ALIGN(8);
    } > ram

    .lit4 : {
        *(.lit4)
        . = ALIGN(8);
    } > ram

    .sbss : {
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        . = ALIGN(8);
    } > ram

    .bss : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(8);
    } > ram

    _sheap = .;
    . = ORIGIN(ram) + LENGTH(ram) - __stack_size - __exception_stack_size;
    _eheap = .;

    . += __stack_size;
    _sp = .;

    . += __exception_stack_size;
    _esp = .;

    .framebuffer (NOLOAD) : SUBALIGN(64) {
        *(.framebuffer .framebuffer.*)
    } > framebuffer

    /DISCARD/ : {
        *(.MIPS.*)
    }
}
