@===============================================================================
@
@ Optimized strcmp / strncmp for ARM7TDMI (GBA)
@
@ Provides:
@   strcmp  - C standard string comparison
@   strncmp - bounded string comparison
@
@ Runs from ROM in Thumb mode. Newlib's strcmp is ARM mode (incurs a
@ mode-switch penalty from Thumb callers and wastes half the 16-bit ROM
@ bus bandwidth on 32-bit fetches). This native Thumb implementation
@ avoids both penalties.
@
@ Algorithm:
@   - Byte-scan while misaligned or pointers have different alignment
@   - When both pointers are word-aligned: word-at-a-time loop using
@     XOR (to find differences) combined with null-byte detection
@   - On mismatch/null: byte-scan to resolve exact position
@
@ Register plan (hot loop):
@   r0 = s1 pointer (advancing)
@   r1 = s2 pointer (advancing)
@   r2 = 0x01010101
@   r3 = 0x80808080
@   r4 = loaded word from s1
@   r5 = loaded word from s2
@   r6 = scratch for detection
@   r7 = scratch
@
@===============================================================================
    .syntax unified
    .thumb
    .align 1
    .section .text._stdgba_strcmp, "ax", %progbits

@ =============================================================================
@ strcmp: compare two null-terminated strings
@
@ Input:  r0 = s1, r1 = s2
@ Output: r0 = <0 / 0 / >0
@ =============================================================================
    .global strcmp
    .thumb_func
    .type strcmp, %function
strcmp:
    push    {r4-r6, lr}

    @ Check alignment compatibility
    movs    r2, r0
    eors    r2, r1
    movs    r3, #3
    tst     r2, r3
    bne     .Lsc_byte_loop      @ different alignment: byte-only

    @ Both pointers share word alignment. Align to boundary.
    tst     r0, r3
    beq     .Lsc_aligned

.Lsc_align_byte:
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    cmp     r2, r3
    bne     .Lsc_diff_byte
    cmp     r2, #0
    beq     .Lsc_equal
    adds    r0, #1
    adds    r1, #1
    movs    r3, #3
    tst     r0, r3
    bne     .Lsc_align_byte

.Lsc_aligned:
    ldr     r2, .Lsc_magic01
    ldr     r3, .Lsc_magic80

    @ Word-at-a-time loop
    @ For each word: compute diff (w1^w2) and null detect on w1.
    @ If either is nonzero, we found a difference or terminator.
.Lsc_word_loop:
    ldr     r4, [r0]            @ w1
    ldr     r5, [r1]            @ w2

    @ Diff detect first (before we clobber r4)
    movs    r6, r4
    eors    r6, r5              @ r6 = w1 ^ w2 (saves into r6)

    @ Null detect: (w1 - 0x01010101) & ~w1 & 0x80808080
    subs    r4, r4, r2          @ r4 = w1 - 0x01010101
    ldr     r5, [r0]            @ reload w1 (need ~w1)
    mvns    r5, r5              @ r5 = ~w1
    ands    r4, r5              @ r4 = (w1 - 0x01010101) & ~w1
    ands    r4, r3              @ r4 = null detect result

    @ Combine: null OR diff
    orrs    r4, r6              @ r4 = null_detect | diff_detect
    bne     .Lsc_found

    adds    r0, #4
    adds    r1, #4
    b       .Lsc_word_loop

    @ Found a null or difference in current word pair
.Lsc_found:
    @ Byte-scan to resolve
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    cmp     r2, r3
    bne     .Lsc_diff_byte
    cmp     r2, #0
    beq     .Lsc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    cmp     r2, r3
    bne     .Lsc_diff_byte
    cmp     r2, #0
    beq     .Lsc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    cmp     r2, r3
    bne     .Lsc_diff_byte
    cmp     r2, #0
    beq     .Lsc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    @ Must differ or be null here (4th byte of word)
    subs    r0, r2, r3
    pop     {r4-r6, pc}

    @ Byte-by-byte fallback (misaligned pointers)
