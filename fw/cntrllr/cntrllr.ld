MEMORY
{
    ram (rwx)   : org = 0x00000000, len = 24k
}

__stack_pointer = ORIGIN(ram) + LENGTH(ram) - 16;

ENTRY(reset_handler)

SECTIONS
{
    .memory :
    {
        . = ALIGN(4);
        *(.reset);
        . = ALIGN(4);
        *(.text .text* .rodata .rodata* .srodata .srodata*);
        . = ALIGN(4);
        *(.bss .bss.* .sbss .sbss.*);
        . = ALIGN(4);
        *(.data .data.* .sdata .stada.*);
    } > ram
}
