/**
 * @file bits/timer/compiled.hpp
 * @brief Consteval-compiled timer configurations.
 *
 * Provides compile-time validated timer setup following the consteval
 * compiler pattern used by gba/flash, gba/format, and gba/music.
 * Errors in duration, prescaler selection, or cascade depth become
 * compiler diagnostics instead of runtime std::expected checks.
 */
#pragma once

#include <gba/peripherals>

#include <array>
#include <chrono>
#include <cstddef>
#include <ratio>

namespace gba {

    /// @brief GBA CPU clock frequency ratio (1 / 16.78MHz).
    ///
    /// The GBA CPU runs at 16,777,216 Hz (2^24 Hz).
    /// Use this ratio with std::chrono::duration to represent GBA clock cycles.
    using clock_ratio = std::ratio<1, 16777216>;

    /**
     * @brief Compiled timer configuration container.
     *
     * Constructed by compile_timer(), compile_timer_exact(), or
     * compile_cycle_timer(). Behaves as a range of timer_config entries --
     * begin()/end() iterate only over the active timers in the cascade chain.
     *
     * Example:
     * @code{.cpp}
     * using namespace std::chrono_literals;
     *
     * constexpr auto timer = gba::compile_timer(1s, true);
     * std::copy(timer.begin(), timer.end(), gba::reg_tmcnt.begin());
     * @endcode
     */
    struct compiled_timer {
        std::array<timer_config, 4> m_timers{};
        std::size_t m_count{};

        /// @brief Number of hardware timers in the cascade chain.
        [[nodiscard]]
        constexpr std::size_t size() const noexcept { return m_count; }

        /// @brief Access a timer configuration by index.
        [[nodiscard]]
        constexpr const timer_config& operator[](std::size_t i) const noexcept { return m_timers[i]; }

        /// @brief Iterator to the first timer configuration.
        [[nodiscard]]
        constexpr const timer_config* begin() const noexcept { return m_timers.data(); }

        /// @brief Iterator past the last active timer configuration.
        [[nodiscard]]
        constexpr const timer_config* end() const noexcept { return m_timers.data() + m_count; }

        /// @brief Pointer to the underlying timer configuration data.
        [[nodiscard]]
        constexpr const timer_config* data() const noexcept { return m_timers.data(); }
    };

    namespace detail {

        /// @brief Build a compiled_timer from prescaled cycle count.
        consteval compiled_timer build_timer(long long total, gba::cycles prescaler, bool irq, std::size_t cascadeTimers) {
            compiled_timer result{};

            if (total <= 0)
                throw "duration must be positive";

            if (total < 0x10000) {
                result.m_count = 1;
                result.m_timers[0] = timer_config{
                    static_cast<unsigned short>(-total),
                    { .cycles = prescaler, .overflow_irq = irq, .enabled = true }
                };
                return result;
            }

            if (cascadeTimers < 1)
                throw "duration requires more cascade timers than available";

            auto cascade1 = total / 0x10000;
            auto base = total - cascade1 * 0x10000;

            if (cascade1 < 0x10000) {
                result.m_count = 2;
                result.m_timers[0] = timer_config{
                    static_cast<unsigned short>(-base),
                    { .cycles = prescaler, .enabled = true }
                };
                result.m_timers[1] = timer_config{
                    static_cast<unsigned short>(-cascade1),
                    { .cascade = true, .overflow_irq = irq, .enabled = true }
                };
                return result;
            }

            if (cascadeTimers < 2)
                throw "duration requires more cascade timers than available";

            auto cascade2 = cascade1 / 0x10000;
            cascade1 = cascade1 - cascade2 * 0x10000;

            if (cascade2 < 0x10000) {
                result.m_count = 3;
                result.m_timers[0] = timer_config{
                    static_cast<unsigned short>(-base),
                    { .cycles = prescaler, .enabled = true }
                };
                result.m_timers[1] = timer_config{
                    static_cast<unsigned short>(-cascade1),
                    { .cascade = true, .enabled = true }
                };
                result.m_timers[2] = timer_config{
                    static_cast<unsigned short>(-cascade2),
                    { .cascade = true, .overflow_irq = irq, .enabled = true }
                };
                return result;
            }

            if (cascadeTimers < 3)
                throw "duration requires more cascade timers than available";

            auto cascade3 = cascade2 / 0x10000;
            cascade2 = cascade2 - cascade3 * 0x10000;

            if (cascade3 < 0x10000) {
                result.m_count = 4;
                result.m_timers[0] = timer_config{
                    static_cast<unsigned short>(-base),
                    { .cycles = prescaler, .enabled = true }
                };
                result.m_timers[1] = timer_config{
                    static_cast<unsigned short>(-cascade1),
                    { .cascade = true, .enabled = true }
                };
                result.m_timers[2] = timer_config{
                    static_cast<unsigned short>(-cascade2),
                    { .cascade = true, .enabled = true }
                };
                result.m_timers[3] = timer_config{
                    static_cast<unsigned short>(-cascade3),
                    { .cascade = true, .overflow_irq = irq, .enabled = true }
                };
                return result;
            }

            throw "duration requires more cascade timers than available";
        }

