@ Agbabi lmul/shift reference implementations for benchmarking.
@ Symbols renamed to avoid conflict with stdgba.
@
@ Original: agbabi (zlib license, github.com/felixjones/agbabi)
@ Copyright (C) 2021-2023 agbabi contributors

.syntax unified
    .arm
    .align 2

@ =============================================================================
@ 64-bit multiplication
@ =============================================================================
    .section .iwram._bench_agbabi_lmul, "ax", %progbits
    .global bench_agbabi_lmul
    .type bench_agbabi_lmul, %function
bench_agbabi_lmul:
    mul     r3, r0, r3
    mla     r1, r2, r1, r3
    umull   r0, r3, r2, r0
    add     r1, r1, r3
    bx      lr

@ =============================================================================
@ 64-bit logical shift left
@ =============================================================================
    .section .iwram._bench_agbabi_llsl, "ax", %progbits
    .global bench_agbabi_llsl
    .type bench_agbabi_llsl, %function
bench_agbabi_llsl:
    subs    r3, r2, #32
    rsb     r12, r2, #32
    lslmi   r1, r1, r2
    lslpl   r1, r0, r3
    orrmi   r1, r1, r0, lsr r12
    lsl     r0, r0, r2
    bx      lr

@ =============================================================================
@ 64-bit logical shift right
@ =============================================================================
    .section .iwram._bench_agbabi_llsr, "ax", %progbits
    .global bench_agbabi_llsr
    .type bench_agbabi_llsr, %function
bench_agbabi_llsr:
    subs    r3, r2, #32
    rsb     r12, r2, #32
    lsrmi   r0, r0, r2
    lsrpl   r0, r1, r3
    orrmi   r0, r0, r1, lsl r12
    lsr     r1, r1, r2
    bx      lr

@ =============================================================================
@ 64-bit arithmetic shift right
@ =============================================================================
    .section .iwram._bench_agbabi_lasr, "ax", %progbits
    .global bench_agbabi_lasr
    .type bench_agbabi_lasr, %function
bench_agbabi_lasr:
    subs    r3, r2, #32
    rsb     r12, r2, #32
    lsrmi   r0, r0, r2
    asrpl   r0, r1, r3
    orrmi   r0, r0, r1, lsl r12
    asr     r1, r1, r2
    bx      lr
