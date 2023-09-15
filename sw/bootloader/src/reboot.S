#define IPL3_ENTRY          0xA4000040
#define REBOOT_ADDRESS      0xA4001000
#define STACK_ADDRESS       0xA4001FF0

#define RI_ADDRESS          0xA4700000

#define RI_MODE             0x00
#define RI_CONFIG           0x04
#define RI_CURRENT_LOAD     0x08
#define RI_SELECT           0x0C
#define RI_REFRESH          0x10
#define RI_LATENCY          0x14

#define RI_MODE_RESET       0x00000000
#define RI_MODE_STANDBY     0x0000000E

#define RDRAM_RESET_DELAY   1024
#define RDRAM_STANDBY_DELAY 512

.set noat
.section .text.reboot, "ax", %progbits
.type reboot, %object
reboot_start:
    .global reboot_start

# NOTE: CIC x105 requirement
ipl2:
.set noreorder
    lui $t5, 0xBFC0
1:
    lw $t0, 0x7FC($t5)
    addiu $t5, $t5, 0x7C0
    andi $t0, $t0, 0x80
    bnel $t0, $zero, 1b
    lui $t5, 0xBFC0
    lw $t0, 0x24($t5)
    lui $t3, 0xB000
.set reorder

reboot_entry:
    .set reboot, REBOOT_ADDRESS + (. - reboot_start)
    .global reboot

    li $sp, STACK_ADDRESS

    bnez $s5, reset_rdram_skip

reset_rdram:
    li $t0, RI_ADDRESS

    li $t1, RI_MODE_RESET
    sw $t1, RI_MODE($t0)

    li $t2, RDRAM_RESET_DELAY
1:
    addiu $t2, (-1)
    bnez $t2, 1b

    sw $zero, RI_CONFIG($t0)
    sw $zero, RI_CURRENT_LOAD($t0)
    sw $zero, RI_SELECT($t0)
    sw $zero, RI_REFRESH($t0)

    li $t1, RI_MODE_STANDBY
    sw $t1, RI_MODE($t0)

    li $t2, RDRAM_STANDBY_DELAY
1:
    addiu $t2, (-1)
    bnez $t2, 1b
reset_rdram_skip:

prepare_registers:
    la $t0, ra_table
    sll $t1, $s4, 2
    add $t0, $t1
    lw $ra, ($t0)

    move $at, $zero
    move $v0, $zero
    move $v1, $zero
    move $a0, $zero
    move $a1, $zero
    move $a2, $zero
    move $a3, $zero
    move $t0, $zero
    move $t1, $zero
    move $t2, $zero
    move $t4, $zero
    move $t5, $zero
    move $t6, $zero
    move $t7, $zero
    move $s0, $zero
    move $s1, $zero
    move $s2, $zero
    move $t8, $zero
    move $t9, $zero
    move $k0, $zero
    move $k1, $zero
    move $gp, $zero
    move $fp, $zero

run_ipl3:
    li $t3, IPL3_ENTRY
    jr $t3

ra_values:
    .set ra_table, REBOOT_ADDRESS + (. - reboot_start)
    .word 0xA4001554
    .word 0xA4001550
    .word 0xA4001554

    .set reboot_size, (. - reboot_start)
    .global reboot_size
