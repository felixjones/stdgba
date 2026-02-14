#include <gba/timer>
#include <mgba_test.hpp>

#include <chrono>

int main() {
    using namespace gba;
    using namespace std::chrono_literals;

    // ========================================
    // Test: make_timer with various durations
    // ========================================

    // 1 second timer (uses cascading)
    {
        auto result = make_timer(1s, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_TRUE(count >= 1);
        EXPECT_TRUE(count <= 4);
    }

    // 1 millisecond timer
    {
        auto result = make_timer(1ms, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_TRUE(count >= 1);
    }

    // ========================================
    // Test: make_timer with IRQ flag
    // ========================================

    // Last timer should have overflow_irq set when irq=true
    {
        auto result = make_timer(100ms, true);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        // The last timer in the chain should have overflow_irq
        EXPECT_TRUE(timers[count - 1].second.overflow_irq);
    }

    // Last timer should NOT have overflow_irq when irq=false
    {
        auto result = make_timer(100ms, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_FALSE(timers[count - 1].second.overflow_irq);
    }

    // ========================================
    // Test: make_timer cascade behavior
    // ========================================

    // Short duration should use 1 timer
    {
        auto result = make_timer(1us, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_EQ(count, 1u);
        // First timer should be enabled
        EXPECT_TRUE(timers[0].second.enabled);
        // First timer should NOT cascade
        EXPECT_FALSE(timers[0].second.cascade);
    }

    // Long duration requiring cascade should have cascade flags set
    {
        auto result = make_timer(1s, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        if (count >= 2) {
            // First timer should NOT cascade
            EXPECT_FALSE(timers[0].second.cascade);
            // Second timer SHOULD cascade
            EXPECT_TRUE(timers[1].second.cascade);
        }
    }

    // ========================================
    // Test: make_timer_exact with limited cascade timers
    // ========================================

    // Restricting to 0 extra timers for a long duration should fail
    // At 1024x prescaler, max single timer = 65535 * 1024 / 16777216 ~= 4 seconds
    // So 5 seconds requires cascading
    {
        auto result = make_timer_exact(5s, false, 0);
        // 5 seconds needs cascading, so this should fail
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), timer_error::not_enough_cascade_timers);
    }

    // ========================================
    // Test: make_timer_exact representation check
    // ========================================

    // make_timer_exact returns error for unrepresentable durations
    {
        // 1ms is likely not exactly representable
        auto result = make_timer_exact(1ms, false);
        if (!result.has_value()) {
            EXPECT_EQ(result.error(), timer_error::unrepresentable_duration);
        }
        // Either way is fine - we just test the API
    }

    // ========================================
    // Test: make_timer approximation
    // ========================================

    // make_timer should always succeed (unless cascade limit hit)
    {
        auto result = make_timer(33ms, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_TRUE(count >= 1);
    }

    // Arbitrary duration that may not be exact
    {
        auto result = make_timer(17ms, false);
        ASSERT_TRUE(result.has_value());
    }

    // ========================================
    // Test: make_cycle_timer with specific prescaler
    // ========================================

    // 1024x prescaler duration
    {
        auto result = make_cycle_timer(clock_duration_1024<>{1000}, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(timers[0].second.cycles, cycles_1024);
    }

    // 256x prescaler duration
    {
        auto result = make_cycle_timer(clock_duration_256<>{1000}, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(timers[0].second.cycles, cycles_256);
    }

    // 64x prescaler duration
    {
        auto result = make_cycle_timer(clock_duration_64<>{1000}, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(timers[0].second.cycles, cycles_64);
    }

    // 1x prescaler duration (raw cycles)
    {
        auto result = make_cycle_timer(clock_duration<>{1000}, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(timers[0].second.cycles, cycles_1);
    }

    // ========================================
    // Test: make_cycle_timer requiring cascade
    // ========================================

    // Large cycle count requiring cascade (> 0xFFFF)
    {
        auto result = make_cycle_timer(clock_duration_1024<>{0x20000}, false);
        ASSERT_TRUE(result.has_value());
        auto [timers, count] = *result;
        EXPECT_TRUE(count >= 2);
    }

    // ========================================
    // Test: make_cycle_timer cascade limit error
    // ========================================

    // Very large count with no cascade timers allowed
    {
        auto result = make_cycle_timer(clock_duration_1024<>{0x20000}, false, 0);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), timer_error::not_enough_cascade_timers);
    }

    // ========================================
    // Test: clock_duration conversions
    // ========================================

    // clock_duration should convert to/from chrono types
    {
        clock_duration<> cycles{16777216}; // 1 second worth of cycles
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(cycles);
        EXPECT_EQ(seconds.count(), 1);
    }

    {
        auto ms = std::chrono::milliseconds{1000};
        auto cycles = std::chrono::duration_cast<clock_duration<>>(ms);
        EXPECT_EQ(cycles.count(), 16777216u);
    }

    test::exit(test::failures());
}
