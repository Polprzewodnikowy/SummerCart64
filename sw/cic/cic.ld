MEMORY {
    ram (rwx) : org = 0x80000000, len = 2k
}

ENTRY(entry_handler)

SECTIONS {
    .text : {
        *(.text.entry_handler)
        . = ALIGN(4);
        *(.text .text.* .gnu.linkonce.t.*)
        . = ALIGN(4);
    } > ram

    .rodata : {
        *(.rdata .rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(4);
    } > ram

    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(4);
    } > ram

    .sdata : {
        _gp = . + 0x800;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(4);
    } > ram

    .sbss : {
        _sbss = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon .scommon.*)
        . = ALIGN(4);
    } > ram

    .bss : {
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > ram

    _sp = ORIGIN(ram) + LENGTH(ram);
}
