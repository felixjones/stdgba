@===============================================================================
@
@ Assert handler entry bridge for ARM7TDMI (GBA)
@
@ Provides:
@   __assert_func - capture CPU/video/IRQ state and call C++ renderer
@
@ Must run in ARM state to read CPSR directly.
@
@===============================================================================

    .section .text._stdgba_assert, "ax", %progbits
    .arm
    .align 2

@ =============================================================================
@ __assert_func
@
@ Input:  r0-r3 = file, line, func, expr (newlib assert ABI)
@ Output: none (tail-calls renderer; does not return)
@ Clobbers: r4-r12, sp
@ =============================================================================
    .global __assert_func
    .type __assert_func, %function
__assert_func:
    @ r0-r3 contain parameters (file, line, func, expr)
    @ Capture sp, lr, cpsr into r4-r6
    mov     r4, sp
    mov     r5, lr
    mrs     r6, cpsr

    @ r7 = IO base (0x04000000), used throughout
    mov     r7, #0x04000000

    @ Read interrupt registers before disabling IME
    @ REG_IE = 0x4000200, REG_IF = 0x4000202, REG_IME = 0x4000208
    add     r8, r7, #0x200
    ldrh    r10, [r8, #8]           @ Save previous IME
    ldrh    r11, [r8]               @ Save IE
    ldrh    r12, [r8, #2]           @ Save IF

    @ Disable IME
    strb    r8, [r8, #8]            @ Disable IME (stores 0x00)

    @ Read DISPCNT (0x4000000)
    ldrh    r9, [r7]                @ Save previous DISPCNT

    @ Set DISPCNT to Mode 3 + BG2 (0x0403)
    mov     r8, #0x0400
    orr     r8, r8, #0x03
    strh    r8, [r7]

    @ Move stack to top of IWRAM (0x3008000)
    mov     sp, #0x03000000
    add     sp, sp, #0x8000

    @ Build assert_state on stack using stmdb (push multiple)
    @ struct: file(0), line(4), func(8), expr(12), sp(16), lr(20), cpsr(24),
    @         dispcnt(28), ime(30), ie(32), if_(34)
    @ Pack dispcnt+ime and ie+if into words
    orr     r9, r9, r10, lsl #16    @ r9 = (ime << 16) | dispcnt
    orr     r11, r11, r12, lsl #16  @ r11 = (if << 16) | ie
    stmdb   sp!, {r11}              @ Push ie+if
    stmdb   sp!, {r0-r6, r9}        @ Push file,line,func,expr,sp,lr,cpsr,dispcnt+ime

    @ r0 = pointer to assert_state (sp)
    mov     r0, sp

    @ Call thumb renderer (use bx for ARM7TDMI compatibility, function is noreturn)
    ldr     r1, =_stdgba_assert_render
    bx      r1


    .ltorg
    .size __assert_func, . - __assert_func
