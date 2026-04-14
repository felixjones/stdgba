@ Agbabi memcpy reference implementation for benchmarking.
@ Symbols renamed to avoid conflict with stdgba's memcpy.
@
@ Original: agbabi (zlib license, github.com/felixjones/agbabi)
@ Copyright (C) 2021-2023 agbabi contributors

.syntax unified
    .arm
    .align 2

    .section .iwram._bench_agbabi_memcpy, "ax", %progbits
    .global bench_agbabi_memcpy
    .type bench_agbabi_memcpy, %function
bench_agbabi_memcpy:
    @ >6-bytes is roughly the threshold when byte-by-byte copy is slower
    cmp     r2, #6
    ble     .Lagbabi_copy1

    @ align_switch: eor + joaobapt_switch
    eor     r3, r0, r1
    movs    r3, r3, lsl #31
    bmi     .Lagbabi_copy1
    bcs     .Lagbabi_copy_halves

    @ Check if r0 (or r1) needs word aligning
    rsbs    r3, r0, #4
    movs    r3, r3, lsl #31

    @ Copy byte head to align
    ldrbmi  r3, [r1], #1
    strbmi  r3, [r0], #1
    submi   r2, r2, #1

    @ Copy half head to align
    ldrhcs  r3, [r1], #2
    strhcs  r3, [r0], #2
    subcs   r2, r2, #2

    .global bench_agbabi_memcpy4
    .type bench_agbabi_memcpy4, %function
bench_agbabi_memcpy4:
    cmp     r2, #32
    blt     .Lagbabi_copy_words

    @ Word aligned, 32-byte copy
    push    {r4-r10}
.Lagbabi_loop_32:
    subs    r2, r2, #32
    ldmiage r1!, {r3-r10}
    stmiage r0!, {r3-r10}
    bgt     .Lagbabi_loop_32
    pop     {r4-r10}
    bxeq    lr

    @ < 32 bytes remaining to be copied
    add     r2, r2, #32

.Lagbabi_copy_words:
    cmp     r2, #4
    blt     .Lagbabi_copy_halves
.Lagbabi_loop_4:
    subs    r2, r2, #4
    ldrge   r3, [r1], #4
    strge   r3, [r0], #4
    bgt     .Lagbabi_loop_4
    bxeq    lr

    @ Copy byte & half tail
    movs    r3, r2, lsl #31
    @ Copy half
    ldrhcs  r3, [r1], #2
    strhcs  r3, [r0], #2
    @ Copy byte
    ldrbmi  r3, [r1]
    strbmi  r3, [r0]
    bx      lr

.Lagbabi_copy_halves:
    @ Copy byte head to align
    tst     r0, #1
    ldrbne  r3, [r1], #1
    strbne  r3, [r0], #1
    subne   r2, r2, #1

.Lagbabi_memcpy2:
    subs    r2, r2, #2
    ldrhge  r3, [r1], #2
    strhge  r3, [r0], #2
    bgt     .Lagbabi_memcpy2
    bxeq    lr

    @ Copy byte tail
    adds    r2, r2, #2
    ldrbne  r3, [r1]
    strbne  r3, [r0]
    bx      lr

.Lagbabi_copy1:
    subs    r2, r2, #1
    ldrbge  r3, [r1], #1
    strbge  r3, [r0], #1
    bgt     .Lagbabi_copy1
    bx      lr
    .size bench_agbabi_memcpy, . - bench_agbabi_memcpy
