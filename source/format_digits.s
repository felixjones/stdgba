@===============================================================================
@
@ Fast unsigned 32-bit to reversed decimal ASCII digits for ARM7TDMI (GBA)
@
@ Provides:
@   _stdgba_utoa10_reversed - unsigned int -> reversed decimal digit string
@
@ Algorithm: Thumb loop in ROM that calls __aeabi_uidiv (IWRAM div.s) for
@ each divide-by-10, then computes the remainder via multiply-subtract.
@ This is faster than the compiler's inline Thumb division because the hot
@ division runs in IWRAM ARM mode at 1-cycle/instruction, while only the
@ lightweight loop overhead runs from the 16-bit ROM bus.
@
@===============================================================================
    .syntax unified
    .thumb
    .align 1
    .section .text._stdgba_utoa10_reversed, "ax", %progbits
@ =============================================================================
@ _stdgba_utoa10_reversed
@
@ Converts an unsigned 32-bit integer to reversed ASCII decimal digits.
@
@ Input:  r0 = value (unsigned 32-bit integer)
@         r1 = output buffer pointer (must hold at least 10 bytes)
@ Output: r0 = number of digits written
@ Clobbers: r1, r2, r3
@ =============================================================================
    .global _stdgba_utoa10_reversed
    .type _stdgba_utoa10_reversed, %function
    .thumb_func
_stdgba_utoa10_reversed:
    push    {r4, r5, r6, lr}
    movs    r4, r1              @ r4 = output pointer
    movs    r5, #0              @ r5 = digit count
    movs    r6, r0              @ r6 = current value

    @ Special case: value == 0
    cmp     r6, #0
    bne     .Lloop
    movs    r2, #48             @ '0'
    strb    r2, [r4, r5]
    movs    r0, #1
    pop     {r4, r5, r6, pc}

.Lloop:
    @ Call __aeabi_uidiv(value, 10) -> r0 = quotient
    movs    r0, r6
    movs    r1, #10
    bl      __aeabi_uidiv

    @ r0 = quotient, compute remainder = value - quotient * 10
    movs    r2, #10
    muls    r2, r0, r2          @ r2 = quotient * 10
    subs    r2, r6, r2          @ r2 = value - quotient * 10 = remainder

    @ Store ASCII digit
    adds    r2, #48             @ r2 = '0' + remainder
    strb    r2, [r4, r5]
    adds    r5, #1

    @ Loop if quotient > 0
    movs    r6, r0              @ r6 = quotient (new value)
    bne     .Lloop

    movs    r0, r5              @ return digit count
    pop     {r4, r5, r6, pc}

    .size _stdgba_utoa10_reversed, . - _stdgba_utoa10_reversed

@ =============================================================================
@ _stdgba_fixed_frac_digits_u16
@
@ Emits exactly `count` ASCII fractional digits for a power-of-two fixed-point
@ remainder where fracBits is in the range [1, 16].
@
@ Input:  r0 = remainder numerator (0 <= remainder < 2^fracBits)
@         r1 = fracBits
@         r2 = output buffer
@         r3 = count
@ Output: writes `count` digits to [r2, r2 + count)
@ Clobbers: r0, r2, r3
@ =============================================================================
    .global _stdgba_fixed_frac_digits_u16
    .type _stdgba_fixed_frac_digits_u16, %function
    .thumb_func
_stdgba_fixed_frac_digits_u16:
    push    {r4, r5, r6, r7, lr}
    movs    r4, r2              @ r4 = output pointer
    movs    r5, r3              @ r5 = remaining digit count
    cmp     r5, #0
    beq     .Lfrac_done

    movs    r6, #1
    lsls    r6, r6, r1          @ r6 = 1 << fracBits
    subs    r6, r6, #1          @ r6 = mask

.Lfrac_loop:
    movs    r2, r0
    lsls    r2, r2, #3          @ r2 = remainder * 8
    adds    r2, r2, r0          @ r2 = remainder * 9
    adds    r2, r2, r0          @ r2 = remainder * 10

    movs    r7, r2
    lsrs    r7, r1              @ r7 = digit = scaled >> fracBits
    adds    r7, r7, #48         @ ASCII digit
    strb    r7, [r4]
    adds    r4, #1

    movs    r0, r2
    ands    r0, r6              @ new remainder = scaled & mask

    subs    r5, r5, #1
    bne     .Lfrac_loop

