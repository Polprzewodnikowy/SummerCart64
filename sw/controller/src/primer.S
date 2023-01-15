.syntax unified
.cpu cortex-m0plus
.fpu softvfp
.thumb


.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    .global Reset_Handler
    ldr r0, =_estack
    msr msp, r0

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
    ldr r0, =_header
    bl hw_set_vector_table
    bl primer

loop:
    b loop


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
