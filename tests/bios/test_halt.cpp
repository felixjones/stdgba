#include <gba/bios>
#include <gba/interrupt>
#include <gba/testing>
#include <gba/timer>

static bool timerHit;

int main() {
    using namespace std::chrono_literals;

    gba::reg_ie = {.timer0 = true};
    gba::irq_handler = [](auto) static {
        timerHit = true;
        gba::reg_tmcnt_h[0] = {};
    };
    gba::reg_ime = true;

    static constexpr auto one_second_timer = gba::compile_timer(1s, true);
    static_assert(one_second_timer.size() == 1);

    timerHit = false;
    gba::reg_tmcnt[0] = one_second_timer[0];
    gba::Halt();

    gba::test.is_true(timerHit);

    timerHit = false;
    gba::reg_tmcnt[0] = one_second_timer[0];
    gba::IntrWait(true, {.timer0 = true});

    gba::test.is_true(timerHit);
    return gba::test.finish();
}