        /// @brief Select best prescaler and build compiled_timer.
        template<typename Rep, typename Period>
        consteval compiled_timer compile_timer_impl(std::chrono::duration<Rep, Period> duration, bool irq, std::size_t extraTimers) {
            using duration_1024 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 1024>>>;
            using duration_256 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 256>>>;
            using duration_64 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 64>>>;
            using duration_1 = std::chrono::duration<long long, clock_ratio>;

            using dur = std::chrono::duration<long long, Period>;

            const auto d1024 = std::chrono::duration_cast<duration_1024>(duration);
            const auto d256 = std::chrono::duration_cast<duration_256>(duration);
            const auto d64 = std::chrono::duration_cast<duration_64>(duration);
            const auto d1 = std::chrono::duration_cast<duration_1>(duration);

            const long long diffs[] = {
                static_cast<long long>(duration.count()) - std::chrono::duration_cast<dur>(d1024).count(),
                static_cast<long long>(duration.count()) - std::chrono::duration_cast<dur>(d256).count(),
                static_cast<long long>(duration.count()) - std::chrono::duration_cast<dur>(d64).count(),
                static_cast<long long>(duration.count()) - std::chrono::duration_cast<dur>(d1).count(),
            };

            // Select the prescaler with smallest approximation error
            // Coarsest-first: on ties, prefer fewer cascade timers
            struct candidate {
                long long count;
                long long diff;
                gba::cycles prescaler;
                bool valid;
            };

            const candidate candidates[] = {
                { d1024.count(), diffs[0], cycles_1024, d1024.count() > 0 },
                { d256.count(), diffs[1], cycles_256, d256.count() > 0 },
                { d64.count(), diffs[2], cycles_64, d64.count() > 0 },
                { d1.count(), diffs[3], cycles_1, d1.count() > 0 },
            };

            long long bestCount = 0;
            long long bestDiff = -1;
            gba::cycles bestPrescaler = cycles_1;

            for (const auto& c : candidates) {
                if (!c.valid) continue;
                long long absDiff = c.diff < 0 ? -c.diff : c.diff;
                if (bestDiff < 0 || absDiff < bestDiff) {
                    bestDiff = absDiff;
                    bestCount = c.count;
                    bestPrescaler = c.prescaler;
                }
            }

            if (bestDiff < 0)
                throw "duration too short to represent with any prescaler";

            return build_timer(bestCount, bestPrescaler, irq, extraTimers);
        }

        /// @brief Select best exact prescaler and build compiled_timer.
        template<typename Rep, typename Period>
        consteval compiled_timer compile_timer_exact_impl(std::chrono::duration<Rep, Period> duration, bool irq, std::size_t extraTimers) {
            using duration_1024 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 1024>>>;
            using duration_256 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 256>>>;
            using duration_64 = std::chrono::duration<long long, std::ratio_divide<clock_ratio, std::ratio<1, 64>>>;
            using duration_1 = std::chrono::duration<long long, clock_ratio>;

            using dur = std::chrono::duration<long long, Period>;

            // Try prescalers from coarsest to finest
            if (const auto c = std::chrono::duration_cast<duration_1024>(duration);
                std::chrono::duration_cast<dur>(c).count() == static_cast<long long>(duration.count()) && c.count() > 0) {
                return build_timer(c.count(), cycles_1024, irq, extraTimers);
            }

            if (const auto c = std::chrono::duration_cast<duration_256>(duration);
                std::chrono::duration_cast<dur>(c).count() == static_cast<long long>(duration.count()) && c.count() > 0) {
                return build_timer(c.count(), cycles_256, irq, extraTimers);
            }

            if (const auto c = std::chrono::duration_cast<duration_64>(duration);
                std::chrono::duration_cast<dur>(c).count() == static_cast<long long>(duration.count()) && c.count() > 0) {
                return build_timer(c.count(), cycles_64, irq, extraTimers);
            }

            if (const auto c = std::chrono::duration_cast<duration_1>(duration);
                std::chrono::duration_cast<dur>(c).count() == static_cast<long long>(duration.count()) && c.count() > 0) {
                return build_timer(c.count(), cycles_1, irq, extraTimers);
            }

            throw "duration cannot be exactly represented with any prescaler";
        }

