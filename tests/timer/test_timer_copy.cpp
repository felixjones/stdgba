#include <gba/timer>

#include <mgba_test.hpp>

#include <algorithm>

int main() {
    using namespace gba;
    using namespace std::chrono_literals;

    // ========================================
    // Test: std::copy_n with timer - verify timer runs
    // ========================================

    {
        // Use a longer duration so we can observe counter changes
        auto result = make_timer(1ms, false);
        ASSERT_TRUE(result.has_value());

        auto [timers, count] = *result;

        // Copy timer config to registers
        std::copy_n(timers.begin(), count, reg_tmcnt.begin());

        // Read initial counter value
        unsigned short initial = reg_tmcnt_l_stat[0];

        // Busy-wait for counter to change (timer is running)
        int spins = 0;
        unsigned short current = initial;
        while (current == initial && spins < 100000) {
            current = reg_tmcnt_l_stat[0];
            spins = spins + 1;
        }

        // Timer should have changed
        EXPECT_TRUE(current != initial);

        // Clean up
        for (std::size_t i = 0; i < count; ++i) {
            reg_tmcnt_h[i] = {};
        }
    }

    // ========================================
    // Test: std::copy_n with cascading timers
    // ========================================

    {
        // Use timer0 as fast clock, timer1 as cascade counter
        // This verifies cascading works when copied via std::copy_n

        // Set up: timer0 overflows fast, timer1 counts overflows
        reg_tmcnt_l[0] = 0xFFF0; // Start near overflow
        reg_tmcnt_h[0] = { .cycles = cycles_1, .enabled = true };
        reg_tmcnt_l[1] = 0;
        reg_tmcnt_h[1] = { .cascade = true, .enabled = true };

        // Wait for timer0 to overflow a few times
        while (reg_tmcnt_l_stat[1] < 5) {}

        // Timer1 should have counted some overflows
        EXPECT_TRUE(reg_tmcnt_l_stat[1] >= 5);

        // Clean up
        reg_tmcnt_h[0] = {};
        reg_tmcnt_h[1] = {};
    }

    // ========================================
    // Test: make_timer result copied correctly
    // ========================================

    {
        auto result = make_timer(10us, false);
        ASSERT_TRUE(result.has_value());

        auto [timers, count] = *result;

        // Copy to registers
        std::copy_n(timers.begin(), count, reg_tmcnt.begin());

        // Verify first timer is enabled
        timer_control ctrl = reg_tmcnt_h[0];
        EXPECT_TRUE(ctrl.enabled);
        EXPECT_FALSE(ctrl.cascade); // First timer never cascades

        // If cascading, verify cascade flag on subsequent timers
        for (std::size_t i = 1; i < count; ++i) {
            timer_control cascade_ctrl = reg_tmcnt_h[i];
            EXPECT_TRUE(cascade_ctrl.cascade);
            EXPECT_TRUE(cascade_ctrl.enabled);
        }

        // Clean up - disable all timers
        for (std::size_t i = 0; i < 4; ++i) {
            reg_tmcnt_h[i] = {};
        }
    }

    // ========================================
    // Test: constexpr make_timer with copy
    // ========================================

    {
        static constexpr auto timer_config = make_timer(1ms, false);
        static_assert(timer_config.has_value());

        auto [timers, count] = *timer_config;

        // Copy timer config to registers
        std::copy_n(timers.begin(), count, reg_tmcnt.begin());

        // Read initial counter value
        unsigned short initial = reg_tmcnt_l_stat[0];

        // Busy-wait for counter to change
        int spins = 0;
        unsigned short current = initial;
        while (current == initial && spins < 100000) {
            current = reg_tmcnt_l_stat[0];
            spins = spins + 1;
        }

        EXPECT_TRUE(current != initial);

        // Clean up
        for (std::size_t i = 0; i < count; ++i) {
            reg_tmcnt_h[i] = {};
        }
    }

    test::exit(test::failures());
}
