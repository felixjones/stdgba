@===============================================================================
@
@ Optimized memmove for ARM7TDMI (GBA)
@
@ Provides:
@   __aeabi_memmove    - move with arbitrary alignment
@   __aeabi_memmove4   - move with 4-byte aligned pointers
@   __aeabi_memmove8   - move with 8-byte aligned pointers
@
@ C standard memmove is in memmove.cpp (separate TU for LTO inlining).
@
@ AEABI signature: __aeabi_memmove(void *dest, const void *src, size_t n)
@ Same as memcpy, but handles overlapping regions.
@
@ Strategy:
@   - If dest <= src or dest >= src + n: forward copy (tail-call to memcpy)
@   - If dest > src (overlap): backward copy from end to start
@
@ The backward path mirrors memcpy's forward path:
@   - Alignment promotion via joaobapt bit test (from the end)
@   - Bulk ldmdb/stmdb 32-byte transfers with double-pump
@   - Computed jump into unrolled word copies
@   - Joaobapt tail for remaining 0-3 bytes
@
@ Runs in IWRAM (ARM, 32-bit bus, 1-cycle access) for maximum throughput.
@
@===============================================================================
.syntax unified
    .arm
    .align 2
    .section .iwram._stdgba_memmove, "ax", %progbits

@ =============================================================================
@ Generic entry: arbitrary alignment
@
@ Input:  r0 = dest, r1 = src, r2 = byte count
@ Output: void (dest in r0 is clobbered)
@ Clobbers: r0, r1, r2, r3
@ =============================================================================
    .global __aeabi_memmove
    .type __aeabi_memmove, %function
