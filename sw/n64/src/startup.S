.section .text.rom_header
header_pi_config:
    .word 0x80371240

header_clock_rate:
    .word 0x0000000F

header_load_addr:
    .word entry_handler

header_sdk_version:
    .word 0x00000000

header_crc:
    .fill 2, 4, 0

    .org 0x20, 0x00
header_text_info:
    .global header_text_info
    .ascii "n64boot SummerCart64"
    .org 0x40, 0x00


.section .text.ipl3
ipl3:
    .incbin "header", 0x40


.section .text.entry_handler
entry_handler:
    .global entry_handler

    la $gp, _gp
    la $sp, _sp

    la $t0, init
    jalr $t0

    la $t0, main
    jalr $t0

loop:
    j loop
