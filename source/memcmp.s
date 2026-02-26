@===============================================================================
@
@ memcmp / bcmp for ARM7TDMI (GBA)
@
@ Provides:
@   memcmp  - C standard memory comparison
@   bcmp    - equality-only comparison (GCC may lower memcmp()==0 to this)
@
@ Input:  r0 = s1, r1 = s2, r2 = byte count
@ Output: r0 = <0 / 0 / >0 (memcmp), 0 or non-zero (bcmp)
@
@ Strategy:
@   - n <= 3: byte-by-byte
@   - Word-aligned fast-path (skip alignment fixup when already aligned)
@   - Joaobapt alignment fixup (branchless, max 3 bytes)
@   - Bulk: 64-byte loop with 4x ldmia pairs, XOR + OR-reduce, subs+bge
@   - 32-byte handler for 32-63 byte case
@   - Computed jump for 1-7 remaining words
@   - Joaobapt tail for 1-3 remaining bytes
@   - On word mismatch: register-based diff resolution (no memory re-reads)
@
@ Register usage: r0-r3, r12 are caller-saved. r4-r9 saved/restored
@ only in the bulk path. lr is never clobbered.
@
@ Runs in IWRAM (ARM, 32-bit bus, 1-cycle access) for maximum throughput.
@
@===============================================================================
.syntax unified
    .arm
    .align 2
    .section .iwram._stdgba_memcmp, "ax", %progbits

    .global __stdgba_memcmp
    .type __stdgba_memcmp, %function
__stdgba_memcmp:
    @ Small (<=3): byte compare directly
    cmp     r2, #3
    ble     .Lcmp_bytes

    @ Check alignment compatibility
    eor     r3, r0, r1
    tst     r3, #3
    bne     .Lcmp_bytes             @ Not word-compatible: byte loop

    @ Fast path: skip alignment fixup when already word-aligned.
    @ The eor/tst above confirmed word-compatible alignment, so if r0
    @ is word-aligned then r1 is too. tst(1) + beq(3) = 4 cycles vs
    @ the full joaobapt path's 8 cycles (aligned case). Saves 4 cycles.
    tst     r0, #3
    beq     .Lcmp_aligned

    @ Joaobapt alignment: compare up to 3 bytes to reach word boundary.
    @ (-r0) & 3 gives bytes needed. lsl #31 maps bit0->N, bit1->C.
    rsbs    r3, r0, #4
    movs    r3, r3, lsl #31
    bpl     .Lcmp_align_no_byte
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    sub     r2, r2, #1
    cmp     r3, r12
    bne     .Lcmp_ret_r3_r12
.Lcmp_align_no_byte:
    bcc     .Lcmp_aligned
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    sub     r2, r2, #1
    cmp     r3, r12
    bne     .Lcmp_ret_r3_r12
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    sub     r2, r2, #1
    cmp     r3, r12
    bne     .Lcmp_ret_r3_r12

