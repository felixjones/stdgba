#include <gba/critical_section>
#include <gba/peripherals>
#include <gba/testing>

#include <type_traits>

[[gnu::target("arm")]]
bool interrupts_disabled() {
    unsigned int status;
    asm volatile("mrs %0, cpsr" : "=r"(status));
    return status & 0x80;
}

static_assert(!std::is_copy_constructible_v<gba::critical_section>);
static_assert(!std::is_copy_assignable_v<gba::critical_section>);
static_assert(!std::is_move_constructible_v<gba::critical_section>);
static_assert(!std::is_move_assignable_v<gba::critical_section>);

int main() {
    gba::reg_ime = true;
    gba::test.expect.is_false(interrupts_disabled());

    {
        const gba::critical_section outer;
        gba::test.expect.is_true(interrupts_disabled());
        gba::test.expect.is_true(gba::reg_ime);

        {
            const gba::critical_section inner;
            gba::test.expect.is_true(interrupts_disabled());
        }

        gba::test.expect.is_true(interrupts_disabled());
    }

    gba::test.expect.is_false(interrupts_disabled());
    gba::test.expect.is_true(gba::reg_ime);

    {
        const gba::critical_section outer;
        {
            const gba::critical_section inner;
            gba::test.expect.is_true(interrupts_disabled());
        }
        gba::test.expect.is_true(interrupts_disabled());
    }

    gba::test.expect.is_false(interrupts_disabled());

    bool entered = false;

    if (const gba::critical_section section{}) {
        entered = true;
        gba::test.expect.is_true(interrupts_disabled());
    }

    gba::test.expect.is_true(entered);
    gba::test.expect.is_false(interrupts_disabled());

    return gba::test.finish();
}
