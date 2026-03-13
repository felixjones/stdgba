#include <gba/testing>
#include <gba/timer>

#include <algorithm>

int main() {
    using namespace gba;
    using namespace std::chrono_literals;

    // Test: compile_timer with 1 second duration (cascading)

    {
        static constexpr auto timer = compile_timer(1s, true);
        static_assert(timer.size() >= 1);
        static_assert(timer.size() <= 4);

        // Write cascade chain to hardware
        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        // First timer is enabled and not cascading
        timer_control ctrl = reg_tmcnt_h[0];
        gba::test.expect.is_true(ctrl.enabled);
        gba::test.expect.is_false(ctrl.cascade);

        // If cascading, subsequent timers have cascade flag
        if constexpr (timer.size() >= 2) {
            timer_control ctrl1 = reg_tmcnt_h[1];
            gba::test.expect.is_true(ctrl1.cascade);
            gba::test.expect.is_true(ctrl1.enabled);
        }

        // Last timer has overflow IRQ
        timer_control last = reg_tmcnt_h[timer.size() - 1];
        gba::test.expect.is_true(last.overflow_irq);

        // Disable timers
        for (std::size_t i = 0; i < timer.size(); ++i) reg_tmcnt_h[i] = {};
    }

    // Test: compile_timer with short duration (single timer, no IRQ)

    {
        static constexpr auto timer = compile_timer(10us, false);
        static_assert(timer.size() == 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        timer_control ctrl = reg_tmcnt_h[0];
        gba::test.expect.is_true(ctrl.enabled);
        gba::test.expect.is_false(ctrl.cascade);
        gba::test.expect.is_false(ctrl.overflow_irq);

        reg_tmcnt_h[0] = {};
    }

    // Test: compile_timer at non-zero base index

    {
        static constexpr auto timer = compile_timer(100ms, true);

        // Write to timers starting at index 2
        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin() + 2);

        timer_control ctrl = reg_tmcnt_h[2];
        gba::test.expect.is_true(ctrl.enabled);

        for (std::size_t i = 0; i < timer.size(); ++i) reg_tmcnt_h[2 + i] = {};
    }

    // Test: compile_timer_exact with exactly representable duration

    {
        static constexpr auto timer = compile_timer_exact(1s, false);
        static_assert(timer.size() >= 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        timer_control ctrl = reg_tmcnt_h[0];
        gba::test.expect.is_true(ctrl.enabled);

        for (std::size_t i = 0; i < timer.size(); ++i) reg_tmcnt_h[i] = {};
    }

    // Test: compile_cycle_timer with each prescaler

    {
        static constexpr auto timer = compile_cycle_timer(clock_duration_1024<>{1000}, true);
        static_assert(timer.size() == 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        timer_control ctrl = reg_tmcnt_h[0];
        gba::test.expect.is_true(ctrl.enabled);
        gba::test.expect.eq(ctrl.cycles, cycles_1024);
        gba::test.expect.is_true(ctrl.overflow_irq);

        reg_tmcnt_h[0] = {};
    }

    {
        static constexpr auto timer = compile_cycle_timer(clock_duration_256<>{500}, false);
        static_assert(timer.size() == 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());
        gba::test.expect.eq(reg_tmcnt_h[0].value().cycles, cycles_256);

        reg_tmcnt_h[0] = {};
    }

    {
        static constexpr auto timer = compile_cycle_timer(clock_duration_64<>{2000}, false);
        static_assert(timer.size() == 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());
        gba::test.expect.eq(reg_tmcnt_h[0].value().cycles, cycles_64);

        reg_tmcnt_h[0] = {};
    }

    {
        static constexpr auto timer = compile_cycle_timer(clock_duration<>{5000}, false);
        static_assert(timer.size() == 1);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());
        gba::test.expect.eq(reg_tmcnt_h[0].value().cycles, cycles_1);

        reg_tmcnt_h[0] = {};
    }

    // Test: compile_cycle_timer requiring cascade

    {
        static constexpr auto timer = compile_cycle_timer(clock_duration_1024<>{0x20000}, false);
        static_assert(timer.size() >= 2);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        gba::test.expect.is_false(reg_tmcnt_h[0].value().cascade);
        gba::test.expect.is_true(reg_tmcnt_h[1].value().cascade);

        for (std::size_t i = 0; i < timer.size(); ++i) reg_tmcnt_h[i] = {};
    }

    // Test: compiled timer actually runs

    {
        static constexpr auto timer = compile_timer(1ms, false);

        std::copy(timer.begin(), timer.end(), reg_tmcnt.begin());

        // Read initial counter
        unsigned short initial = reg_tmcnt_l_stat[0];

        // Busy-wait for counter to change (timer is running)
        int spins = 0;
        unsigned short current = initial;
        while (current == initial && spins < 100000) {
            current = reg_tmcnt_l_stat[0];
            spins = spins + 1;
        }

        gba::test.expect.is_true(current != initial);

        for (std::size_t i = 0; i < timer.size(); ++i) reg_tmcnt_h[i] = {};
    }

    // Test: clock_duration conversions

    {
        clock_duration<> cycles{16777216}; // 1 second worth of cycles
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(cycles);
        gba::test.expect.eq(seconds.count(), 1);
    }

    {
        auto ms = std::chrono::milliseconds{1000};
        auto cycles = std::chrono::duration_cast<clock_duration<>>(ms);
        gba::test.expect.eq(cycles.count(), 16777216u);
    }

    return gba::test.finish();
}
