#define STR(x)  #x
#define XSTR(s) STR(s)
#define VERSION XSTR(__SC64_VERSION)

.section .text.bootcode
header_pi_config:
    .word 0x80371240

header_clock_rate:
    .word 0x0000000F

header_load_addr:
    .word entry_handler

header_release_addr:
    .word 0x00000000

header_crc:
    .fill 2, 4, 0

    .org 0x18, 0x00

header_text_info:
    .ascii "byMFinPL"
    .ascii "SummerLoader64  "
    .ascii VERSION

    .org 0x40, 0x20

ipl3:
    .incbin "blob/ipl3.bin"


.section .text.entry_handler
.set noreorder
entry_handler:
    .global entry_handler

    li $a0, 0x08
    sw $a0, 0xBFC007FC

    la $gp, _gp
    la $sp, _sp

init_bss:
    la $a0, _sbss
    la $a1, _ebss
    bge $a0, $a1, 2f
    nop
1:
    sd $zero, 0($a0)
    addi $a0, $a0, 8
    blt $a0, $a1, 1b
    nop
2:

init_bss_cache:
    la $a0, _sbss
    la $a1, _ebss
    bge $a0, $a1, 2f
    nop
1:
    cache 0x15, 0($a0)
    addi $a0, $a0, 16
    blt $a0, $a1, 1b
    nop
2:

run:
    j main
    nop
