@===============================================================================
@
@ Flash memory helper routines for ARM7TDMI (GBA)
@
@ Provides:
@   flash_write_byte_routine   - program one byte
@   flash_erase_sector_routine - erase one 4 KB sector
@   flash_erase_chip_routine   - erase the full chip
@   flash_switch_bank_routine  - switch 128 KB flash bank
@   flash_read_id_routine      - read manufacturer/device ID
@   flash_memcpy_routine       - byte copy helper for flash reads
@   flash_write_atmel_routine  - program one 128-byte Atmel page
@
@ These routines are position-independent and intended to be copied to RAM
@ before execution. Flash and ROM share the same bus, so flash command code
@ cannot safely execute from ROM while programming/erasing.
@
@ Each routine exposes matching _end and _size symbols for runtime copying.
@
@===============================================================================

    .arm
    .section .rodata.flash, "a", %progbits

@ =============================================================================
@ flash_write_byte_routine - Write single byte to Flash
@ Input: r0 = address offset, r1 = data byte
@ Output: r0 = 0 on success, non-zero on timeout
@ =============================================================================
    .global flash_write_byte_routine
    .global flash_write_byte_routine_end
    .align 4
flash_write_byte_routine:
    push    {r4, r5, r6, lr}
    mov     r4, r1                      @ save data byte
    ldr     r5, .Lwb_5555
    ldr     r6, .Lwb_2aaa
    ldr     r2, .Lwb_base
    @ Write command sequence
    mov     r3, #0xAA
    strb    r3, [r5]
    mov     r3, #0x55
    strb    r3, [r6]
    mov     r3, #0xA0                   @ CMD_WRITE
    strb    r3, [r5]
    @ Write data
    add     r2, r2, r0
    strb    r4, [r2]
    @ Poll for completion
    mov     r3, #0x100000
.Lwb_poll:
    ldrb    r1, [r2]
    cmp     r1, r4
    beq     .Lwb_done
    subs    r3, r3, #1
    bne     .Lwb_poll
    mov     r0, #1
    pop     {r4, r5, r6, pc}
.Lwb_done:
    mov     r0, #0
    pop     {r4, r5, r6, pc}
.Lwb_5555:
    .word   0x0E005555
.Lwb_2aaa:
    .word   0x0E002AAA
.Lwb_base:
    .word   0x0E000000
flash_write_byte_routine_end:
    .global flash_write_byte_routine_size
    .align 2
flash_write_byte_routine_size:
    .int flash_write_byte_routine_end - flash_write_byte_routine

@ =============================================================================
@ flash_erase_sector_routine - Erase 4KB sector
@ Input: r0 = sector address offset (will be aligned)
@ Output: r0 = 0 on success, non-zero on timeout
@ =============================================================================
    .global flash_erase_sector_routine
    .global flash_erase_sector_routine_end
    .align 4
flash_erase_sector_routine:
    push    {r4, r5, r6, lr}
    ldr     r1, .Les_mask
    and     r4, r0, r1
    ldr     r5, .Les_5555
    ldr     r6, .Les_2aaa
    ldr     r1, .Les_base
    add     r4, r1, r4
    @ Erase command sequence
    mov     r2, #0xAA
    strb    r2, [r5]
    mov     r2, #0x55
    strb    r2, [r6]
    mov     r2, #0x80                   @ CMD_ERASE
    strb    r2, [r5]
    mov     r2, #0xAA
    strb    r2, [r5]
    mov     r2, #0x55
    strb    r2, [r6]
    mov     r2, #0x30                   @ CMD_ERASE_SECTOR
    strb    r2, [r4]
    @ Poll for completion
    mov     r3, #0x100000
.Les_poll:
    ldrb    r2, [r4]
    cmp     r2, #0xFF
    beq     .Les_done
    cmp     r2, #0x00
    beq     .Les_done
    subs    r3, r3, #1
    bne     .Les_poll
    mov     r0, #1
    pop     {r4, r5, r6, pc}
