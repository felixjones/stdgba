@ Agbabi memset reference implementation for benchmarking.
@ Symbols renamed to avoid conflict with stdgba's memset.
@
@ Original: agbabi (zlib license, github.com/felixjones/agbabi)
@ Copyright (C) 2021-2023 agbabi contributors

.syntax unified
    .arm
    .align 2

    .section .iwram._bench_agbabi_memset, "ax", %progbits

@ __aeabi_memclr reference
    .global bench_agbabi_memclr
    .type bench_agbabi_memclr, %function
bench_agbabi_memclr:
    mov     r2, #0
    b       bench_agbabi_memset

    .global bench_agbabi_memclr4
    .type bench_agbabi_memclr4, %function
bench_agbabi_memclr4:
    mov     r2, #0
    @ Fall through

@ __aeabi_memset4/8 reference
    .global bench_agbabi_memset4
    .type bench_agbabi_memset4, %function
bench_agbabi_memset4:
    @ Broadcast byte
    and     r2, r2, #0xFF
    orr     r2, r2, r2, lsl #8
    orr     r2, r2, r2, lsl #16

    cmp     r1, #32
    blt     .Lagbabi_fill_words

    @ Spread to r3-r10
    mov     r3, r2
    push    {r4-r10}
    mov     r4, r2
    mov     r5, r2
    mov     r6, r2
    mov     r7, r2
    mov     r8, r2
    mov     r9, r2
    mov     r10, r2
.Lagbabi_fill_32:
    subs    r1, r1, #32
    stmiage r0!, {r2-r9}
    bgt     .Lagbabi_fill_32
    pop     {r4-r10}
    bxeq    lr

    add     r1, r1, #32

.Lagbabi_fill_words:
    cmp     r1, #4
    blt     .Lagbabi_fill_halves
.Lagbabi_fill_word_loop:
    subs    r1, r1, #4
    strge   r2, [r0], #4
    bgt     .Lagbabi_fill_word_loop
    bxeq    lr

    @ Fill byte & half tail
    movs    r3, r1, lsl #31
    @ Fill half
    strhcs  r2, [r0], #2
    @ Fill byte
    strbmi  r2, [r0]
    bx      lr

.Lagbabi_fill_halves:
    @ Fill byte head to align
    tst     r0, #1
    strbne  r2, [r0], #1
    subne   r1, r1, #1

.Lagbabi_fill_half_loop:
    subs    r1, r1, #2
    strhge  r2, [r0], #2
    bgt     .Lagbabi_fill_half_loop
    bxeq    lr

    @ Fill byte tail
    adds    r1, r1, #2
    strbne  r2, [r0]
    bx      lr

@ __aeabi_memset reference (generic entry, arbitrary alignment)
    .global bench_agbabi_memset
    .type bench_agbabi_memset, %function
bench_agbabi_memset:
    @ >6-bytes is roughly the threshold
    cmp     r1, #6
    ble     .Lagbabi_fill_bytes

    @ Align to word boundary
    rsbs    r3, r0, #4
    movs    r3, r3, lsl #31

    strbmi  r2, [r0], #1
    submi   r1, r1, #1

    strhcs  r2, [r0], #2
    subcs   r1, r1, #2

    b       bench_agbabi_memset4

.Lagbabi_fill_bytes:
    subs    r1, r1, #1
    strbge  r2, [r0], #1
    bgt     .Lagbabi_fill_bytes
    bx      lr
    .size bench_agbabi_memset, . - bench_agbabi_memset
