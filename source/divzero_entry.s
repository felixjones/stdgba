@===============================================================================
@
@ Division-by-zero handler entry bridge for ARM7TDMI (GBA)
@
@ Provides:
@   __aeabi_idiv0 - 32-bit division-by-zero trap entry
@   __aeabi_ldiv0 - 64-bit division-by-zero trap entry
@
@ Overrides libgcc weak symbols, captures machine state, then tail-calls
@ the C++ diagnostic renderer. Must run in ARM state to read CPSR.
@
@ Division by zero always traps (independent of NDEBUG).
@
@ Entry contract from libgcc tail-call:
@   r0-r12 = register state at fault site
@   lr     = caller return address
@   sp     = caller stack pointer
@
@===============================================================================

    .section .text._stdgba_divzero, "ax", %progbits
    .arm
    .align 2

@ =============================================================================
@ __aeabi_ldiv0 / __aeabi_idiv0
@
@ Input:  libgcc fault context in r0-r12/lr/sp (see file banner)
@ Output: none (tail-calls renderer; does not return)
@ Clobbers: r0-r12, sp
@ =============================================================================
    .global __aeabi_idiv0
    .global __aeabi_ldiv0
    .type __aeabi_idiv0, %function
    .type __aeabi_ldiv0, %function

__aeabi_ldiv0:
    str     lr, [sp, #-4]!      @ save original LR to caller's stack
    mov     lr, #1               @ flag: 64-bit division (ldiv0)
    b       .Ldivzero_common

__aeabi_idiv0:
    str     lr, [sp, #-4]!      @ save original LR to caller's stack
    mov     lr, #0               @ flag: 32-bit division (idiv0)

.Ldivzero_common:
    @ State: r0-r12 intact, lr = is_ldiv flag, [sp] = original LR
    stmdb   sp!, {r0-r12}       @ save r0-r12 to caller's stack (52 bytes)
    @ Stack layout: [r0-r12 (52 bytes)] [original LR (4 bytes)]

    @ Recover saved values into callee-saved registers
    ldr     r4, [sp, #52]        @ r4 = original LR
    add     r5, sp, #56          @ r5 = original SP (before our pushes)
    mrs     r6, cpsr
    mov     r7, lr               @ r7 = is_ldiv flag
    mov     r3, sp               @ r3 = source pointer for r0-r12 copy

    @ r8 = IO base (0x04000000)
    mov     r8, #0x04000000

    @ Read interrupt registers before disabling IME
    @ REG_IE = 0x4000200, REG_IF = 0x4000202, REG_IME = 0x4000208
    add     r9, r8, #0x200
    ldrh    r10, [r9, #8]        @ r10 = IME
    ldrh    r11, [r9]            @ r11 = IE
    ldrh    r12, [r9, #2]        @ r12 = IF

    @ Disable IME
    strb    r9, [r9, #8]         @ Disable IME (stores 0x00)

    @ Read DISPCNT (0x4000000)
    ldrh    r9, [r8]             @ r9 = DISPCNT

    @ Set DISPCNT to Mode 3 + BG2 (0x0403)
    mov     r0, #0x0400
    orr     r0, r0, #0x03
    strh    r0, [r8]

    @ Switch to IWRAM stack (0x3008000)
    mov     sp, #0x03000000
    add     sp, sp, #0x8000

    @ Allocate divzero_state struct on IWRAM stack:
    @   struct { is_ldiv(0), r[13](4-56), sp(56), lr(60), cpsr(64),
    @            dispcnt(68), ime(70), ie(72), if_(74) }
    @   Total = 76 bytes
    sub     sp, sp, #76

    @ Write fixed fields
    str     r7, [sp, #0]         @ is_ldiv
    str     r5, [sp, #56]        @ original SP
    str     r4, [sp, #60]        @ original LR
    str     r6, [sp, #64]        @ CPSR
    orr     r9, r9, r10, lsl #16
    str     r9, [sp, #68]        @ (ime << 16) | dispcnt
    orr     r11, r11, r12, lsl #16
    str     r11, [sp, #72]       @ (if << 16) | ie

    @ Copy r0-r12 from caller's stack to struct.r[0..12]
    @ Source: r3, Dest: sp+4, Count: 13 words
    add     r0, sp, #4
    mov     r1, #13
.Lcopy_regs:
    ldr     r2, [r3], #4
    str     r2, [r0], #4
    subs    r1, r1, #1
    bne     .Lcopy_regs

    @ r0 = pointer to divzero_state
    mov     r0, sp

    @ Call thumb renderer (use bx for ARM7TDMI interworking)
    ldr     r1, =_stdgba_divzero_render
    bx      r1

    .ltorg
    .size __aeabi_ldiv0, . - __aeabi_ldiv0
    .size __aeabi_idiv0, . - __aeabi_idiv0
