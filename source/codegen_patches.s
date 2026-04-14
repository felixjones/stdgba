@===============================================================================
@
@ Fast ARM patch application loops for gba::codegen
@
@ Provides:
@   _stdgba_apply_patches_imm8  - tight loop for all-imm8 patch lists
@   _stdgba_apply_patches_core  - general loop: imm8, signed12, branch, instr
@
@ Both functions run from IWRAM in 32-bit ARM mode for single-cycle instruction
@ fetch, making them suitable for calling from IWRAM HBlank ISR handlers.
@
@ patch_slot memory layout (8 bytes, little-endian):
@   byte 0 : word_index   (uint8_t)  -- index into destination array
@   byte 1 : arg_index    (uint8_t)  -- index into args array
@   byte 2 : kind         (uint8_t)  -- 0=imm8, 1=signed12, 2=branch_offset, 3=instruction
@   byte 3 : padding      (uint8_t)
@   bytes 4-7: base_word  (uint32_t) -- precomputed base instruction word
@
@ AAPCS ARM calling convention:
@   r0 = uint32_t*       destination  -- installed code buffer
@   r1 = patch_slot*     patches      -- patch descriptor array
@   r2 = uint32_t        patch_count  -- number of patches (> 0)
@   r3 = const uint32_t* args         -- patch argument values
@
@===============================================================================
    .syntax unified
    .arm
    .align 2

@-------------------------------------------------------------------------------
@ _stdgba_apply_patches_imm8
@
@   Fast path for patch lists where all patches have kind == imm8 (0).
@   No per-patch kind dispatch; writes: dest[word_index] = base_word | args[arg_index]
@
@   Uses ldmia to load both words of each patch_slot in one instruction:
@     low word  -> r4: {padding[31:24], kind[23:16], arg_index[15:8], word_index[7:0]}
@     high word -> r5: base_word
@
@   Caller guarantees: patch_count > 0, all patches have kind == 0 (imm8).
@   Validation and range checking is done in the C++ apply_patches wrapper.
@
@   Clobbers: r4, r5, r6 (callee-saved via stmdb/ldmia)
@-------------------------------------------------------------------------------
    .section .iwram._stdgba_apply_patches_imm8, "ax", %progbits
    .global _stdgba_apply_patches_imm8
    .type   _stdgba_apply_patches_imm8, %function
_stdgba_apply_patches_imm8:
    @ r0=dest, r1=patches, r2=count, r3=args
    stmdb   sp!, {r4, r5, r6, r7, lr}
    tst     r2, #1
    beq     .Limm8_pair_loop

    @ Odd patch count: handle one patch so the main loop can consume pairs.
    ldmia   r1!, {r4, r5}           @ r4={pad,kind,arg_idx,word_idx}, r5=base_word
    and     r6, r4, #0xFF           @ r6 = word_index
    lsr     r4, r4, #8
    and     r4, r4, #0xFF           @ r4 = arg_index
    ldr     r4, [r3, r4, lsl #2]    @ r4 = args[arg_index]
    orr     r4, r5, r4
    str     r4, [r0, r6, lsl #2]
    subs    r2, r2, #1
    beq     .Limm8_done

.Limm8_pair_loop:
    ldmia   r1!, {r4, r5, r6, ip}   @ {patch0.low, patch0.base, patch1.low, patch1.base}

    and     r7, r4, #0xFF           @ r7 = patch0.word_index
    lsr     lr, r4, #8
    and     r4, lr, #0xFF           @ r4 = patch0.arg_index
    ldr     r4, [r3, r4, lsl #2]
    orr     r4, r5, r4
    str     r4, [r0, r7, lsl #2]

    and     r7, r6, #0xFF           @ r7 = patch1.word_index
    lsr     lr, r6, #8
    and     r6, lr, #0xFF           @ r6 = patch1.arg_index
    ldr     r6, [r3, r6, lsl #2]
    orr     r6, ip, r6
    str     r6, [r0, r7, lsl #2]

    subs    r2, r2, #2
    bne     .Limm8_pair_loop

.Limm8_done:
    ldmia   sp!, {r4, r5, r6, r7, lr}
    bx      lr

@-------------------------------------------------------------------------------
@ _stdgba_apply_patches_core
@
@   General patch loop handling all patch_kind values:
@     0 (imm8)          : dest[word_idx] = base_word | value
@     1 (signed12)      : dest[word_idx] = base_word | u_bit | abs(value)[11:0]
@                         u_bit = (1 << 23) when value >= 0; 0 when negative
@     2 (branch_offset) : dest[word_idx] = base_word | (value & 0x00FFFFFF)
@     3 (instruction)   : dest[word_idx] = value  (full instruction replacement)
@
@   Caller guarantees: patch_count > 0.
@   Validation and range checking is done in the C++ apply_patches wrapper.
@
@   Clobbers: r4-r8 (callee-saved via stmdb/ldmia)
@-------------------------------------------------------------------------------
    .section .iwram._stdgba_apply_patches_core, "ax", %progbits
    .global _stdgba_apply_patches_core
    .type   _stdgba_apply_patches_core, %function
_stdgba_apply_patches_core:
    @ r0=dest, r1=patches, r2=count, r3=args
    stmdb   sp!, {r4, r5, lr}
.Lcore_loop:
    ldmia   r1!, {r4, ip}           @ r4={pad,kind,arg_idx,word_idx}, ip=base_word
    and     r5, r4, #0xFF           @ r5 = word_index
    lsr     lr, r4, #8
    and     r4, lr, #0xFF           @ r4 = arg_index
    lsr     lr, lr, #8
    and     lr, lr, #0xFF           @ lr = kind
    ldr     r4, [r3, r4, lsl #2]    @ r4 = args[arg_index]

    @ Dispatch on kind. imm8 is encoded in the fallthrough path.
    cmp     lr, #1
    beq     .Lcore_signed12
    cmp     lr, #2
    beq     .Lcore_branch
    cmp     lr, #3
    beq     .Lcore_store            @ instruction: store r4 (= value) directly
    orr     r4, ip, r4              @ imm8: base_word | value
    b       .Lcore_store

.Lcore_branch:
    bic     r4, r4, #0xFF000000     @ keep only the low 24 branch bits
    orr     r4, ip, r4
    b       .Lcore_store

.Lcore_signed12:
    @ Treat r4 as signed int32_t.
    @ If negative: magnitude = -r8, U bit = 0.
    @ If non-negative: magnitude = r8, U bit = (1 << 23).
    @ dest[wi] = base_word | u_bit | magnitude[11:0]
    mov     lr, r4, asr #31         @ lr = sign mask (0 or -1)
    eor     r4, r4, lr
    sub     r4, r4, lr              @ r4 = abs(value)
    lsl     r4, r4, #20             @ isolate lower 12 bits via paired shifts
    lsr     r4, r4, #20
    mvn     lr, lr                  @ lr = ~sign mask (all ones for non-negative)
    and     lr, lr, #0x800000       @ lr = U bit when value >= 0, else 0
    orr     r4, ip, r4
    orr     r4, r4, lr              @ result = base_word | u_bit | magnitude

.Lcore_store:
    str     r4, [r0, r5, lsl #2]    @ destination[word_index] = r4
    subs    r2, r2, #1
    bne     .Lcore_loop
    ldmia   sp!, {r4, r5, lr}
    bx      lr