.Lfrac_done:
    pop     {r4, r5, r6, r7, pc}

    .size _stdgba_fixed_frac_digits_u16, . - _stdgba_fixed_frac_digits_u16

@ =============================================================================
@ _stdgba_frac_digits_u32
@
@ Emits exactly `count` ASCII fractional digits where the denominator is 2^32.
@ This is used by angle semantic decimal formatting (degrees / turns), where
@ each step is: scaled = remainder * 10, digit = high32(scaled), remainder =
@ low32(scaled).
@
@ Input:  r0 = remainder numerator (0 <= remainder < 2^32)
@         r1 = output buffer
@         r2 = count
@ Output: writes `count` digits to [r1, r1 + count)
@ Clobbers: r0, r1, r2, r3
@ =============================================================================
    .global _stdgba_frac_digits_u32
    .type _stdgba_frac_digits_u32, %function
    .thumb_func
_stdgba_frac_digits_u32:
    push    {r4, r5, r6, r7, lr}
    movs    r4, r1              @ r4 = output pointer
    movs    r5, r2              @ r5 = remaining digit count
    cmp     r5, #0
    beq     .Lfrac32_done

.Lfrac32_loop:
    movs    r2, r0
    lsls    r2, r2, #3          @ low part of remainder * 8
    lsrs    r3, r0, #29         @ high part of remainder * 8

    movs    r6, r0
    lsls    r6, r6, #1          @ low part of remainder * 2
    lsrs    r7, r0, #31         @ high part of remainder * 2
    adds    r2, r2, r6          @ low32(remainder * 10)
    adcs    r3, r7              @ high32(remainder * 10)

    movs    r6, r3
    adds    r6, #48             @ ASCII digit
    strb    r6, [r4]
    adds    r4, #1

    movs    r0, r2              @ new remainder = low32(remainder * 10)
    subs    r5, r5, #1
    bne     .Lfrac32_loop

.Lfrac32_done:
    pop     {r4, r5, r6, r7, pc}

    .size _stdgba_frac_digits_u32, . - _stdgba_frac_digits_u32

@ =============================================================================
@ _stdgba_grouped3_copy_reversed
@
@ Copies reversed ASCII digits into forward order while inserting a separator
@ every 3 digits from the right.
@
@ Input:  r0 = reversed input buffer
@         r1 = digit count
@         r2 = output buffer
@         r3 = separator character
@ Output: r0 = output length (digits + separators)
@ Clobbers: r1, r2, r3
@ =============================================================================
    .section .text._stdgba_grouped3_copy_reversed, "ax", %progbits
    .global _stdgba_grouped3_copy_reversed
    .type _stdgba_grouped3_copy_reversed, %function
    .thumb_func
_stdgba_grouped3_copy_reversed:
    push    {r4, r5, r6, r7, lr}
    movs    r4, r0              @ r4 = reversed input
    movs    r5, r1              @ r5 = remaining digits
    movs    r6, r2              @ r6 = output pointer
    movs    r7, r3              @ r7 = separator
    movs    r0, #0              @ r0 = output length

    cmp     r5, #0
    beq     .Lgroup3_done

    @ rem = count % 3, but keep rem in [1, 3]
    movs    r3, r5
.Lgroup3_mod_loop:
    cmp     r3, #3
    bls     .Lgroup3_mod_done
    subs    r3, #3
    b       .Lgroup3_mod_loop

.Lgroup3_mod_done:
    cmp     r3, #0
    bne     .Lgroup3_loop
    movs    r3, #3

.Lgroup3_loop:
    subs    r5, #1
    adds    r2, r4, r5
    ldrb    r1, [r2]
    strb    r1, [r6]
    adds    r6, #1
    adds    r0, #1

    subs    r3, #1
    cmp     r5, #0
    beq     .Lgroup3_done

    cmp     r3, #0
    bne     .Lgroup3_loop

    strb    r7, [r6]
    adds    r6, #1
    adds    r0, #1
    movs    r3, #3
    b       .Lgroup3_loop

.Lgroup3_done:
    pop     {r4, r5, r6, r7, pc}

    .size _stdgba_grouped3_copy_reversed, . - _stdgba_grouped3_copy_reversed