.Lsc_byte_loop:
    ldrb    r2, [r0]
    ldrb    r3, [r1]
    cmp     r2, r3
    bne     .Lsc_diff_byte
    cmp     r2, #0
    beq     .Lsc_equal
    adds    r0, #1
    adds    r1, #1
    b       .Lsc_byte_loop

.Lsc_diff_byte:
    subs    r0, r2, r3
    pop     {r4-r6, pc}

.Lsc_equal:
    movs    r0, #0
    pop     {r4-r6, pc}

    .align 2
.Lsc_magic01:
    .word   0x01010101
.Lsc_magic80:
    .word   0x80808080
    .size strcmp, . - strcmp

@ =============================================================================
@ strncmp: bounded string comparison
@
@ Input:  r0 = s1, r1 = s2, r2 = maxlen
@ Output: r0 = <0 / 0 / >0
@ =============================================================================
    .global strncmp
    .thumb_func
    .type strncmp, %function
strncmp:
    push    {r4-r7, lr}
    movs    r7, r2              @ r7 = remaining count

    cmp     r7, #0
    beq     .Lsnc_equal

    @ Check alignment compatibility
    movs    r3, r0
    eors    r3, r1
    movs    r4, #3
    tst     r3, r4
    bne     .Lsnc_byte_loop     @ different alignment: byte-only

    @ Same alignment. Align to boundary.
    tst     r0, r4
    beq     .Lsnc_aligned

.Lsnc_align_byte:
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_byte
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    subs    r7, #1
    beq     .Lsnc_equal
    movs    r4, #3
    tst     r0, r4
    bne     .Lsnc_align_byte

.Lsnc_aligned:
    cmp     r7, #3
    ble     .Lsnc_tail

    ldr     r2, .Lsnc_magic01
    ldr     r3, .Lsnc_magic80

.Lsnc_word_loop:
    ldr     r4, [r0]            @ w1
    ldr     r5, [r1]            @ w2

    @ Diff detect first (before clobbering r4)
    movs    r6, r4
    eors    r6, r5              @ r6 = w1 ^ w2

    @ Null detect: (w1 - 0x01010101) & ~w1 & 0x80808080
    subs    r4, r4, r2
    ldr     r5, [r0]            @ reload w1
    mvns    r5, r5
    ands    r4, r5
    ands    r4, r3

    @ Combine
    orrs    r4, r6
    bne     .Lsnc_found

    adds    r0, #4
    adds    r1, #4
    subs    r7, #4
    cmp     r7, #3
    bgt     .Lsnc_word_loop

.Lsnc_tail:
    cmp     r7, #0
    beq     .Lsnc_equal
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_r3r4
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    subs    r7, #1
    b       .Lsnc_tail

.Lsnc_found:
    @ Check remaining count: at least 4 bytes remain (we entered word loop)
    @ Byte-scan up to 4 bytes
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_r3r4
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_r3r4
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_r3r4
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    subs    r0, r3, r4
    pop     {r4-r7, pc}

.Lsnc_byte_loop:
    ldrb    r3, [r0]
    ldrb    r4, [r1]
    cmp     r3, r4
    bne     .Lsnc_diff_r3r4
    cmp     r3, #0
    beq     .Lsnc_equal
    adds    r0, #1
    adds    r1, #1
    subs    r7, #1
    bne     .Lsnc_byte_loop

.Lsnc_equal:
    movs    r0, #0
    pop     {r4-r7, pc}

.Lsnc_diff_byte:
    subs    r0, r3, r4
    pop     {r4-r7, pc}

.Lsnc_diff_r3r4:
    subs    r0, r3, r4
    pop     {r4-r7, pc}

    .align 2
.Lsnc_magic01:
    .word   0x01010101
.Lsnc_magic80:
    .word   0x80808080
    .size strncmp, . - strncmp
