@ Agbabi div/ldiv reference implementations for benchmarking.
@ Symbols renamed to avoid conflict with stdgba.
@
@ Original: agbabi (zlib license, github.com/felixjones/agbabi)
@ Copyright (C) 2021-2023 agbabi contributors

.syntax unified
    .arm
    .align 2

@ =============================================================================
@ 32-bit unsigned division (uidiv / uidivmod)
@ =============================================================================
    .section .iwram._bench_agbabi_uidiv, "ax", %progbits
    .global bench_agbabi_uidivmod
    .type bench_agbabi_uidivmod, %function
bench_agbabi_uidivmod:
    .global bench_agbabi_uidiv
    .type bench_agbabi_uidiv, %function
bench_agbabi_uidiv:
    cmp     r1, #0
    beq     .Lab_div0_32

    @ If n < d, bail
    cmp     r0, r1
    movlo   r1, r0
    movlo   r0, #0
    bxlo    lr

    mov     r2, r1
    mov     r3, #28
    lsr     r1, r0, #4

    cmp     r2, r1, lsr #12
    suble   r3, r3, #16
    lsrle   r1, r1, #16

    cmp     r2, r1, lsr #4
    suble   r3, r3, #8
    lsrle   r1, r1, #8

    cmp     r2, r1
    suble   r3, r3, #4
    lsrle   r1, r1, #4

    lsl     r0, r0, r3
    adds    r0, r0, r0
    rsb     r2, r2, #0

    add     r3, r3, r3, lsl #1
    add     pc, pc, r3, lsl #2
    mov     r0, r0

    .rept 32
    adcs    r1, r2, r1, lsl #1
    sublo   r1, r1, r2
    adcs    r0, r0, r0
    .endr

    bx      lr

.Lab_div0_32:
    mov     r0, #0
    mov     r1, #0
    bx      lr

@ =============================================================================
@ 32-bit signed division (idiv / idivmod)
@ =============================================================================
    .section .iwram._bench_agbabi_idiv, "ax", %progbits
    .global bench_agbabi_idivmod
    .type bench_agbabi_idivmod, %function
bench_agbabi_idivmod:
    .global bench_agbabi_idiv
    .type bench_agbabi_idiv, %function
bench_agbabi_idiv:
    cmp     r1, #0
    beq     .Lab_div0_32

    mov     r12, lr

    cmp     r0, #0
    rsblt   r0, r0, #0
    orrlt   r12, #1 << 28

    cmp     r1, #0
    rsblt   r1, r1, #0
    orrlt   r12, #1 << 31

    bl      bench_agbabi_uidivmod + 16  @ skip div-by-zero check

    msr     cpsr_f, r12

    rsblt   r0, r0, #0
    rsbvs   r1, r1, #0

    bic     r12, #0xF << 28
    bx      r12

@ =============================================================================
@ 64-bit unsigned division: 64/32 path (uluidivmod)
@ =============================================================================
    .section .iwram._bench_agbabi_uluidiv, "ax", %progbits
    .global bench_agbabi_uluidivmod
    .type bench_agbabi_uluidivmod, %function
bench_agbabi_uluidivmod:
    cmp     r2, #0
    beq     .Lab_div0_64

    cmp     r1, #0
    beq     .Lab_bridgeTo32

    push    {r4}
    rsb     r3, r2, #0
    mov     r4, #28
    lsr     r2, r1, #4

    cmn     r3, r2, lsr #12
    subge   r4, r4, #16
    lsrge   r2, r2, #16

    cmn     r3, r2, lsr #4
    subge   r4, r4, #8
    lsrge   r2, r2, #8

    cmn     r3, r2
    subge   r4, r4, #4
    lsrge   r2, r2, #4

    lsl     r1, r1, r4
    rsb     r4, r4, #32
    orr     r1, r1, r0, lsr r4
    rsb     r4, r4, #32
    lsl     r0, r0, r4
    adds    r0, r0, r0
    adcs    r1, r1, r1

    add     pc, pc, r4, lsl #4
    mov     r0, r0

    .rept 64
    adcs    r2, r3, r2, lsl #1
    subcc   r2, r2, r3
    adcs    r0, r0, r0
    adcs    r1, r1, r1
    .endr

    pop     {r4}
    mov     r3, #0
    bx      lr

