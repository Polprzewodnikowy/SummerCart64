MEMORY {
    RAM (rwx)       : org = 0x00000000, len = 16k
    ROM (rx)        : org = 0x10000000, len = 128
}

__stack_pointer = ORIGIN(RAM) + LENGTH(RAM) - 16;

ENTRY(reset_handler)

SECTIONS {
    .text : {
        PROVIDE(__app_entry_point = .);
        *(.text.app_handler)
        *(.text.unlikely .text.unlikely.*)
        *(.text.startup .text.startup.*)
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    } > RAM

    .rodata : {
        *(.rdata)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(8);
        *(.srodata.cst16)
        *(.srodata.cst8)
        *(.srodata.cst4)
        *(.srodata.cst2)
        *(.srodata .srodata.*)
    } > RAM

    .data : {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer = . + 0x800);
        *(.sdata .sdata.* .sdata2.*)
        *(.gnu.linkonce.s.*)
    } > RAM

    .bss : ALIGN(8) {
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
    } > RAM

    .bootloader : {
        *(.text.reset_handler)
    } > ROM
}
