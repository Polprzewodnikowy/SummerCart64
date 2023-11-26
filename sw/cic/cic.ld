OUTPUT_ARCH("riscv")
OUTPUT_FORMAT("elf32-littleriscv")

MEMORY {
    ram (rwx) : org = 0x80000000, len = 2k
}

ENTRY(entry_handler)

SECTIONS {
    .text : {
        *(.text.entry_handler)
        *(.text .text.* .gnu.linkonce.t.*)
        . = ALIGN(4);
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.rodata1)
        . = ALIGN(4);
    } > ram : text

    .data : {
        . = ALIGN(4);
        *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
        *(.data1)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(4);
    } > ram : data

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.dynsbss)
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)
        *(.scommon)
        *(.dynbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > ram : bss

    _sp = ORIGIN(ram) + LENGTH(ram);
}

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    bss PT_LOAD FLAGS(6);
}
