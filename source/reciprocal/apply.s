    .syntax unified
    .arch armv4t
    .arm

    .section .iwram.reciprocal_apply, "ax", %progbits
    .align 2
    .global _stdgba_reciprocal_apply
    .type _stdgba_reciprocal_apply, %function

@ r0: numerator bits, r1: multiplier, r2: packed apply config.
@ The internal shift range is [-1, 61]; direct tables use 0, 16 or 32.
_stdgba_reciprocal_apply:
    mov     r3, r0, asr #31
    tst     r2, #0x200
    moveq   r3, #0
    eor     r0, r0, r3
    sub     ip, r0, r3
    and     r3, r3, #0x100
    eor     r2, r2, r3
@ ARM7TDMI early termination depends on Rs: put the numerator there.
    umull   r0, r3, r1, ip
    mov     r1, r2, lsl #24
    mov     r1, r1, asr #24
    cmp     r1, #32
    blt     .Lsmall
    sub     r1, r1, #32
    mov     r0, r3, lsr r1
    b       .Lcheck_range
.Lsmall:
    cmp     r1, #0
    blt     .Lleft
    rsb     ip, r1, #32
    mov     r0, r0, lsr r1
    orr     r0, r0, r3, lsl ip
    mov     r3, r3, lsr r1
.Lfinish:
    cmp     r3, #0
    bne     _stdgba_reciprocal_range_error
.Lcheck_range:
    tst     r2, #0x200
    beq     .Lunsigned
    mov     ip, #0x80000000
    tst     r2, #0x100
    subeq   ip, ip, #1
    cmp     r0, ip
    bhi     _stdgba_reciprocal_range_error
    tst     r2, #0x100
    rsbne   r0, r0, #0
    bx      lr
.Lunsigned:
    tst     r2, #0x100
    bxeq    lr
    cmp     r0, #0
    bne     _stdgba_reciprocal_range_error
    bx      lr
.Lleft:
    adds    r0, r0, r0
    adc     r3, r3, r3
    b       .Lfinish
    .size _stdgba_reciprocal_apply, . - _stdgba_reciprocal_apply