.Les_done:
    mov     r0, #0
    pop     {r4, r5, r6, pc}
.Les_5555:
    .word   0x0E005555
.Les_2aaa:
    .word   0x0E002AAA
.Les_base:
    .word   0x0E000000
.Les_mask:
    .word   0xFFFFF000
flash_erase_sector_routine_end:
    .global flash_erase_sector_routine_size
    .align 2
flash_erase_sector_routine_size:
    .int flash_erase_sector_routine_end - flash_erase_sector_routine

@ =============================================================================
@ flash_erase_chip_routine - Erase entire chip
@ Output: r0 = 0 on success, non-zero on timeout
@ =============================================================================
    .global flash_erase_chip_routine
    .global flash_erase_chip_routine_end
    .align 4
flash_erase_chip_routine:
    push    {r4, r5, lr}
    ldr     r4, .Lec_5555
    ldr     r5, .Lec_2aaa
    ldr     r1, .Lec_base
    @ Erase command sequence
    mov     r2, #0xAA
    strb    r2, [r4]
    mov     r2, #0x55
    strb    r2, [r5]
    mov     r2, #0x80
    strb    r2, [r4]
    mov     r2, #0xAA
    strb    r2, [r4]
    mov     r2, #0x55
    strb    r2, [r5]
    mov     r2, #0x10                   @ CMD_ERASE_CHIP
    strb    r2, [r4]
    @ Poll for completion
    mov     r3, #0x100000
.Lec_poll:
    ldrb    r2, [r1]
    cmp     r2, #0xFF
    beq     .Lec_done
    cmp     r2, #0x00
    beq     .Lec_done
    subs    r3, r3, #1
    bne     .Lec_poll
    mov     r0, #1
    pop     {r4, r5, pc}
.Lec_done:
    mov     r0, #0
    pop     {r4, r5, pc}
.Lec_5555:
    .word   0x0E005555
.Lec_2aaa:
    .word   0x0E002AAA
.Lec_base:
    .word   0x0E000000
flash_erase_chip_routine_end:
    .global flash_erase_chip_routine_size
    .align 2
flash_erase_chip_routine_size:
    .int flash_erase_chip_routine_end - flash_erase_chip_routine

@ =============================================================================
@ flash_switch_bank_routine - Switch bank (128KB chips)
@ Input: r0 = bank number (0 or 1)
@ =============================================================================
    .global flash_switch_bank_routine
    .global flash_switch_bank_routine_end
    .align 4
flash_switch_bank_routine:
    ldr     r1, .Lsb_5555
    ldr     r2, .Lsb_2aaa
    ldr     r3, .Lsb_base
    mov     r12, #0xAA
    strb    r12, [r1]
    mov     r12, #0x55
    strb    r12, [r2]
    mov     r12, #0xB0                  @ CMD_SWITCH_BANK
    strb    r12, [r1]
    strb    r0, [r3]
    bx      lr
.Lsb_5555:
    .word   0x0E005555
.Lsb_2aaa:
    .word   0x0E002AAA
.Lsb_base:
    .word   0x0E000000
flash_switch_bank_routine_end:
    .global flash_switch_bank_routine_size
    .align 2
flash_switch_bank_routine_size:
    .int flash_switch_bank_routine_end - flash_switch_bank_routine

@ =============================================================================
@ flash_read_id_routine - Read chip ID
@ Output: r0 = manufacturer (low byte) | device (high byte)
@ =============================================================================
    .global flash_read_id_routine
    .global flash_read_id_routine_end
    .align 4
flash_read_id_routine:
    push    {r4, lr}
    ldr     r1, .Lid_5555
    ldr     r2, .Lid_2aaa
    ldr     r4, .Lid_base
    @ Enter ID mode
    mov     r3, #0xAA
    strb    r3, [r1]
    mov     r3, #0x55
    strb    r3, [r2]
    mov     r3, #0x90                   @ CMD_ENTER_ID
    strb    r3, [r1]
    @ Delay
    mov     r3, #0x10000
