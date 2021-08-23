MEMORY
{
    rom (rx)    : org = 0x10000000, len = 128
}

ENTRY(reset_handler)

SECTIONS
{
    .rom :
    {
        . = ALIGN(4);
        *(.text .text* .rodata .rodata* .srodata .srodata*)
        . = ALIGN(4);
    } > rom AT > rom
}
