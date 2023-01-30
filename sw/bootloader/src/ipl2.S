.set noat
.set noreorder


.section .text.ipl2, "ax", %progbits
.type ipl2, %object
ipl2:
    .global ipl2
    lui $t5, 0xBFC0
1:
    lw $t0, 0x7FC($t5)
    addiu $t5, $t5, 0x7C0
    andi $t0, $t0, 0x80
    bnel $t0, $zero, 1b
    lui $t5, 0xBFC0
    lw $t0, 0x24($t5)
    lui $t3, 0xB000
