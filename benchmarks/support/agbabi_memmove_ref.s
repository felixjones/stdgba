@ Agbabi memmove reference implementation for benchmarking.
@ Symbols renamed to avoid conflict with stdgba's memmove.
@
@ Original: agbabi (zlib license, github.com/felixjones/agbabi)
@ Copyright (C) 2021-2023 agbabi contributors
@
@ This is a faithful recreation of agbabi's memmove approach:
@   - Forward: delegates to memcpy
@   - Backward: byte loop for misaligned, word loop for aligned

.syntax unified
    .arm
    .align 2

    .section .iwram._bench_agbabi_memmove, "ax", %progbits

    .global bench_agbabi_memmove
    .type bench_agbabi_memmove, %function
bench_agbabi_memmove:
    cmp     r0, r1
    bls     bench_agbabi_memcpy         @ dest <= src: forward
    add     r3, r1, r2
    cmp     r0, r3
    bhs     bench_agbabi_memcpy         @ dest >= src+n: no overlap

    @ Backward copy
    add     r0, r0, r2
    add     r1, r1, r2

    cmp     r2, #6
    ble     .Lagbabi_rev_bytes

    @ Check alignment compatibility
    eor     r3, r0, r1
    tst     r3, #3
    bne     .Lagbabi_rev_bytes          @ Not word-compatible

    @ Align to word boundary from end
    movs    r3, r0, lsl #31
    ldrbmi  r3, [r1, #-1]!
    strbmi  r3, [r0, #-1]!
    submi   r2, r2, #1
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    subcs   r2, r2, #2

    .global bench_agbabi_memmove4
    .type bench_agbabi_memmove4, %function
bench_agbabi_memmove4:
    cmp     r0, r1
    bls     bench_agbabi_memcpy4
    add     r3, r1, r2
    cmp     r0, r3
    bhs     bench_agbabi_memcpy4

    add     r0, r0, r2
    add     r1, r1, r2

    @ Re-align if n not multiple of 4
    movs    r3, r2, lsl #31
    ldrbmi  r3, [r1, #-1]!
    strbmi  r3, [r0, #-1]!
    submi   r2, r2, #1
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    subcs   r2, r2, #2

.Lagbabi_rev_aligned:
    cmp     r2, #32
    blt     .Lagbabi_rev_words

    push    {r4-r10}
.Lagbabi_rev_32:
    subs    r2, r2, #32
    ldmdbge r1!, {r3-r10}
    stmdbge r0!, {r3-r10}
    bgt     .Lagbabi_rev_32
    pop     {r4-r10}
    bxeq    lr

    add     r2, r2, #32

.Lagbabi_rev_words:
    cmp     r2, #4
    blt     .Lagbabi_rev_tail
.Lagbabi_rev_word_loop:
    subs    r2, r2, #4
    ldrge   r3, [r1, #-4]!
    strge   r3, [r0, #-4]!
    bgt     .Lagbabi_rev_word_loop
    bxeq    lr

.Lagbabi_rev_tail:
    movs    r3, r2, lsl #31
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    ldrbmi  r3, [r1, #-1]
    strbmi  r3, [r0, #-1]
    bx      lr

.Lagbabi_rev_bytes:
    subs    r2, r2, #1
    ldrbge  r3, [r1, #-1]!
    strbge  r3, [r0, #-1]!
    bgt     .Lagbabi_rev_bytes
    bx      lr
    .size bench_agbabi_memmove, . - bench_agbabi_memmove
