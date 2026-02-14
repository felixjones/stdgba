#include <gba/interrupt>

#include <gba/peripherals>

static gba::handler<gba::irq> user_handler;

[[gnu::target("arm"), gnu::section(".iwram._stdgba_irq_user_handler"), gnu::naked]]
void irq_user_handler();

[[gnu::target("arm"), gnu::section(".iwram._stdgba_irq_empty_handler"), gnu::naked]]
void irq_empty_handler();

namespace gba {

    const isr& isr::operator=(const value_type& value) const noexcept { // NOLINT(*-unconventional-assign-operator)
        if (value == nullisr) {
            registral<void(*)()>{0x3007FFC} = irq_empty_handler;
        } else {
            user_handler = value;
            registral<void(*)()>{0x3007FFC} = irq_user_handler;
        }
        return *this;
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    bool isr::has_value() const noexcept { // NOLINT(*-convert-member-functions-to-static)
        return registral<void(*)()>{0x3007FFC} != irq_empty_handler; // Has any value (including user modified)
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    const isr::value_type& isr::value() const { // NOLINT(*-convert-member-functions-to-static)
        if (registral<void(*)()>{0x3007FFC} != irq_user_handler) [[unlikely]] {
            return nullisr;
        }
        return user_handler;
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void isr::swap(value_type& value) const noexcept { // NOLINT(*-convert-member-functions-to-static)
        const auto prev = handler{this->value()}; // Copy previous

        if (value == nullisr) {
            registral<void(*)()>{0x3007FFC} = irq_empty_handler;
        } else {
            user_handler = value;
            registral<void(*)()>{0x3007FFC} = irq_user_handler;
        }

        value = prev; // Set to previous
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void isr::reset() const noexcept { // NOLINT(*-convert-member-functions-to-static)
        registral<void(*)()>{0x3007FFC} = irq_empty_handler;
    }

    bool isr::operator==(const value_type& value) const noexcept {
        if (registral<void (*)()>{0x3007FFC} == irq_empty_handler) {
            return value == nullisr;
        }
        if (registral<void (*)()>{0x3007FFC} != irq_user_handler) [[unlikely]] {
            return false;
        }
        return this->value() == value;
    }

} // namespace gba

void irq_user_handler() {
    static constexpr auto call_op = &decltype(user_handler)::operator();
    // ReSharper disable once CppDeprecatedRegisterStorageClassSpecifier
    register auto* self asm("r0") = &user_handler;

    asm volatile (
        ".set REG_BIOSIF, 0x3FFFFF8\n"
        ".set REG_BASE, 0x4000000\n"
        ".set REG_IE_IF, 0x4000200\n"
        ".set REG_IF, 0x4000202\n"
        ".set REG_IME, 0x4000208\n"
        // r1 = REG_BASE
        "mov r3, #REG_BASE\n"
        // r1 = REG_IE & REG_IF, r3 = &REG_IE_IF
        "ldr r1, [r3, #(REG_IE_IF - REG_BASE)]!\n"
        "and r1, r1, r1, lsr #16\n"
        // r2 = REG_BIOSIF | r1
        "ldr r2, [r3, #(REG_BIOSIF - REG_IE_IF)]\n"
        "orr r2, r2, r1\n"
        // Acknowledge REG_IF and REG_BIOSIF
        "strh r1, [r3, #(REG_IF - REG_IE_IF)]\n"
        "str r2, [r3, #(REG_BIOSIF - REG_IE_IF)]\n"
        // Clear handled from REG_IE
        "ldrh r2, [r3]\n"
        "bic r2, r2, r1\n"
        "strh r2, [r3]\n"
        // Disable REG_IME
        // Use r3/REG_BASE because lowest bit is clear
        "str r3, [r3, #(REG_IME - REG_IE_IF)]\n"
        // Change to user mode
        "msr cpsr_c, #0x1f\n"
        "push {r1, r3-r10, lr}\n"
        // Call user_handler()
        "ldr %[call_op], [%[call_op]]\n"
        "mov lr, pc\n"
        "bx %[call_op]\n"

        "pop {r1, r3-r10, lr}\n"
        // Disable REG_IME again
        // Use r3/REG_BASE because lowest bit is clear
        "str r3, [r3, #(REG_IME - REG_IE_IF)]\n"

        // Change to irq mode
        "msr cpsr_c, #0x92\n"
        // Restore REG_IE
        "ldrh r2, [r3]\n"
        "orr r2, r2, r1\n"
        "strh r2, [r3]\n"
        // Enable REG_IME
        "mov r2, #1\n"
        "str r2, [r3, #(REG_IME - REG_IE_IF)]\n"
        "bx lr"
    :: [user_handler]"l"(self), [call_op]"l"(&call_op) : "r1", "r2", "r3");
}

void irq_empty_handler() {
    asm volatile (
        ".set REG_BIOSIF, 0x3FFFFF8\n"
        ".set REG_BASE, 0x4000000\n"
        ".set REG_IE_IF, 0x4000200\n"
        ".set REG_IF, 0x4000202\n"
        "mov r0, #REG_BASE\n"
        // r1 = REG_IE & REG_IF, r0 = &REG_IE_IF
        "ldr r1, [r0, #(REG_IE_IF - REG_BASE)]!\n"
        "and r1, r1, r1, lsr #16\n"
        // r2 = REG_BIOSIF | r1
        "ldr r2, [r0, #(REG_BIOSIF - REG_IE_IF)]\n"
        "orr r2, r2, r1\n"
        // Acknowledge REG_IF and REG_BIOSIF
        "strh r1, [r0, #(REG_IF - REG_IE_IF)]\n"
        "str r2, [r0, #(REG_BIOSIF - REG_IE_IF)]\n"
        "bx lr"
    ::: "r0", "r1", "r2");
}
