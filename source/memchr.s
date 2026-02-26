@===============================================================================
@
@ Optimized memchr for ARM7TDMI (GBA)
@
@ Provides:
@   memchr - C standard memory byte search
@
@ Runs from ROM in Thumb mode.
@
@ Algorithm: broadcast the search byte to all 4 positions, align the
@ pointer, then word-at-a-time: XOR word with broadcast, apply null-byte
@ detection to the XOR result. A zero byte in the XOR means a match.
@
@ Register plan (hot loop):
@   r0 = current pointer (advancing)
@   r1 = remaining byte count
@   r2 = broadcast search byte (4 copies)
@   r3 = 0x01010101
@   r4 = scratch / loaded word
@   r5 = 0x80808080
@   r6 = scratch
@   r12 = original search byte (preserved across word loop)
@
@===============================================================================
    .syntax unified
    .thumb
    .align 1
    .section .text._stdgba_memchr, "ax", %progbits

@ =============================================================================
@ memchr: find first occurrence of byte in memory
@
@ Input:  r0 = ptr, r1 = byte value (int), r2 = count
@ Output: r0 = pointer to found byte, or NULL
@ =============================================================================
    .global memchr
    .thumb_func
    .type memchr, %function
memchr:
    push    {r4-r6, lr}

    @ Mask search byte to 8 bits
    movs    r3, #0xFF
    ands    r1, r3

    @ Quick check for count == 0
    cmp     r2, #0
    beq     .Lmc_not_found

    @ Save search byte in r12 for later recovery
    mov     r12, r1

    @ Byte scan until aligned or count < 4
    movs    r3, #3
    tst     r0, r3
    beq     .Lmc_check_word

.Lmc_align_byte:
    ldrb    r3, [r0]
    cmp     r3, r1
    beq     .Lmc_found
    adds    r0, #1
    subs    r2, #1
    beq     .Lmc_not_found
    movs    r3, #3
    tst     r0, r3
    bne     .Lmc_align_byte

.Lmc_check_word:
    cmp     r2, #3
    ble     .Lmc_tail

    @ Broadcast byte to all 4 positions: r2 = byte * 0x01010101
    movs    r3, r1              @ r3 = byte
    lsls    r4, r1, #8
    orrs    r3, r4              @ r3 = byte | (byte << 8)
    lsls    r4, r3, #16
    orrs    r3, r4              @ r3 = broadcast

    @ Rearrange: r1 = count, r2 = broadcast
    movs    r1, r2              @ r1 = count
    movs    r2, r3              @ r2 = broadcast

    ldr     r3, .Lmc_magic01
    ldr     r5, .Lmc_magic80

    @ Word-at-a-time loop
    @ XOR word with broadcast: zero byte in result means match
    @ Detect: (xor - 0x01010101) & ~xor & 0x80808080
.Lmc_word_loop:
    ldr     r4, [r0]            @ w
    eors    r4, r2              @ r4 = w ^ broadcast
    movs    r6, r4              @ r6 = xor copy
    subs    r4, r4, r3          @ r4 = xor - 0x01010101
    mvns    r6, r6              @ r6 = ~xor
    ands    r4, r6              @ r4 = (xor - 0x01010101) & ~xor
    ands    r4, r5              @ r4 = ... & 0x80808080
    bne     .Lmc_found_word
    adds    r0, #4
    subs    r1, #4
    cmp     r1, #3
    bgt     .Lmc_word_loop

    @ Transition to tail: restore r2 = count, r1 = byte
    movs    r2, r1              @ r2 = remaining count
    mov     r1, r12             @ r1 = original byte

.Lmc_tail:
    cmp     r2, #0
    beq     .Lmc_not_found
    ldrb    r3, [r0]
    cmp     r3, r1
    beq     .Lmc_found
    adds    r0, #1
    subs    r2, #1
    b       .Lmc_tail

.Lmc_found_word:
    @ Byte scan within word to find exact match position
    @ Recover search byte from r12
    mov     r3, r12
    ldrb    r4, [r0]
    cmp     r4, r3
    beq     .Lmc_found
    adds    r0, #1
    ldrb    r4, [r0]
    cmp     r4, r3
    beq     .Lmc_found
    adds    r0, #1
    ldrb    r4, [r0]
    cmp     r4, r3
    beq     .Lmc_found
    adds    r0, #1              @ must be byte 3

.Lmc_found:
    pop     {r4-r6, pc}

.Lmc_not_found:
    movs    r0, #0
    pop     {r4-r6, pc}

    .align 2
.Lmc_magic01:
    .word   0x01010101
.Lmc_magic80:
    .word   0x80808080
    .size memchr, . - memchr