.Lab_bridgeTo32:
    mov     r1, r2
    push    {lr}
    bl      bench_agbabi_uidivmod + 16  @ skip div-by-zero check
    pop     {lr}
    mov     r2, r1
    mov     r1, #0
    mov     r3, #0
    bx      lr

.Lab_div0_64:
    mov     r0, #0
    mov     r1, #0
    mov     r2, #0
    mov     r3, #0
    bx      lr

@ =============================================================================
@ 64-bit unsigned division: 64/64 path (uldivmod)
@ =============================================================================
    .section .iwram._bench_agbabi_uldiv, "ax", %progbits
    .global bench_agbabi_uldivmod
    .type bench_agbabi_uldivmod, %function
bench_agbabi_uldivmod:
    cmp     r3, #0
    beq     bench_agbabi_uluidivmod

    @ n < d check
    cmp     r1, r3
    cmpeq   r0, r2
    blo     .Lab_zero_quot_64

    push    {r4-r5}
    rsbs    r4, r2, #0
    mov     r2, r1
    rsc     r1, r3, #0

    mov     r5, #30
    lsr     r3, r2, #2

    cmn     r1, r3, lsr #14
    subge   r5, r5, #16
    lsrge   r3, r3, #16

    cmn     r1, r3, lsr #6
    subge   r5, r5, #8
    lsrge   r3, r3, #8

    cmn     r1, r3, lsr #2
    subge   r5, r5, #4
    lsrge   r3, r3, #4

    cmn     r1, r3
    subge   r5, r5, #2
    lsrge   r3, r3, #2

    lsl     r2, r2, r5
    rsb     r5, r5, #32
    orr     r2, r2, r0, lsr r5
    rsb     r5, r5, #32
    lsl     r0, r0, r5
    adds    r0, r0, r0

    add     r5, r5, r5, lsl #3
    add     pc, pc, r5, lsl #2
    mov     r0, r0

    .rept 32
    adcs    r2, r2, r2
    adc     r3, r3, r3
    adds    r2, r2, r4
    adcs    r3, r3, r1
    bcs     1f
    subs    r2, r2, r4
    sbc     r3, r3, r1
    adds    r0, r0, #0
1:
    adcs    r0, r0, r0
    .endr

    pop     {r4-r5}
    mov     r1, #0
    bx      lr

.Lab_zero_quot_64:
    mov     r2, r0
    mov     r3, r1
    mov     r0, #0
    mov     r1, #0
    bx      lr

@ =============================================================================
@ 64-bit signed division (ldivmod)
@ =============================================================================
    .section .iwram._bench_agbabi_ldiv, "ax", %progbits
    .global bench_agbabi_ldivmod
    .type bench_agbabi_ldivmod, %function
bench_agbabi_ldivmod:
    cmp     r3, #0
    cmpeq   r2, #0
    beq     .Lab_div0_64

    mov     r12, lr

    cmp     r1, #0
    orrlt   r12, #1 << 28
    bge     .Lab_num_pos
    rsbs    r0, r0, #0
    rsc     r1, r1, #0
.Lab_num_pos:

    cmp     r3, #0
    orrlt   r12, #1 << 31
    bge     .Lab_den_pos
    rsbs    r2, r2, #0
    rsc     r3, r3, #0
.Lab_den_pos:

    cmp     r3, #0
    adreq   lr, .Lab_skip_sign
    beq     bench_agbabi_uluidivmod + 8  @ skip div-by-zero check

    bl      bench_agbabi_uldivmod + 8    @ skip dispatch (r3 != 0)

.Lab_skip_sign:
    msr     cpsr_f, r12

    bge     .Lab_quot_pos
    rsbs    r0, r0, #0
    rsc     r1, r1, #0
    msr     cpsr_f, r12
.Lab_quot_pos:

    bvc     .Lab_rem_pos
    rsbs    r2, r2, #0
    rsc     r3, r3, #0
.Lab_rem_pos:

    bic     r12, #0xF << 28
    bx      r12
    .size bench_agbabi_ldivmod, . - bench_agbabi_ldivmod