.Lcmp_aligned:
    @ Bulk path: 64 bytes/iteration with subs+bge back-edge.
    @ Eliminates the cmp+bge overhead that the old subsge cascade required
    @ (orrs clobbers flags, so we can't reuse subs flags for the back-edge).
    @ New: subs AFTER the XOR/OR chain. 84 cycles/64 bytes vs 88 old.
    cmp     r2, #32
    blt     .Lcmp_words

    push    {r4-r9}

    @ Pre-subtract 64 to test if we can do at least one full iteration.
    @ For 32-63 bytes: handle one 32-byte block then exit.
    subs    r2, r2, #64
    blt     .Lcmp_bulk_32

.Lcmp_bulk:
    @ Half-block 1 (bytes 0-15)
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    @ Half-block 2 (bytes 16-31)
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    @ Half-block 3 (bytes 32-47)
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    @ Half-block 4 (bytes 48-63)
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    @ Back-edge: 84 cycles per 64 bytes (was 88 with cmp+bge)
    subs    r2, r2, #64
    bge     .Lcmp_bulk

    @ Remainder: r2 = -64 to -1 (0 to 63 bytes left)
    adds    r2, r2, #64
    beq     .Lcmp_bulk_done

    @ Check if 32+ bytes remain (need one more 32-byte block)
    cmp     r2, #32
    blt     .Lcmp_bulk_done
    @ One more 32-byte block
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    sub     r2, r2, #32

.Lcmp_bulk_done:
    pop     {r4-r9}
    cmp     r2, #0
    beq     .Lcmp_equal
    b       .Lcmp_words

    @ Entry for 32-63 bytes (pre-subtracted by 64, so r2 = -32 to -1)
.Lcmp_bulk_32:
    adds    r2, r2, #32       @ r2 = 0-31 remaining after one 32-byte block
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r7, r3, r7
    eor     r8, r4, r8
    eor     r9, r5, r9
    eor     r12, r6, r12
    orr     r3, r7, r8
    orr     r4, r9, r12
    orrs    r3, r3, r4
    bne     .Lcmp_bulk_diff
    cmp     r2, #0
    beq     .Lcmp_bulk_done_eq
    pop     {r4-r9}
    b       .Lcmp_words

.Lcmp_bulk_done_eq:
    pop     {r4-r9}
    mov     r0, #0
    bx      lr

    @ Computed jump for 1-7 remaining words.
    @ Each word-pair is: ldr r3,[r0],#4 / ldr r12,[r1],#4 / cmp r3,r12 /
    @                     bne .Lcmp_cj_diff
    @ = 4 instructions = 16 bytes per word.
    @ Skip (7 - n_words) pairs via: rsb r3, r3, #28; add pc, pc, r3, lsl #2
    @ When r3=0 (no whole words), rsb gives 28, add pc skips all 7 pairs
    @ and falls through to .Lcmp_tail naturally -- no explicit cmp/beq needed.
.Lcmp_words:
    bic     r3, r2, #3              @ r3 = word-aligned byte count (0-28)
    sub     r2, r2, r3              @ r2 = tail bytes (0-3)
    rsb     r3, r3, #28            @ r3 = (7 - n_words) * 4
    add     pc, pc, r3, lsl #2     @ jump over (7-n) pairs
    mov     r0, r0                  @ pipeline nop
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    bne     .Lcmp_cj_diff

    @ Joaobapt tail: compare 0-3 remaining bytes without branches.
.Lcmp_tail:
    movs    r3, r2, lsl #31
    bcc     .Lcmp_tail_no_half
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    cmp     r3, r12
    bne     .Lcmp_ret_r3_r12
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    cmp     r3, r12
    bne     .Lcmp_ret_r3_r12
.Lcmp_tail_no_half:
    bpl     .Lcmp_equal
    ldrb    r3, [r0]
    ldrb    r12, [r1]
    subs    r0, r3, r12
    bx      lr

    @ Byte loop: unaligned path or n <= 3
.Lcmp_bytes:
    subs    r2, r2, #1
    blt     .Lcmp_equal
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    cmp     r3, r12
    beq     .Lcmp_bytes

.Lcmp_ret_r3_r12:
    sub     r0, r3, r12
    bx      lr

.Lcmp_equal:
    mov     r0, #0
    bx      lr

    @ Computed-jump word pair found a diff: resolve from registers.
    @ r3 = s1 word, r12 = s2 word (loaded by the ldr pair that differed).
    @ XOR to find differing bytes, then extract via shift+mask.
    @ No memory re-reads needed -- all data is already in registers.
.Lcmp_cj_diff:
    eor     r2, r3, r12            @ XOR: non-zero byte = diff
    tst     r2, #0xFF
    bne     .Lcmp_extract_byte0
    tst     r2, #0xFF00
    bne     .Lcmp_extract_byte1
    tst     r2, #0x00FF0000
    bne     .Lcmp_extract_byte2
    @ Must be byte 3
    mov     r0, r3, lsr #24
    sub     r0, r0, r12, lsr #24
    bx      lr
.Lcmp_extract_byte0:
    and     r0, r3, #0xFF
    and     r12, r12, #0xFF
    sub     r0, r0, r12
    bx      lr
.Lcmp_extract_byte1:
    and     r0, r3, #0xFF00
    and     r12, r12, #0xFF00
    sub     r0, r0, r12
    mov     r0, r0, asr #8
    bx      lr
.Lcmp_extract_byte2:
    and     r0, r3, #0x00FF0000
    and     r12, r12, #0x00FF0000
    sub     r0, r0, r12
    mov     r0, r0, asr #16
    bx      lr

    @ Bulk diff handler: resolve from preserved XOR values.
    @ After restructured XOR/OR: r7=xor[0], r8=xor[1], r9=xor[2], r12=xor[3]
    @ All diff positions (first_hi/lo, second_hi/lo) enter here since they
    @ all need to back up r0/r1 by 16. The XOR values identify the word.
    @ A single shared handler replaces the previous 4 separate handlers.
.Lcmp_bulk_diff:
    sub     r0, r0, #16
    sub     r1, r1, #16
    @ Find first non-zero XOR word (first differing word)
    cmp     r7, #0
    bne     .Lcmp_bulk_found_word
    add     r0, r0, #4
    add     r1, r1, #4
    cmp     r8, #0
    movne   r7, r8
    bne     .Lcmp_bulk_found_word
    add     r0, r0, #4
    add     r1, r1, #4
    cmp     r9, #0
    movne   r7, r9
    bne     .Lcmp_bulk_found_word
    add     r0, r0, #4
    add     r1, r1, #4
    mov     r7, r12
.Lcmp_bulk_found_word:
    @ r7 = XOR of the differing word. r0/r1 point to it.
    @ Find first non-zero byte, load it, compute result.
    tst     r7, #0xFF
    bne     .Lcmp_bulk_found_byte
    tst     r7, #0xFF00
    bne     .Lcmp_bulk_byte_at_1
    tst     r7, #0x00FF0000
    bne     .Lcmp_bulk_byte_at_2
    @ Must be byte 3
    ldrb    r3, [r0, #3]
    ldrb    r12, [r1, #3]
    sub     r0, r3, r12
    pop     {r4-r9}
    bx      lr
.Lcmp_bulk_found_byte:
    ldrb    r3, [r0]
    ldrb    r12, [r1]
    sub     r0, r3, r12
    pop     {r4-r9}
    bx      lr
.Lcmp_bulk_byte_at_1:
    ldrb    r3, [r0, #1]
    ldrb    r12, [r1, #1]
    sub     r0, r3, r12
    pop     {r4-r9}
    bx      lr
.Lcmp_bulk_byte_at_2:
    ldrb    r3, [r0, #2]
    ldrb    r12, [r1, #2]
    sub     r0, r3, r12
    pop     {r4-r9}
    bx      lr
    .size __stdgba_memcmp, . - __stdgba_memcmp

@ =============================================================================
@ bcmp: equality-only comparison (0 = equal, non-zero = not equal)
@
@ Same bulk structure as memcmp but simpler: no byte-resolution on mismatch,
@ no computed-jump (words only need equal/not-equal check).
@ =============================================================================
    .global __stdgba_bcmp
    .type __stdgba_bcmp, %function
__stdgba_bcmp:
    cmp     r2, #3
    ble     .Lbcmp_bytes

    eor     r3, r0, r1
    tst     r3, #3
    bne     .Lbcmp_bytes

    @ Fast path: skip alignment when already word-aligned
    tst     r0, #3
    beq     .Lbcmp_aligned

    @ Joaobapt alignment
    rsbs    r3, r0, #4
    movs    r3, r3, lsl #31
    bpl     .Lbcmp_align_no_byte
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    sub     r2, r2, #1
    cmp     r3, r12
    bne     .Lbcmp_ne
.Lbcmp_align_no_byte:
    bcc     .Lbcmp_aligned
    ldrh    r3, [r0], #2
    ldrh    r12, [r1], #2
    sub     r2, r2, #2
    cmp     r3, r12
    bne     .Lbcmp_ne

.Lbcmp_aligned:
    @ Bulk path: 64 bytes/iteration (same restructure as memcmp)
    cmp     r2, #32
    blt     .Lbcmp_words

    push    {r4-r9}

    subs    r2, r2, #64
    blt     .Lbcmp_bulk_32

.Lbcmp_bulk:
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    subs    r2, r2, #64
    bge     .Lbcmp_bulk

    @ Remainder: 0-63 bytes left
    adds    r2, r2, #64
    beq     .Lbcmp_bulk_done_eq
    cmp     r2, #32
    blt     .Lbcmp_bulk_done
    @ One more 32-byte block
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    sub     r2, r2, #32

.Lbcmp_bulk_done:
    pop     {r4-r9}
    cmp     r2, #0
    beq     .Lbcmp_equal
    b       .Lbcmp_words

.Lbcmp_bulk_32:
    adds    r2, r2, #32
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    ldmia   r0!, {r3-r6}
    ldmia   r1!, {r7-r9, r12}
    eor     r3, r3, r7
    eor     r4, r4, r8
    orr     r3, r3, r4
    eor     r5, r5, r9
    orr     r3, r3, r5
    eor     r6, r6, r12
    orrs    r3, r3, r6
    bne     .Lbcmp_bulk_ne
    cmp     r2, #0
    beq     .Lbcmp_bulk_done_eq
    pop     {r4-r9}
    b       .Lbcmp_words

.Lbcmp_bulk_done_eq:
    pop     {r4-r9}
    mov     r0, #0
    bx      lr

    @ Word-at-a-time (bcmp: no computed jump needed, loop is fine)
.Lbcmp_words:
    subs    r2, r2, #4
    blt     .Lbcmp_words_done
    ldr     r3, [r0], #4
    ldr     r12, [r1], #4
    cmp     r3, r12
    beq     .Lbcmp_words
    mov     r0, #1
    bx      lr

.Lbcmp_words_done:
    adds    r2, r2, #4
    beq     .Lbcmp_equal

.Lbcmp_bytes:
    subs    r2, r2, #1
    blt     .Lbcmp_equal
    ldrb    r3, [r0], #1
    ldrb    r12, [r1], #1
    cmp     r3, r12
    beq     .Lbcmp_bytes
    mov     r0, #1
    bx      lr

.Lbcmp_bulk_ne:
    pop     {r4-r9}
.Lbcmp_ne:
    mov     r0, #1
    bx      lr


.Lbcmp_equal:
    mov     r0, #0
    bx      lr
    .size __stdgba_bcmp, . - __stdgba_bcmp
