@===============================================================================
@
@ Scoped interrupt masking primitives for ARM7TDMI (GBA)
@
@ Provides:
@   _stdgba_enter_critical_section - disable IRQs and return the previous state
@   _stdgba_exit_critical_section  - restore the previous IRQ state
@
@ Must run in ARM state to access CPSR directly.
@
@===============================================================================
    .syntax unified
    .arm
    .align 2
    .section .text._stdgba_critical_section, "ax", %progbits

@ =============================================================================
@ _stdgba_enter_critical_section
@
@ Input:  none
@ Output: r0 = previous CPSR IRQ disable bit
@ Clobbers: r1
@ =============================================================================
    .global _stdgba_enter_critical_section
    .type _stdgba_enter_critical_section, %function
_stdgba_enter_critical_section:
    mrs     r0, cpsr
    orr     r1, r0, #0x80
    msr     cpsr_c, r1
    and     r0, r0, #0x80
    bx      lr
    .size _stdgba_enter_critical_section, . - _stdgba_enter_critical_section

@ =============================================================================
@ _stdgba_exit_critical_section
@
@ Input:  r0 = previous CPSR IRQ disable bit
@ Output: none
@ Clobbers: r0, r1
@ =============================================================================
    .global _stdgba_exit_critical_section
    .type _stdgba_exit_critical_section, %function
_stdgba_exit_critical_section:
    mrs     r1, cpsr
    bic     r1, r1, #0x80
    orr     r1, r1, r0
    msr     cpsr_c, r1
    bx      lr
    .size _stdgba_exit_critical_section, . - _stdgba_exit_critical_section
