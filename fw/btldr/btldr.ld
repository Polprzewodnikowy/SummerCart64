MEMORY
{
    ram (rwx)   : org = 0x00000000, len = 24k
    rom (rx)    : org = 0x10000000, len = 1k
}

__stack_pointer = ORIGIN(ram) + LENGTH(ram) - 16;

ENTRY(reset_handler);

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        *(.text .text* .rodata .rodata* .srodata .srodata*)
        . = ALIGN(4);
    } > rom AT > rom

    .data :
    {
        . = ALIGN(4);
        __data_init_start = LOADADDR(.data);
        __data_start = .;
        *(.data .data.* .sdata .stada.*);
        . = ALIGN(4);
        __data_end = .;
    } > ram AT > rom

    .bss :
    {
		. = ALIGN(4);
		__bss_start = .;
		*(.bss .bss.* .sbss .sbss.*)
		. = ALIGN(4);
		*(COMMON);
		. = ALIGN(4);
		__bss_end = .;
    } > ram AT > ram
}
