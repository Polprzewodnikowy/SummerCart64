#include "vr4300.h"


#define RSP_DMEM_ADDRESS    0xA4000000


.section .text.entry_handler, "ax", %progbits
.type entry_handler, %function
.global entry_handler
entry_handler:
    li $t0, (C0_SR_CU1 | C0_SR_CU0 | C0_SR_FR)
	mtc0 $t0, C0_STATUS

    la $gp, _gp
    la $sp, _sp

    li $t0, RSP_DMEM_ADDRESS    # IPL3 Boot flags location
    lbu $a0, 9($t0)             # TV type
    lbu $a1, 10($t0)            # Reset type
    lw $a2, 4($t0)              # Entropy

    jal init

    jal main

loop:
    j loop