        /// @brief Build compiled_timer from explicit prescaled cycles.
        template<typename Rep, typename Period>
        consteval compiled_timer compile_cycle_timer_impl(std::chrono::duration<Rep, Period> cycles, bool irq, std::size_t cascadeTimers) {
            static_assert(
                std::ratio_equal_v<Period, clock_ratio> ||
                std::ratio_equal_v<Period, std::ratio_divide<clock_ratio, std::ratio<1, 64>>> ||
                std::ratio_equal_v<Period, std::ratio_divide<clock_ratio, std::ratio<1, 256>>> ||
                std::ratio_equal_v<Period, std::ratio_divide<clock_ratio, std::ratio<1, 1024>>>,
                "Period must match a GBA prescaler (1, 64, 256, or 1024 cycles)"
            );

            constexpr auto prescaler =
                std::ratio_equal_v<Period, clock_ratio> ? cycles_1 :
                std::ratio_equal_v<Period, std::ratio_divide<clock_ratio, std::ratio<1, 64>>> ? cycles_64 :
                std::ratio_equal_v<Period, std::ratio_divide<clock_ratio, std::ratio<1, 256>>> ? cycles_256 : cycles_1024;

            return build_timer(static_cast<long long>(cycles.count()), prescaler, irq, cascadeTimers);
        }

    } // namespace detail

    /**
     * @brief Compile a timer configuration for a duration (best-fit prescaler).
     *
     * Selects the prescaler that produces the closest approximation to the
     * target duration. Errors are diagnosed at compile time.
     *
     * @param duration The target duration (chrono literal).
     * @param irq Enable overflow IRQ on the final timer in the chain.
     * @param extraTimers Maximum number of cascade timers (0-3, default 3).
     * @return A compiled_timer containing the cascade chain.
     *
     * Example:
     * @code{.cpp}
     * using namespace std::chrono_literals;
     *
     * constexpr auto timer = gba::compile_timer(1s, true);
     * std::copy(timer.begin(), timer.end(), gba::reg_tmcnt.begin());
     * @endcode
     *
     * TONCLIB Equivalent:
     * @code{.c}
     * // Manual prescaler selection and reload calculation
     * REG_TM0D = reload;
     * REG_TM0CNT = TM_FREQ_1024 | TM_IRQ | TM_ENABLE;
     * @endcode
     */
    template<typename Rep, typename Period>
    consteval compiled_timer compile_timer(std::chrono::duration<Rep, Period> duration, bool irq = false, std::size_t extraTimers = 3) {
        return detail::compile_timer_impl(duration, irq, extraTimers);
    }

    /**
     * @brief Compile a timer configuration for an exact duration.
     *
     * Only succeeds if the duration can be exactly represented with one
     * of the GBA prescaler values. Produces a compile error otherwise.
     *
     * @param duration The target duration (chrono literal).
     * @param irq Enable overflow IRQ on the final timer in the chain.
     * @param extraTimers Maximum number of cascade timers (0-3, default 3).
     * @return A compiled_timer containing the cascade chain.
     *
     * Example:
     * @code{.cpp}
     * using namespace std::chrono_literals;
     *
     * constexpr auto timer = gba::compile_timer_exact(1s, true);
     * std::copy(timer.begin(), timer.end(), gba::reg_tmcnt.begin());
     * @endcode
     */
    template<typename Rep, typename Period>
    consteval compiled_timer compile_timer_exact(std::chrono::duration<Rep, Period> duration, bool irq = false, std::size_t extraTimers = 3) {
        return detail::compile_timer_exact_impl(duration, irq, extraTimers);
    }

    /**
     * @brief Compile a timer configuration for explicit prescaled cycles.
     *
     * The Period must match a GBA prescaler (1, 64, 256, or 1024 cycles).
     *
     * @param cycles The cycle count as a prescaled chrono duration.
     * @param irq Enable overflow IRQ on the final timer.
     * @param cascadeTimers Maximum cascade timers (0-3, default 3).
     * @return A compiled_timer containing the cascade chain.
     *
     * Example:
     * @code{.cpp}
     * constexpr auto timer = gba::compile_cycle_timer(
     *     gba::clock_duration_1024<>{50000}, true
     * );
     * std::copy(timer.begin(), timer.end(), gba::reg_tmcnt.begin());
     * @endcode
     *
     * TONCLIB Equivalent:
     * @code{.c}
     * REG_TM0D = (u16)(-(50000));
     * REG_TM0CNT = TM_FREQ_1024 | TM_IRQ | TM_ENABLE;
     * @endcode
     */
    template<typename Rep, typename Period>
    consteval compiled_timer compile_cycle_timer(std::chrono::duration<Rep, Period> cycles, bool irq = false, std::size_t cascadeTimers = 3) {
        return detail::compile_cycle_timer_impl(cycles, irq, cascadeTimers);
    }

} // namespace gba
