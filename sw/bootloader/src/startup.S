.section .text.entry_handler, "ax", %progbits

entry_handler:
    .type entry_handler, %function
    .global entry_handler

    la $gp, _gp
    la $sp, _sp

    lui $t0, 0xA400
    lbu $a0, 9($t0)     # TV type
    lbu $a1, 10($t0)    # Reset type
    lw $a2, 4($t0)      # Entropy

    la $t0, init
    jalr $t0

    la $t0, main
    jalr $t0

loop:
    j loop
