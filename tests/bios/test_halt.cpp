#include <gba/bios>
#include <gba/interrupt>
#include <gba/timer>

#include <mgba_test.hpp>

static bool timerHit;

int main() {
    using namespace std::chrono_literals;

    gba::reg_ie = {.timer0 = true};
    gba::irq_handler = [](auto) static {
        timerHit = true;
        gba::reg_tmcnt_h[0] = {};
    };
    gba::reg_ime = true;

    static constexpr auto one_second_timer = gba::make_timer(1s, true);
    static_assert(one_second_timer.has_value());

    timerHit = false;
    gba::reg_tmcnt[0] = one_second_timer.value().first[0];
    gba::Halt();

    ASSERT_TRUE(timerHit);

    timerHit = false;
    gba::reg_tmcnt[0] = one_second_timer.value().first[0];
    gba::IntrWait(true, {.timer0 = true});

    ASSERT_TRUE(timerHit);
}
