#include "vr4300.h"


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
    .ascii "SC64 bootloader"
    .org 0x40, 0x00


.section .text.ipl3
ipl3:
    .incbin "header", 0x40


.section .text.entry_handler
entry_handler:
    .global entry_handler

    la $gp, _gp
    la $sp, _sp

bss_init:
	la $a0, _sbss
	la $a1, _ebss
1:
	sd $zero, 0($a0)
    cache HIT_WRITE_BACK_D, 0($a0)
	addiu $a0, 8
	bltu $a0, $a1, 1b

    la $t0, init
    jalr $t0

    la $t0, main
    jalr $t0

loop:
    j loop
