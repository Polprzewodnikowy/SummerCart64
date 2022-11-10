.syntax unified
.cpu cortex-m0plus
.fpu softvfp
.thumb

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    .global Reset_Handler
    cpsid i

init_text:
    ldr r0, =_stext
    ldr r1, =_etext
    ldr r2, =_sitext
    bl copy_section

init_rodata:
    ldr r0, =_srodata
    ldr r1, =_erodata
    ldr r2, =_sirodata
    bl copy_section

init_data:
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    bl copy_section

init_bss:
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b 2f
1:
    str r3, [r2]
    adds r2, r2, #4
2:
    cmp r2, r4
    bcc 1b

run:
    bl loader

boot:
    ldr r0, =_app_magic
    ldr r0, [r0]
    ldr r1, =0x34364353
    cmp r0, r1
    bne empty_image

    ldr r0, =_app_header
    push {r0}
    bl set_vector_table_offset
    pop {r0}
    ldr r1, [r0, #0]
    msr MSP, r1
    ldr r1, [r0, #4]
    blx r1

empty_image:
    bl no_valid_image

loop:
    b loop

copy_section:
    movs r3, #0
    b 2f
1:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
2:
    adds r4, r0, r3
    cmp r4, r1
    bcc 1b
    bx lr


.section .text.Default_Handler, "ax", %progbits
Default_Handler:
    .global Default_Handler
    b Default_Handler


.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .global g_pfnVectors
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler

    .weak      NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak      HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