__aeabi_memmove:
    cmp     r0, r1
    bls     __aeabi_memcpy          @ dest <= src: forward (no overlap possible)
    add     r3, r1, r2
    cmp     r0, r3
    bhs     __aeabi_memcpy          @ dest >= src + n: no overlap

    @ Backward copy needed (dest > src, regions overlap)
    @ Advance pointers to the end of the buffers
    add     r0, r0, r2              @ r0 = dest_end
    add     r1, r1, r2              @ r1 = src_end

    @ Small backward copies (<=3 bytes): byte-by-byte from end
    cmp     r2, #3
    ble     .Lrev_bytes

    @ Test alignment compatibility (same as forward memcpy)
    eor     r3, r0, r1
    tst     r3, #1
    bne     .Lrev_bytes             @ Unresolvable byte misalignment
    tst     r3, #2
    bne     .Lrev_halves_entry      @ Halfword-compatible only

    @ Both pointers share word alignment from the end
    @ Fast path: if end-pointers are already word-aligned, skip fixup.
    @ tst(1)+beq(3)=4 vs movs(1)+6 NOPs(6)+b(3)=10. Saves 6 cycles.
    tst     r0, #3
    beq     .Lrev_aligned

    @ Align to word boundary: need to copy (r0 & 3) trailing bytes
    @ joaobapt trick: movs r3, r0, lsl #31 maps bit 0 -> N, bit 1 -> C
    movs    r3, r0, lsl #31
    @ Copy 1 byte if bit 0 was set (mi)
    ldrbmi  r3, [r1, #-1]!
    strbmi  r3, [r0, #-1]!
    submi   r2, r2, #1
    @ Copy 2 bytes if bit 1 was set (cs)
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    subcs   r2, r2, #2
    @ Now word-aligned from the end: jump to backward aligned path
    b       .Lrev_aligned

@ =============================================================================
@ Word-aligned backward entry
@
@ Input:  r0 = dest_end (word-aligned), r1 = src_end (word-aligned),
@         r2 = remaining byte count
@ Output: void
@ Clobbers: r0, r1, r2, r3, r12, r4-r9 (saved/restored for bulk path)
@
@ Uses {r3-r9, r12} for 8-register ldmdb/stmdb. Same register set as
@ forward memcpy: only r4-r9 need push/pop (r3, r12 are caller-saved).
@ =============================================================================
    .global __aeabi_memmove8
    .type __aeabi_memmove8, %function
__aeabi_memmove8:
    .global __aeabi_memmove4
    .type __aeabi_memmove4, %function
__aeabi_memmove4:
    cmp     r0, r1
    bls     __aeabi_memcpy4         @ dest <= src: forward copy
    add     r3, r1, r2
    cmp     r0, r3
    bhs     __aeabi_memcpy4         @ dest >= src + n: no overlap

    @ Backward copy needed -- advance to end
    add     r0, r0, r2
    add     r1, r1, r2

    @ The original pointers are word-aligned, but after adding n the end
    @ pointers may not be. Copy (n & 3) tail bytes to re-align before
    @ entering the word-aligned backward bulk path.
    @ Fast path: if n is a multiple of 4, end-pointers are already aligned.
    tst     r2, #3
    beq     .Lrev_aligned
    movs    r3, r2, lsl #31         @ joaobapt: bit0 -> N, bit1 -> C
    ldrbmi  r3, [r1, #-1]!
    strbmi  r3, [r0, #-1]!
    submi   r2, r2, #1
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    subcs   r2, r2, #2

.Lrev_aligned:
    cmp     r2, #32
    blt     .Lrev_words

    push    {r4-r9}
    @ GE is set from cmp (r2 >= 32): first subsge always executes
.Lrev_bulk:
    @ First 32-byte block
    subsge  r2, r2, #32
    ldmdbge r1!, {r3-r9, r12}
    stmdbge r0!, {r3-r9, r12}
    @ Second 32-byte block (skipped if first went negative)
    subsge  r2, r2, #32
    ldmdbge r1!, {r3-r9, r12}
    stmdbge r0!, {r3-r9, r12}
    bgt     .Lrev_bulk
    pop     {r4-r9}
    bxeq    lr                      @ exact multiple of 64
    adds    r2, r2, #32
    bxeq    lr                      @ exact multiple of 32

.Lrev_words:
    @ Computed jump into unrolled backward word copies
    @ Same technique as forward memcpy: no bics/beq/sub guard needed.
    @ The computed jump naturally skips all 7 pairs when r2 < 4,
    @ and the joaobapt tail only reads r2's low 2 bits.
    bic     r3, r2, #3              @ r3 = word bytes = r2 & ~3
    rsb     r3, r3, #28             @ r3 = (7 - n_words) * 4
    add     pc, pc, r3, lsl #1      @ skip (7 - n_words) pairs
    mov     r0, r0                  @ ARM7 pipeline nop
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    ldr     r3, [r1, #-4]!
    str     r3, [r0, #-4]!
    @ Fall through to tail

.Lrev_tail:
    @ Copy remaining 0-3 bytes from end using joaobapt bit test
    @ bit 1 -> halfword (cs), bit 0 -> byte (mi)
    movs    r3, r2, lsl #31
    ldrhcs  r3, [r1, #-2]!
    strhcs  r3, [r0, #-2]!
    ldrbmi  r3, [r1, #-1]
    strbmi  r3, [r0, #-1]
    bx      lr

@ Backward halfword copy: halfword-compatible but not word-compatible
.Lrev_halves_entry:
    @ Align to halfword boundary from end if needed
    tst     r0, #1
    ldrbne  r3, [r1, #-1]!
    strbne  r3, [r0, #-1]!
    subne   r2, r2, #1
.Lrev_halves:
    subs    r2, r2, #2
    ldrhge  r3, [r1, #-2]!
    strhge  r3, [r0, #-2]!
    bgt     .Lrev_halves
    bxeq    lr
    @ One trailing byte at the start
    adds    r2, r2, #2
    ldrbne  r3, [r1, #-1]
    strbne  r3, [r0, #-1]
    bx      lr

@ Backward byte copy: unresolvable alignment or small transfer
.Lrev_bytes:
    subs    r2, r2, #1
    ldrbge  r3, [r1, #-1]!
    strbge  r3, [r0, #-1]!
    bgt     .Lrev_bytes
    bx      lr
    .size __aeabi_memmove, . - __aeabi_memmove
