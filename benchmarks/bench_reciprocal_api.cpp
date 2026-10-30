#include <gba/benchmark>
#include <gba/fixed_point>
#include <gba/peripherals>
#include <gba/reciprocal>
#include <gba/testing>

#include <array>
#include <cstdint>
#include <limits>
#include <span>

namespace {

    using namespace gba::literals;
    using q16 = gba::fixed<std::int32_t, 16>;
    using wide_q16 = gba::precise<std::int32_t, 16>;
    using u32 = std::uint32_t;
    constexpr unsigned sample_count = 128;
    constexpr unsigned reuse_count = 4;

    struct sample {
        q16 numerator;
        q16 denominator;
        std::int32_t integer_denominator;
    };

    std::array<sample, sample_count> samples;
    std::array<q16, sample_count> numerators;
    std::array<q16, sample_count> denominators;
    std::array<std::int32_t, sample_count> integer_denominators;
    std::array<q16, sample_count> results;
    volatile u32 sink;

    u32 checksum() {
        u32 sum = 0;
        for (const auto value : results) {
            sum += static_cast<u32>(gba::bit_cast(value));
        }
        return sum;
    }

    template<auto Strategy, bool Integer>
    [[gnu::noinline, gnu::noclone]] void batch_kernel() {
        if constexpr (Integer) {
            Strategy(numerators, integer_denominators, results);
        } else {
            Strategy(numerators, denominators, results);
        }
    }

    template<auto Strategy, bool Integer, unsigned Group>
    [[gnu::noinline, gnu::noclone]] void shared_batch_kernel() {
        for (unsigned i = 0; i < sample_count; i += Group) {
            const auto divider = [&] {
                if constexpr (Integer) {
                    return Strategy(integer_denominators[i]);
                } else {
                    return Strategy(denominators[i]);
                }
            }();
            const std::span<const q16, Group> in{numerators.data() + i, Group};
            std::span<q16, Group> out{results.data() + i, Group};
            divider(in, out);
        }
    }

    constexpr q16 from_raw(std::int32_t raw) {
        return __builtin_bit_cast(q16, raw);
    }

    template<auto Strategy, bool Reuse>
    [[gnu::noinline, gnu::noclone]] void normalized_kernel() {
        u32 sum = 0;
        if constexpr (Reuse) {
            for (unsigned i = 0; i < sample_count; i += reuse_count) {
                const auto divider = Strategy(samples[i].denominator);
                for (unsigned j = 0; j < reuse_count; ++j) {
                    sum += static_cast<u32>(gba::bit_cast(divider(samples[i + j].numerator)));
                }
            }
        } else {
            for (const auto& item : samples) {
                sum += static_cast<u32>(
                    gba::bit_cast(Strategy(item.numerator, item.denominator)));
            }
        }
        sink = sum;
    }

    template<auto Strategy, bool Reuse>
    [[gnu::noinline, gnu::noclone]] void direct_kernel() {
        u32 sum = 0;
        if constexpr (Reuse) {
            for (unsigned i = 0; i < sample_count; i += reuse_count) {
                const auto divider = Strategy(samples[i].integer_denominator);
                for (unsigned j = 0; j < reuse_count; ++j) {
                    sum += static_cast<u32>(gba::bit_cast(divider(samples[i + j].numerator)));
                }
            }
        } else {
            for (const auto& item : samples) {
                sum += static_cast<u32>(
                    gba::bit_cast(Strategy(item.numerator, item.integer_denominator)));
            }
        }
        sink = sum;
    }

    [[gnu::noinline, gnu::noclone]] void fixed_division_kernel() {
        u32 sum = 0;
        for (const auto& item : samples) {
            const auto numerator = __builtin_bit_cast(wide_q16, gba::bit_cast(item.numerator));
            const auto denominator = __builtin_bit_cast(wide_q16, gba::bit_cast(item.denominator));
            sum += static_cast<u32>(gba::bit_cast(numerator / denominator));
        }
        sink = sum;
    }

    [[gnu::noinline, gnu::noclone]] void integer_division_kernel() {
        u32 sum = 0;
        for (const auto& item : samples) {
            sum += static_cast<u32>(gba::bit_cast(item.numerator / item.integer_denominator));
        }
        sink = sum;
    }

    template<auto Strategy, bool Integer, bool Exact = false>
    void audit(const char* label) {
        u32 max_error = 0;
        for (const auto& item : samples) {
            const auto denominator = [&] {
                if constexpr (Integer) {
                    return item.integer_denominator;
                } else {
                    return item.denominator;
                }
            }();
            const auto n = gba::bit_cast(item.numerator);
            const auto expected = [&] {
                if constexpr (Integer) {
                    return n / denominator;
                } else {
                    return static_cast<std::int32_t>(
                        (std::int64_t{n} * 65536) / gba::bit_cast(denominator));
                }
            }();
            const auto actual = gba::bit_cast(Strategy(item.numerator, denominator));
            const auto shared = Strategy(denominator);
            gba::test.expect.eq(gba::bit_cast(shared(item.numerator)), actual);
            if constexpr (Exact) {
                gba::test.expect.eq(actual, expected);
            }
            const auto error = static_cast<u32>(actual > expected ? actual - expected : expected - actual);
            if (error > max_error) {
                max_error = error;
            }
        }
        gba::benchmark::log(
            gba::log::level::info, "RECIPROCAL_API_ACCURACY,{strategy},{max_raw_error},{table_bytes}"_fmt,
            "strategy"_arg = label, "max_raw_error"_arg = max_error,
            "table_bytes"_arg = Strategy.table_bytes);
    }