.Lid_delay1:
    subs    r3, r3, #1
    bne     .Lid_delay1
    @ Read IDs
    ldrb    r0, [r4]
    ldrb    r3, [r4, #1]
    orr     r0, r0, r3, lsl #8
    @ Exit ID mode
    ldr     r1, .Lid_5555
    ldr     r2, .Lid_2aaa
    mov     r3, #0xAA
    strb    r3, [r1]
    mov     r3, #0x55
    strb    r3, [r2]
    mov     r3, #0xF0                   @ CMD_LEAVE_ID
    strb    r3, [r1]
    @ Delay
    mov     r3, #0x10000
.Lid_delay2:
    subs    r3, r3, #1
    bne     .Lid_delay2
    pop     {r4, pc}
.Lid_5555:
    .word   0x0E005555
.Lid_2aaa:
    .word   0x0E002AAA
.Lid_base:
    .word   0x0E000000
flash_read_id_routine_end:
    .global flash_read_id_routine_size
    .align 2
flash_read_id_routine_size:
    .int flash_read_id_routine_end - flash_read_id_routine

@ =============================================================================
@ flash_memcpy_routine - Byte copy (for reading from flash)
@ Input: r0 = dst, r1 = src, r2 = count
@ =============================================================================
    .global flash_memcpy_routine
    .global flash_memcpy_routine_end
    .align 4
flash_memcpy_routine:
    cmp     r2, #0
    bxeq    lr
.Lmc_loop:
    ldrb    r3, [r1], #1
    strb    r3, [r0], #1
    subs    r2, r2, #1
    bne     .Lmc_loop
    bx      lr
flash_memcpy_routine_end:
    .global flash_memcpy_routine_size
    .align 2
flash_memcpy_routine_size:
    .int flash_memcpy_routine_end - flash_memcpy_routine

@ =============================================================================
@ flash_write_atmel_routine - Write 128-byte sector (Atmel chips)
@ Input: r0 = address offset, r1 = data pointer
@ Output: r0 = 0 on success, non-zero on timeout
@ =============================================================================
    .global flash_write_atmel_routine
    .global flash_write_atmel_routine_end
    .align 4
flash_write_atmel_routine:
    push    {r4, r5, r6, r7, r8, lr}
    mov     r4, r0
    mov     r5, r1
    ldr     r6, .Lwa_5555
    ldr     r7, .Lwa_2aaa
    ldr     r8, .Lwa_base
    @ Write command
    mov     r2, #0xAA
    strb    r2, [r6]
    mov     r2, #0x55
    strb    r2, [r7]
    mov     r2, #0xA0
    strb    r2, [r6]
    @ Write 128 bytes
    add     r4, r8, r4
    mov     r3, #128
.Lwa_write:
    ldrb    r2, [r5], #1
    strb    r2, [r4], #1
    subs    r3, r3, #1
    bne     .Lwa_write
    @ Poll last byte
    sub     r4, r4, #1
    sub     r5, r5, #1
    ldrb    r1, [r5]
    mov     r3, #0x100000
.Lwa_poll:
    ldrb    r2, [r4]
    cmp     r2, r1
    beq     .Lwa_done
    subs    r3, r3, #1
    bne     .Lwa_poll
    mov     r0, #1
    pop     {r4, r5, r6, r7, r8, pc}
.Lwa_done:
    mov     r0, #0
    pop     {r4, r5, r6, r7, r8, pc}
.Lwa_5555:
    .word   0x0E005555
.Lwa_2aaa:
    .word   0x0E002AAA
.Lwa_base:
    .word   0x0E000000
flash_write_atmel_routine_end:
    .global flash_write_atmel_routine_size
    .align 2
flash_write_atmel_routine_size:
    .int flash_write_atmel_routine_end - flash_write_atmel_routine
