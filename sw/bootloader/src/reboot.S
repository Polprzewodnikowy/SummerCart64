#define IPL3_ENTRY          0xA4000040
#define REBOOT_ADDRESS      0xA4001000
#define STACK_ADDRESS       0xA4001FF0

#define RI_ADDRESS          0xA4700000

#define RI_SELECT           0x0C
#define RI_REFRESH          0x10


.set noat
.section .text.reboot, "ax", %progbits

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

reset_rdram:
    bnez $s5, reset_rdram_skip

    li $t0, RI_ADDRESS

    sw $zero, RI_REFRESH($t0)
    sw $zero, RI_SELECT($t0)
reset_rdram_skip:

detect_console_region:
    li $t0, 1
    beq $s4, $zero, pal_console
    beq $s4, $t0, ntsc_console
    b mpal_console

pal_console:
    li $ra, 0xA4001554
    b prepare_registers

ntsc_console:
    li $ra, 0xA4001550
    b prepare_registers

mpal_console:
    li $ra, 0xA4001554

prepare_registers:
    move $at, $zero
    move $v0, $zero
    move $v1, $zero
    move $a0, $zero
    move $a1, $zero
    move $a2, $zero
    move $a3, $zero
    move $t0, $zero
    move $t1, $zero
    li $t2, 0x40
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

    .set reboot_size, (. - reboot_start)
    .global reboot_size
