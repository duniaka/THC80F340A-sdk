.syntax unified
.thumb
.cpu cortex-m0

.section .isr_vector, "a"
.word 0x20002800
.word Reset_Handler + 1
.word Fault + 1
.word Fault + 1
.word 0, 0, 0, 0, 0, 0, 0
.word Fault + 1
.word 0, 0
.word Fault + 1
.word Fault + 1
.word Fault + 1

.section .text.micro
.thumb_func
.global Reset_Handler
Reset_Handler:
    ldr r0, =micro_reset
    bx  r0

.thumb_func
Fault:
    b Fault
