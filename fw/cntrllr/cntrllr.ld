MEMORY
{
    ram (rwx)       : org = 0x00000000, len = 16k
}

__ram_size = LENGTH(ram);
__stack_pointer = ORIGIN(ram) + LENGTH(ram) - 16;

ENTRY(reset_handler)

SECTIONS
{
    .ram :
    {
        . = ALIGN(4);
        *(.text.startup);
        . = ALIGN(4);
        *(.text .text* .rodata .rodata* .srodata .srodata*);
        . = ALIGN(4);
        *(.bss .bss.* .sbss .sbss.*);
        . = ALIGN(4);
        *(.data .data.* .sdata .stada.*);
        . = ALIGN(4);
    } > ram AT > ram
}