    template<typename Function>
    void measure(const char* label, unsigned reuse, Function function, bool batch = false) {
        u32 low = std::numeric_limits<u32>::max();
        u32 high = 0;
        u32 total = 0;
        for (unsigned repeat = 0; repeat < 9; ++repeat) {
            const auto cycles = gba::benchmark::measure(function);
            if (cycles < low) {
                low = cycles;
            }
            if (cycles > high) {
                high = cycles;
            }
            total += cycles;
        }
        if (batch) {
            sink = checksum();
        }
        gba::benchmark::log(
            gba::log::level::info,
            "RECIPROCAL_API,{strategy},{count},{reuse},{min},{mean},{max},{checksum}"_fmt,
            "strategy"_arg = label,
            "count"_arg = sample_count,
            "reuse"_arg = reuse,
            "min"_arg = low,
            "mean"_arg = total / 9,
            "max"_arg = high,
            "checksum"_arg = static_cast<u32>(sink));
    }

    void fill_samples() {
        for (unsigned i = 0; i < sample_count; ++i) {
            const auto numerator_raw = static_cast<std::int32_t>((i * 104729u) % 20000001u) - 10000000;
            const auto group = i / reuse_count;
            samples[i] = {
                from_raw(numerator_raw),
                from_raw(static_cast<std::int32_t>(65536u + (group * 104729u) % (16u * 65536u))),
                static_cast<std::int32_t>(1 + (i / reuse_count) % 79)
            };
            numerators[i] = samples[i].numerator;
            denominators[i] = samples[i].denominator;
            integer_denominators[i] = samples[i].integer_denominator;
        }
    }

} // namespace

int main() {
    using namespace gba::reciprocal;
    gba::reg_ime = false;
    gba::reg_waitcnt = {};
    gba::reg_dispcnt = {.disable = true};
    fill_samples();

    constexpr auto nr1 = normalized<8, 1>;
    constexpr auto nr2 = normalized<8, 2>;
    constexpr auto affine3 = affine<3>;
    constexpr auto exact_rows = corrected<direct<1, 79>>;
    constexpr auto packed_rows = direct<1, 79, entry_format::packed_u16>;

    gba::benchmark::log(gba::log::level::info,
                        "RECIPROCAL_API_BEGIN,waitcnt=0x4317,caller=thumb_rom,kernels=arm_iwram,tables=rom");
    audit<nr1, false>("normalized8_nr1");
    audit<nr2, false>("normalized8_nr2");
    audit<corrected<nr2>, false, true>("normalized8_corrected");
    audit<affine3, false>("affine3");
    audit<exact_rows, true, true>("direct_u32_corrected");
    audit<packed_rows, true>("direct_u16_approx");
    measure("fixed_division", 1, fixed_division_kernel);
    measure("integer_division", 1, integer_division_kernel);
    measure("normalized8_nr1_one_shot", 1, normalized_kernel<nr1, false>);
    measure("normalized8_nr1_reused", reuse_count, normalized_kernel<nr1, true>);
    measure("normalized8_nr2_one_shot", 1, normalized_kernel<nr2, false>);
    measure("normalized8_nr2_reused", reuse_count, normalized_kernel<nr2, true>);
    measure("normalized8_corrected_one_shot", 1, normalized_kernel<corrected<nr2>, false>);
    measure("normalized8_corrected_reused", reuse_count, normalized_kernel<corrected<nr2>, true>);
    measure("affine3_one_shot", 1, normalized_kernel<affine3, false>);
    measure("affine3_reused", reuse_count, normalized_kernel<affine3, true>);
    measure("direct_u32_corrected_one_shot", 1, direct_kernel<exact_rows, false>);
    measure("direct_u32_corrected_reused", reuse_count, direct_kernel<exact_rows, true>);
    measure("direct_u16_approx_one_shot", 1, direct_kernel<packed_rows, false>);
    measure("direct_u16_approx_reused", reuse_count, direct_kernel<packed_rows, true>);
    measure("normalized8_nr1_batch", 1, batch_kernel<nr1, false>, true);
    measure("normalized8_nr1_shared_batch4", reuse_count, shared_batch_kernel<nr1, false, 4>, true);
    measure("normalized8_nr1_shared_batch32", 32, shared_batch_kernel<nr1, false, 32>, true);
    measure("normalized8_nr2_batch", 1, batch_kernel<nr2, false>, true);
    measure("normalized8_nr2_shared_batch4", reuse_count, shared_batch_kernel<nr2, false, 4>, true);
    measure("normalized8_corrected_batch", 1, batch_kernel<corrected<nr2>, false>, true);
    measure("normalized8_corrected_shared_batch4", reuse_count,
            shared_batch_kernel<corrected<nr2>, false, 4>, true);
    measure("affine3_batch", 1, batch_kernel<affine3, false>, true);
    measure("affine3_shared_batch4", reuse_count, shared_batch_kernel<affine3, false, 4>, true);
    measure("direct_u32_corrected_batch", 1, batch_kernel<exact_rows, true>, true);
    measure("direct_u32_corrected_shared_batch4", reuse_count, shared_batch_kernel<exact_rows, true, 4>, true);
    measure("direct_u32_corrected_shared_batch32", 32, shared_batch_kernel<exact_rows, true, 32>, true);
    measure("direct_u16_approx_batch", 1, batch_kernel<packed_rows, true>, true);
    measure("direct_u16_approx_shared_batch4", reuse_count, shared_batch_kernel<packed_rows, true, 4>, true);
    gba::benchmark::log(gba::log::level::info, "RECIPROCAL_API_END");
    return gba::test.finish();
}
