@ Simple word-at-a-time memcmp reference for benchmarking.
@ Placed in IWRAM to match stdgba's memory advantage.

.syntax unified
    .arm
    .align 2

    .section .iwram._bench_newlib_memcmp, "ax", %progbits
    .global bench_newlib_memcmp
    .type bench_newlib_memcmp, %function
bench_newlib_memcmp:
    cmp     r2, #3
    ble     .Lref_bytes

    @ Check if both pointers are word-aligned
    orr     r3, r0, r1
    tst     r3, #3
    bne     .Lref_bytes

    @ Word-aligned loop
.Lref_words:
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lref_word_diff
    subs    r2, r2, #4
    bgt     .Lref_words
    mov     r0, #0
    bx      lr

.Lref_word_diff:
    sub     r0, r0, #4
    sub     r1, r1, #4
    @ Fall through to byte compare

.Lref_bytes:
    subs    r2, r2, #1
    blt     .Lref_equal
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    subs    r3, r3, r12
    beq     .Lref_bytes
    mov     r0, r3
    bx      lr

.Lref_equal:
    mov     r0, #0
    bx      lr
    .size bench_newlib_memcmp, . - bench_newlib_memcmp

    .global bench_newlib_bcmp
    .type bench_newlib_bcmp, %function
bench_newlib_bcmp:
    @ Newlib bcmp is just memcmp
    b       bench_newlib_memcmp
    .size bench_newlib_bcmp, . - bench_newlib_bcmp
