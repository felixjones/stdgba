@===============================================================================
@
@ Optimized strchr / strrchr for ARM7TDMI (GBA)
@
@ Provides:
@   strchr  - find first occurrence of char in string
@   strrchr - find last occurrence of char in string
@
@ Runs from ROM in Thumb mode.
@
@===============================================================================
    .syntax unified
    .thumb
    .align 1
    .section .text._stdgba_strchr, "ax", %progbits

@ =============================================================================
@ strchr: find first occurrence of byte in null-terminated string
@
@ Input:  r0 = string, r1 = character (int, only low 8 bits used)
@ Output: r0 = pointer to found char, or NULL if not found
@ Note:   searching for '\0' returns pointer to the terminator
@ =============================================================================
    .global strchr
    .thumb_func
    .type strchr, %function
strchr:
    @ Mask to byte
    movs    r2, #0xFF
    ands    r1, r2

    @ Simple byte loop -- strings are typically short on GBA
    @ and the overhead of word-at-a-time with TWO detections
    @ (null + target) rarely pays off for typical GBA string lengths.
.Lschr_loop:
    ldrb    r2, [r0]
    cmp     r2, r1
    beq     .Lschr_found
    cmp     r2, #0
    beq     .Lschr_not_found
    adds    r0, #1
    b       .Lschr_loop

.Lschr_found:
    bx      lr

.Lschr_not_found:
    movs    r0, #0
    bx      lr
    .size strchr, . - strchr

@ =============================================================================
@ strrchr: find last occurrence of byte in null-terminated string
@
@ Input:  r0 = string, r1 = character (int, only low 8 bits used)
@ Output: r0 = pointer to last occurrence, or NULL if not found
@ Note:   searching for '\0' returns pointer to the terminator
@ =============================================================================
    .global strrchr
    .thumb_func
    .type strrchr, %function
strrchr:
    @ Mask to byte
    movs    r3, #0xFF
    ands    r1, r3

    movs    r3, #0              @ r3 = last found (NULL initially)

.Lsrchr_loop:
    ldrb    r2, [r0]
    cmp     r2, r1
    bne     .Lsrchr_no_match
    movs    r3, r0              @ update last found
.Lsrchr_no_match:
    cmp     r2, #0
    beq     .Lsrchr_done
    adds    r0, #1
    b       .Lsrchr_loop

.Lsrchr_done:
    movs    r0, r3              @ return last found (or NULL)
    bx      lr
    .size strrchr, . - strrchr
