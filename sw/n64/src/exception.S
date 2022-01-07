#include "exception.h"


.section .text.exception_handler
exception_handler:
    .set noat
    la $k0, (_esp - SAVE_REGISTERS_SIZE)
    sd $at, AT_OFFSET($k0)
    sd $v0, V0_OFFSET($k0)
    sd $v1, V1_OFFSET($k0)
    sd $a0, A0_OFFSET($k0)
    sd $a1, A1_OFFSET($k0)
    sd $a2, A2_OFFSET($k0)
    sd $a3, A3_OFFSET($k0)
    sd $t0, T0_OFFSET($k0)
    sd $t1, T1_OFFSET($k0)
    sd $t2, T2_OFFSET($k0)
    sd $t3, T3_OFFSET($k0)
    sd $t4, T4_OFFSET($k0)
    sd $t5, T5_OFFSET($k0)
    sd $t6, T6_OFFSET($k0)
    sd $t7, T7_OFFSET($k0)
    sd $s0, S0_OFFSET($k0)
    sd $s1, S1_OFFSET($k0)
    sd $s2, S2_OFFSET($k0)
    sd $s3, S3_OFFSET($k0)
    sd $s4, S4_OFFSET($k0)
    sd $s5, S5_OFFSET($k0)
    sd $s6, S6_OFFSET($k0)
    sd $s7, S7_OFFSET($k0)
    sd $t8, T8_OFFSET($k0)
    sd $t9, T9_OFFSET($k0)
    sd $gp, GP_OFFSET($k0)
    sd $sp, SP_OFFSET($k0)
    sd $fp, FP_OFFSET($k0)
    sd $ra, RA_OFFSET($k0)
    .set at

    move $sp, $k0

exception_is_fatal:
    mfc0 $a0, C0_CAUSE
    sw $a0, C0_CAUSE_OFFSET($k0)
    move $a1, $a0
    andi $a1, EXCEPTION_CODE_MASK
    srl $a1, $a1, EXCEPTION_CODE_BIT
    beq $a1, $zero, exception_interrupt

exception_fatal:
    sd $k0, K0_OFFSET($k0)
    sd $k1, K1_OFFSET($k0)
    mfc0 $t0, C0_STATUS
    sw $t0, C0_STATUS_OFFSET($k0)
    dmfc0 $t0, C0_EPC
    sd $t0, C0_EPC_OFFSET($k0)
    addiu $t0, 4
    dmtc0 $t0, C0_EPC
    move $a0, $k0
    la $t1, exception_fatal_handler
    jalr $t1
    j exception_restore

exception_interrupt:
    andi $a0, INTERRUPT_PENDING_MASK
    srl $a0, $a0, INTERRUPT_PENDING_BIT
    la $t1, exception_interrupt_handler
    jalr $t1

exception_restore:
    .set noat
    ld $at, AT_OFFSET($k0)
    ld $v0, V0_OFFSET($k0)
    ld $v1, V1_OFFSET($k0)
    ld $a0, A0_OFFSET($k0)
    ld $a1, A1_OFFSET($k0)
    ld $a2, A2_OFFSET($k0)
    ld $a3, A3_OFFSET($k0)
    ld $t0, T0_OFFSET($k0)
    ld $t1, T1_OFFSET($k0)
    ld $t2, T2_OFFSET($k0)
    ld $t3, T3_OFFSET($k0)
    ld $t4, T4_OFFSET($k0)
    ld $t5, T5_OFFSET($k0)
    ld $t6, T6_OFFSET($k0)
    ld $t7, T7_OFFSET($k0)
    ld $s0, S0_OFFSET($k0)
    ld $s1, S1_OFFSET($k0)
    ld $s2, S2_OFFSET($k0)
    ld $s3, S3_OFFSET($k0)
    ld $s4, S4_OFFSET($k0)
    ld $s5, S5_OFFSET($k0)
    ld $s6, S6_OFFSET($k0)
    ld $s7, S7_OFFSET($k0)
    ld $t8, T8_OFFSET($k0)
    ld $t9, T9_OFFSET($k0)
    ld $gp, GP_OFFSET($k0)
    ld $sp, SP_OFFSET($k0)
    ld $fp, FP_OFFSET($k0)
    ld $ra, RA_OFFSET($k0)
    .set at

    eret


.section .text.exception_vector
exception_vector:
    .set noreorder
    la $k0, exception_handler
    jalr $k1, $k0
    nop
    .equ exception_vector_size, (. - exception_vector)
    .set reorder


.section .text.exception_install
exception_install:
    .global exception_install
    la $t0, exception_vector
    li $t1, VECTOR_LOCATION
    li $t2, (VECTOR_SIZE * VECTOR_NUM)
    add $t2, $t2, $t1
1:
    move $t3, $t0
    move $t4, $t1
    li $t5, exception_vector_size
    add $t5, $t5, $t4
2:
    lw $t6, 0($t3)
    sw $t6, 0($t4)
    cache HIT_INVALIDATE_I, 0($t4)
    addiu $t3, 4
    addiu $t4, 4
    bne $t4, $t5, 2b
    addiu $t1, VECTOR_SIZE
    bne $t1, $t2, 1b
    jr $ra
