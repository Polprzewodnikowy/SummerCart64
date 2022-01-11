#define VECTOR_LOCATION         (0xA0000000UL)
#define VECTOR_SIZE             (0x80)
#define VECTOR_NUM              (4)

#define HIT_INVALIDATE_I        ((4 << 2) | 0)

#define TICKS_PER_SECOND        (93750000UL / 2)
#define WATCHDOG_TIMEOUT        (10)

#define C0_COUNT                $9
#define C0_COMPARE              $11
#define C0_STATUS               $12
#define C0_CAUSE                $13
#define C0_EPC                  $14

#define INTERRUPT_ENABLE        (1 << 0)
#define INTERRUPT_MASK_TIMER    (1 << 15)

#define EXCEPTION_CODE_MASK     (0x007C)
#define EXCEPTION_CODE_BIT      (2)
#define INTERRUPT_PENDING_MASK  (0xFF00)
#define INTERRUPT_PENDING_BIT   (8)
#define INTERRUPT_PENDING_TIMER (1 << 7)

#define AT_OFFSET               (8)
#define V0_OFFSET               (16)
#define V1_OFFSET               (24)
#define A0_OFFSET               (32)
#define A1_OFFSET               (40)
#define A2_OFFSET               (48)
#define A3_OFFSET               (56)
#define T0_OFFSET               (64)
#define T1_OFFSET               (72)
#define T2_OFFSET               (80)
#define T3_OFFSET               (88)
#define T4_OFFSET               (96)
#define T5_OFFSET               (104)
#define T6_OFFSET               (112)
#define T7_OFFSET               (120)
#define S0_OFFSET               (128)
#define S1_OFFSET               (136)
#define S2_OFFSET               (144)
#define S3_OFFSET               (152)
#define S4_OFFSET               (160)
#define S5_OFFSET               (168)
#define S6_OFFSET               (176)
#define S7_OFFSET               (184)
#define T8_OFFSET               (192)
#define T9_OFFSET               (200)
#define K0_OFFSET               (208)
#define K1_OFFSET               (216)
#define GP_OFFSET               (224)
#define SP_OFFSET               (232)
#define FP_OFFSET               (240)
#define RA_OFFSET               (248)
#define C0_STATUS_OFFSET        (256)
#define C0_CAUSE_OFFSET         (260)
#define C0_EPC_OFFSET           (264)
#define SAVE_REGISTERS_SIZE     (272)


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
    andi $a0, EXCEPTION_CODE_MASK
    srl $a0, $a0, EXCEPTION_CODE_BIT
    andi $a1, INTERRUPT_PENDING_MASK
    srl $a1, $a1, INTERRUPT_PENDING_BIT
    move $t0, $a1
    andi $t0, INTERRUPT_PENDING_TIMER
    bne $t0, $zero, exception_fatal
    beq $a0, $zero, exception_interrupt

exception_fatal:
    sd $k0, K0_OFFSET($k0)
    sd $k1, K1_OFFSET($k0)
    mfc0 $t0, C0_STATUS
    sw $t0, C0_STATUS_OFFSET($k0)
    dmfc0 $t0, C0_EPC
    sd $t0, C0_EPC_OFFSET($k0)
    addiu $t0, 4
    dmtc0 $t0, C0_EPC
    move $a2, $k0
    la $t1, exception_fatal_handler
    jalr $t1
    j exception_restore

exception_interrupt:
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
    li $t7, (WATCHDOG_TIMEOUT * TICKS_PER_SECOND)
    mtc0 $zero, C0_COUNT
    mtc0 $t7, C0_COMPARE
    mfc0 $t7, C0_STATUS
    ori $t7, (INTERRUPT_MASK_TIMER | INTERRUPT_ENABLE)
    mtc0 $t7, C0_STATUS
    jr $ra
