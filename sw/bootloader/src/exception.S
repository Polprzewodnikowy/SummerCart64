#include "vr4300.h"


#define ZR_OFFSET                   (0)
#define AT_OFFSET                   (8)
#define V0_OFFSET                   (16)
#define V1_OFFSET                   (24)
#define A0_OFFSET                   (32)
#define A1_OFFSET                   (40)
#define A2_OFFSET                   (48)
#define A3_OFFSET                   (56)
#define T0_OFFSET                   (64)
#define T1_OFFSET                   (72)
#define T2_OFFSET                   (80)
#define T3_OFFSET                   (88)
#define T4_OFFSET                   (96)
#define T5_OFFSET                   (104)
#define T6_OFFSET                   (112)
#define T7_OFFSET                   (120)
#define S0_OFFSET                   (128)
#define S1_OFFSET                   (136)
#define S2_OFFSET                   (144)
#define S3_OFFSET                   (152)
#define S4_OFFSET                   (160)
#define S5_OFFSET                   (168)
#define S6_OFFSET                   (176)
#define S7_OFFSET                   (184)
#define T8_OFFSET                   (192)
#define T9_OFFSET                   (200)
#define K0_OFFSET                   (208)
#define K1_OFFSET                   (216)
#define GP_OFFSET                   (224)
#define SP_OFFSET                   (232)
#define S8_OFFSET                   (240)
#define RA_OFFSET                   (248)
#define HI_OFFSET                   (256)
#define LO_OFFSET                   (264)
#define C0_EPC_OFFSET               (272)
#define C0_BADVADDR_OFFSET          (280)
#define C0_STATUS_OFFSET            (288)
#define C0_CAUSE_OFFSET             (292)
#define SAVE_REGISTERS_SIZE         (296)


.section .text.exception_vector
exception_tlb_miss:
    .org 0x0000
    j exception_handler

exception_xtlb_miss:
    .org 0x0080
    j exception_handler

exception_ecc:
    .org 0x0100
    j exception_handler

exception_other:
    .org 0x0180
    j exception_handler


.section .text.exception_handler
.type exception_handler, %function
exception_handler:
    .set noat
    la $k0, (_esp - SAVE_REGISTERS_SIZE)
    sd $zero, ZR_OFFSET($k0)
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
    sd $s8, S8_OFFSET($k0)
    sd $ra, RA_OFFSET($k0)
    mfhi $t0
    mflo $t1
    sd $t0, HI_OFFSET($k0)
    sd $t1, LO_OFFSET($k0)
    .set at

    move $sp, $k0

exception_check_type:
    mfc0 $t0, C0_CAUSE
    sw $t0, C0_CAUSE_OFFSET($k0)
    andi $a0, $t0, C0_CR_EC_MASK
    srl $a0, C0_CR_EC_BIT
    beqz $a0, exception_interrupt

exception_fatal:
    sd $k0, K0_OFFSET($k0)
    sd $k1, K1_OFFSET($k0)
    mfc0 $t0, C0_STATUS
    sw $t0, C0_STATUS_OFFSET($k0)
    dmfc0 $t0, C0_EPC
    sd $t0, C0_EPC_OFFSET($k0)
    dmfc0 $t0, C0_BADVADDR
    sd $t0, C0_BADVADDR_OFFSET($k0)
    move $a1, $k0
    jal exception_fatal_handler
    ld $t0, C0_EPC_OFFSET($k0)
    dmtc0 $t0, C0_EPC
    j exception_restore

exception_interrupt:
    andi $a0, $t0, C0_CR_IP_MASK
    srl $a0, C0_CR_IP_BIT
    mfc0 $t0, C0_STATUS
    andi $t0, C0_SR_IM_MASK
    srl $t0, C0_SR_IM_BIT
    and $a0, $t0
    jal interrupts_handler

exception_restore:
    .set noat
    ld $t0, HI_OFFSET($k0)
    ld $t1, LO_OFFSET($k0)
    mthi $t0
    mtlo $t1
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
    ld $s8, S8_OFFSET($k0)
    ld $ra, RA_OFFSET($k0)
    .set at

    eret
