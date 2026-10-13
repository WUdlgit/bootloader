.syntax unified
.cpu cortex-m4
.thumb

.section .text.app_handoff, "ax", %progbits
.global app_handoff
.type app_handoff, %function
.thumb_func

app_handoff:
    msr msp, r0
    bx  r1

.size app_handoff, .-app_handoff