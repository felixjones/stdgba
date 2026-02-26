@===============================================================================
@
@ Optimized strlen / strnlen for ARM7TDMI (GBA)
@
@ Provides:
@   strlen  - C standard string length
@   strnlen - bounded string length
@
@ Runs from ROM in Thumb mode. The 16-bit ROM bus means Thumb fetches are
@ 1 sequential cycle each (vs 2 for ARM), making Thumb the right choice
@ for ROM-resident code.
@
@ Algorithm: byte-scan to align the pointer, then word-at-a-time using
@ the null-byte detection trick: (word - 0x01010101) & ~word & 0x80808080
@ is nonzero iff any byte in the word is zero.
@
@ Register plan (Thumb r0-r7 only in hot loop):
@   r0 = current pointer (advancing)
@   r1 = scratch / loaded word
@   r2 = 0x01010101
@   r3 = 0x80808080
@   r4 = scratch (copy of word for null detect)
@   r12 = saved original pointer (high register, set once)
@
@===============================================================================
    .syntax unified
    .thumb
    .align 1
    .section .text._stdgba_strlen, "ax", %progbits

@ =============================================================================
@ strlen: find length of null-terminated string
@
@ Input:  r0 = pointer to string
@ Output: r0 = length (bytes before the null terminator)
@ Clobbers: r1-r4, r12
@ =============================================================================
    .global strlen
    .thumb_func
    .type strlen, %function
strlen:
    push    {r4, lr}
    mov     r12, r0             @ save original pointer

    @ Align to word boundary by scanning 0-3 bytes
    movs    r1, #3
    tst     r0, r1
    beq     .Ls_aligned

.Ls_align_byte:
    ldrb    r1, [r0]
    adds    r0, #1
    cmp     r1, #0
    beq     .Ls_found_align
    movs    r1, #3
    tst     r0, r1
    bne     .Ls_align_byte

.Ls_aligned:
    @ Load magic constants for null detection
    ldr     r2, .Ls_magic01
    ldr     r3, .Ls_magic80

    @ Word-at-a-time null detection loop (2 words per iteration)
    @ Detect: (w - 0x01010101) & ~w & 0x80808080
.Ls_loop:
    @ First word
    ldr     r1, [r0]            @ w0
    movs    r4, r1              @ r4 = w0
    subs    r1, r1, r2          @ r1 = w0 - 0x01010101
    mvns    r4, r4              @ r4 = ~w0
    ands    r1, r4              @ r1 = (w0 - 0x01010101) & ~w0
    ands    r1, r3              @ r1 = ... & 0x80808080
    bne     .Ls_found_word

    @ Second word
    ldr     r1, [r0, #4]        @ w1
    movs    r4, r1
    subs    r1, r1, r2
    mvns    r4, r4
    ands    r1, r4
    ands    r1, r3
    bne     .Ls_found_word_1

    adds    r0, #8
    b       .Ls_loop

.Ls_found_word_1:
    adds    r0, #4              @ adjust pointer to second word

    @ Null byte is somewhere in the word at [r0].
    @ Byte-scan to find exact position (max 3 iterations).
.Ls_found_word:
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Ls_done
    adds    r0, #1
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Ls_done
    adds    r0, #1
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Ls_done
    adds    r0, #1              @ must be byte 3

.Ls_done:
    mov     r1, r12
    subs    r0, r0, r1          @ length = current - original
    pop     {r4, pc}

    @ Found null during alignment scan (r0 already incremented past it)
.Ls_found_align:
    subs    r0, #1
    mov     r1, r12
    subs    r0, r0, r1
    pop     {r4, pc}

    .align 2
.Ls_magic01:
    .word   0x01010101
.Ls_magic80:
    .word   0x80808080
    .size strlen, . - strlen

@ =============================================================================
@ strnlen: bounded string length
@
@ Input:  r0 = pointer to string, r1 = maxlen
@ Output: r0 = min(strlen(s), maxlen)
@ Clobbers: r1-r5, r12
@ =============================================================================
    .global strnlen
    .thumb_func
    .type strnlen, %function
strnlen:
    push    {r4-r5, lr}
    mov     r12, r0             @ save original pointer
    movs    r5, r1              @ r5 = remaining count

    cmp     r5, #0
    beq     .Lsn_zero

    @ Byte scan until aligned or count exhausted
    movs    r1, #3
    tst     r0, r1
    beq     .Lsn_check_word

.Lsn_align_byte:
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Lsn_done
    adds    r0, #1
    subs    r5, #1
    beq     .Lsn_done
    movs    r1, #3
    tst     r0, r1
    bne     .Lsn_align_byte

.Lsn_check_word:
    cmp     r5, #7
    ble     .Lsn_word_single

    ldr     r2, .Lsn_magic01
    ldr     r3, .Lsn_magic80

    @ Unrolled: 2 words per iteration (8 bytes)
.Lsn_loop:
    ldr     r1, [r0]
    movs    r4, r1
    subs    r1, r1, r2
    mvns    r4, r4
    ands    r1, r4
    ands    r1, r3
    bne     .Lsn_found_word

    ldr     r1, [r0, #4]
    movs    r4, r1
    subs    r1, r1, r2
    mvns    r4, r4
    ands    r1, r4
    ands    r1, r3
    bne     .Lsn_found_word_1

    adds    r0, #8
    subs    r5, #8
    cmp     r5, #7
    bgt     .Lsn_loop

    @ Handle remaining 4-7 bytes
.Lsn_word_single:
    cmp     r5, #3
    ble     .Lsn_tail
    ldr     r2, .Lsn_magic01
    ldr     r3, .Lsn_magic80
    ldr     r1, [r0]
    movs    r4, r1
    subs    r1, r1, r2
    mvns    r4, r4
    ands    r1, r4
    ands    r1, r3
    bne     .Lsn_found_word
    adds    r0, #4
    subs    r5, #4

.Lsn_tail:
    cmp     r5, #0
    beq     .Lsn_done
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Lsn_done
    adds    r0, #1
    subs    r5, #1
    b       .Lsn_tail

.Lsn_found_word_1:
    adds    r0, #4              @ adjust for second word
.Lsn_found_word:
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Lsn_done
    adds    r0, #1
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Lsn_done
    adds    r0, #1
    ldrb    r1, [r0]
    cmp     r1, #0
    beq     .Lsn_done
    adds    r0, #1

.Lsn_done:
    mov     r1, r12
    subs    r0, r0, r1
    pop     {r4-r5, pc}

.Lsn_zero:
    movs    r0, #0
    pop     {r4-r5, pc}

    .align 2
.Lsn_magic01:
    .word   0x01010101
.Lsn_magic80:
    .word   0x80808080
    .size strnlen, . - strnlen
