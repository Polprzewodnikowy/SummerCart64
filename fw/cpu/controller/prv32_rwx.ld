MEMORY
{
    ram (rwx)       : org = 0x00000000, len = 16k
    bootloader (rx) : org = 0x10000000, len = 128
    sdram (rwx)     : org = 0x80000000, len = 64M
}

__ram_size = LENGTH(ram);
__stack_pointer = ORIGIN(ram) + LENGTH(ram) - 4;

ENTRY(main)

SECTIONS
{
    .ram :
    {
        . = ALIGN(4);
        *(.text.main);
        . = ALIGN(4);
        *(.text .text* .rodata .rodata* .srodata .srodata*);
        . = ALIGN(4);
        *(.data .data.* .sdata .sdata.*);
        . = ALIGN(4);
        *(.bss .bss.* .sbss .sbss.*);
        . = ALIGN(4);
    } > ram AT > ram
}
