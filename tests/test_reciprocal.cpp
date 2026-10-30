#include <gba/bits/fixed_point/fixed_point_base.hpp>
#include <gba/reciprocal>
#include <gba/testing>

#include <array>
#include <cstdint>
#include <limits>

namespace {

    using q8 = gba::fixed<std::int32_t, 8>;
    using q16 = gba::fixed<std::int32_t, 16>;
    using q31 = gba::fixed<std::int32_t, 31>;
    using uq16 = gba::fixed<std::uint32_t, 16>;
    using uq31 = gba::fixed<std::uint32_t, 31>;
    namespace rc = gba::reciprocal;

    template<typename T>
    constexpr T from_raw(typename gba::fixed_point_traits<T>::rep value) {
        return __builtin_bit_cast(T, value);
    }

    template<auto Strategy, gba::FixedPoint  T>
    constexpr T apply_strategy(T numerator, T denominator) {
        return Strategy(numerator, denominator);
    }

    constexpr auto norm = rc::normalized<8, 2>;
    constexpr auto exact_norm = rc::corrected<rc::normalized<8, 2>>;
    constexpr auto exact_rows = rc::corrected<rc::direct<1, 79>>;
    constexpr auto packed_rows = rc::direct<1, 79, rc::entry_format::packed_u16>;
    constexpr q8 q8_three = from_raw<q8>(3 * 256);
    constexpr q8 q8_ten = from_raw<q8>(10 * 256);
    constexpr q8 q8_exact = exact_norm(q8_ten, q8_three);

    static_assert(gba::bit_cast(q8_exact) == (10 * 256) / 3);
    static_assert(gba::bit_cast(exact_norm(q8_ten, from_raw<q8>(-3 * 256))) ==
                  -(10 * 256) / 3);
    static_assert(gba::bit_cast(exact_rows(q8_ten, 3)) == (10 * 256) / 3);
    static_assert(gba::bit_cast(exact_rows(from_raw<q8>(-10 * 256), -3)) == (10 * 256) / 3);
    static_assert(gba::bit_cast(norm(q8_ten, q8_three)) >= (10 * 256) / 3 - 1);
    static_assert(gba::bit_cast(norm(q8_ten, q8_three)) <= (10 * 256) / 3 + 1);
    static_assert(gba::bit_cast(packed_rows(q8_ten, 3)) == (10 * 256) / 3);
    static_assert(gba::bit_cast(apply_strategy<rc::normalized<8, 2>>(q8_ten, q8_three)) ==
                  gba::bit_cast(q8_exact));

    constexpr auto shared = exact_norm(q8_three);
    static_assert(gba::bit_cast(shared(q8_ten)) == gba::bit_cast(q8_exact));
    static_assert(std::is_empty_v<std::remove_cv_t<decltype(norm)>>);
    static_assert(std::is_trivially_copyable_v<std::remove_cv_t<decltype(norm)>>);

    constexpr q16 q16_ten = from_raw<q16>(10 * 65536);
    constexpr q16 q16_three = from_raw<q16>(3 * 65536);
    static_assert(gba::bit_cast(rc::corrected<rc::affine<3>>(q16_ten, q16_three)) ==
                  (10 * 65536) / 3);

    template<auto Strategy>
    concept correctable = requires { typename rc::corrected_strategy<Strategy>; };

    static_assert(correctable<rc::normalized<8, 2>>);
    static_assert(correctable<rc::affine<3>>);
    static_assert(correctable<rc::direct<1, 79>>);
    static_assert(!correctable<rc::normalized<1, 2>>);
    static_assert(!correctable<rc::normalized<8, 1>>);
    static_assert(!correctable<packed_rows>);
    static_assert(!std::invocable<decltype(norm), q8, q16>);
    static_assert(!std::invocable<decltype(shared), q16>);
    static_assert(!std::invocable<decltype(exact_norm), q8, q16>);
    static_assert(!std::invocable<decltype(norm), gba::fixed<short>, gba::fixed<short>>);
    static_assert(!std::invocable<decltype(exact_rows), q8, bool>);
    static_assert(std::is_empty_v<std::remove_cv_t<decltype(exact_norm)>>);
    static_assert(std::is_empty_v<std::remove_cv_t<decltype(exact_rows)>>);
    static_assert(norm.table_bytes == 512);
    static_assert(exact_norm.table_bytes == norm.table_bytes);
    static_assert(exact_rows.table_bytes == 79 * 4);
    static_assert(packed_rows.table_bytes == 79 * 2);
    static_assert(rc::affine<3>.table_bytes == 0);
    static_assert(gba::bit_cast(exact_norm(from_raw<uq31>(1), from_raw<uq31>(1))) == 0x80000000u);
    static_assert(gba::bit_cast(exact_norm(from_raw<q31>(-1), from_raw<q31>(1))) ==
                  std::numeric_limits<std::int32_t>::min());
    static_assert(gba::bit_cast(exact_rows(from_raw<q8>(std::numeric_limits<std::int32_t>::min()), 1)) ==
                  std::numeric_limits<std::int32_t>::min());
    static_assert(gba::bit_cast(rc::direct<0xffffffffu, 0xffffffffu, rc::entry_format::packed_u16>(
                      from_raw<uq16>(0xffffffffu), 0xffffffffu)) == 65535u);

    template<auto Strategy, typename T>
    void check_parity() {
        constexpr auto n = from_raw<T>(123456);
        constexpr auto d = from_raw<T>(12345);
        constexpr auto expected = gba::bit_cast(Strategy(n, d));
        volatile auto n_raw = gba::bit_cast(n);
        volatile auto d_raw = gba::bit_cast(d);
        const auto numerator = from_raw<T>(n_raw);
        const auto denominator = from_raw<T>(d_raw);
        const auto divider = Strategy(denominator);
        gba::test.expect.eq(gba::bit_cast(Strategy(numerator, denominator)), expected);
        gba::test.expect.eq(gba::bit_cast(divider(numerator)), expected);
    }

    template<typename T>
    void check_extremes() {
        using rep = typename gba::fixed_point_traits<T>::rep;
        constexpr std::array<rep, 7> inputs{
            std::numeric_limits<rep>::min(), rep{0}, rep{1}, rep{2}, rep{12345},
            static_cast<rep>(std::numeric_limits<rep>::max() - 1), std::numeric_limits<rep>::max()
        };
        constexpr auto strategy = rc::corrected<rc::normalized<8, 2>>;
        for (auto d : inputs) {
            if (d == 0) {
                continue;
            }
            const auto divider = strategy(from_raw<T>(d));
            for (auto n : inputs) {
                const auto target = static_cast<std::int64_t>(n) *
                    (std::int64_t{1} << gba::fixed_point_traits<T>::frac_bits);
                const auto expected = target / static_cast<std::int64_t>(d);
                if (expected < static_cast<std::int64_t>(std::numeric_limits<rep>::min()) ||
                    expected > static_cast<std::int64_t>(std::numeric_limits<rep>::max())) {
                    continue;
                }
                const auto numerator = from_raw<T>(n);
                gba::test.expect.eq(gba::bit_cast(divider(numerator)), static_cast<rep>(expected));
                gba::test.expect.eq(
                    gba::bit_cast(rc::corrected<rc::affine<3>>(numerator, from_raw<T>(d))),
                    static_cast<rep>(expected));
            }
        }
    }

    template<auto Strategy, typename T>
    consteval auto apply_boundary_cases() {
        using rep = typename gba::fixed_point_traits<T>::rep;
        struct sample {
            rep numerator{};
            rep denominator{};
            rep expected{};
        };
        std::array<sample, 32 * 3 * 8> cases{};
        constexpr std::array<std::uint32_t, 8> inputs{
            0, 1, 2, 65535, 65536, 0x7fffffffu, 0x80000000u, 0xffffffffu
        };
        unsigned index = 0;
        for (unsigned e = 0; e < 32; ++e) {
            for (int delta = -1; delta <= 1; ++delta) {
                const auto d64 = (std::int64_t{1} << e) + delta;
                if (d64 <= 0 || d64 > 0xffffffffll) {
                    continue;
                }
                const auto d_raw = __builtin_bit_cast(rep, static_cast<std::uint32_t>(d64));
                const auto d = from_raw<T>(d_raw);
                const auto divider = Strategy(d);
                for (auto raw : inputs) {
                    const auto n_raw = __builtin_bit_cast(rep, raw);
                    const auto n = from_raw<T>(n_raw);
                    const bool negative = rc::bits::negative<T>(n_raw) != rc::bits::negative<T>(d_raw);
                    if (divider.estimate_magnitude(n) <= rc::bits::quotient_limit<T>(negative)) {
                        cases[index++] = {n_raw, d_raw, gba::bit_cast(divider(n))};
                    }
                }
            }
        }
        return cases;
    }

    template<auto Strategy, typename T>
    void check_apply_boundaries() {
        static constexpr auto cases = apply_boundary_cases<Strategy, T>();
        bool valid = true;
        for (const auto& item : cases) {
            if (item.denominator == 0) {
                continue;
            }
            const auto n = from_raw<T>(item.numerator);
            const auto d = from_raw<T>(item.denominator);
            valid &= gba::bit_cast(Strategy(d)(n)) == item.expected;
            valid &= gba::bit_cast(Strategy(n, d)) == item.expected;
        }
        gba::test.expect.eq(valid, true);
    }

    template<std::uint32_t D, rc::entry_format Format>
    void check_direct_apply_boundaries() {
        constexpr auto strategy = rc::direct<D, D, Format>;
        constexpr std::array<std::uint32_t, 8> inputs{
            0, 1, D - 1, D, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu
        };
        constexpr auto expected = [=] {
            std::array<std::uint32_t, inputs.size()> out{};
            for (unsigned i = 0; i < inputs.size(); ++i) {
                out[i] = gba::bit_cast(strategy(from_raw<uq16>(inputs[i]), D));
            }
            return out;
        }();
        for (unsigned i = 0; i < inputs.size(); ++i) {
            gba::test.expect.eq(gba::bit_cast(strategy(from_raw<uq16>(inputs[i]), D)), expected[i]);
        }
    }

    template<auto Strategy>
    void check_error_bound() {
        constexpr auto bound = static_cast<std::uint64_t>(
            Strategy.relative_error_bound * 4294967296.0) + 1;
        std::uint32_t state = 0x12345678;
        bool valid = true;
        for (unsigned i = 0; i < 4096; ++i) {
            state = state * 1664525u + 1013904223u;
            const auto d = state | 0x10000u;
            state = state * 1664525u + 1013904223u;
            const auto n = state >> 1;
            const auto target = std::uint64_t{n} << 16;
            const auto exact = target / d;
            const auto allowed = ((exact + 1) * bound + 0xffffffffu) >> 32;
            const auto actual = gba::bit_cast(Strategy(from_raw<uq16>(n), from_raw<uq16>(d)));
            const auto delta = actual > exact ? actual - exact : exact - actual;
            valid &= delta <= allowed;
        }
        gba::test.expect.eq(valid, true);
    }

    void check_normalization_boundaries() {
        bool valid = true;
        constexpr std::array<std::int32_t, 7> numerators{
            -0x7fffffff, -65536, -1, 0, 1, 65536, 0x7fffffff
        };
        for (unsigned e = 0; e < 31; ++e) {
            for (std::int32_t delta = -1; delta <= 1; ++delta) {
                const auto d = static_cast<std::int64_t>(std::uint32_t{1} << e) + delta;
                if (d < 1 || d > 0x7fffffff) {
                    continue;
                }
                for (auto n : numerators) {
                    const auto expected = (std::int64_t{n} * 65536) / d;
                    if (expected < -0x80000000ll || expected > 0x7fffffff) {
                        continue;
                    }
                    const auto actual = gba::bit_cast(
                        exact_norm(from_raw<q16>(n), from_raw<q16>(static_cast<std::int32_t>(d))));
                    valid &= actual == expected;
                }
            }
        }
        for (std::uint32_t index = 0; index <= 256; ++index) {
            for (int delta = -1; delta <= 1; ++delta) {
                const auto d64 = 0x80000000ll + (std::int64_t{index} << 23) + delta;
                if (d64 < 1 || d64 > 0xffffffffll) {
                    continue;
                }
                const auto d = static_cast<std::uint32_t>(d64);
                const auto expected = (std::uint64_t{0xffffffffu} << 16) / d;
                valid &= gba::bit_cast(exact_norm(from_raw<uq16>(0xffffffffu), from_raw<uq16>(d))) ==
                         expected;
            }
        }
        gba::test.expect.eq(valid, true);
    }

    template<typename T, std::size_t N>
    std::array<T, N> raw_values(std::uint32_t seed, std::uint32_t mask, std::uint32_t floor) {
        std::array<T, N> values{};
        for (auto& value : values) {
            seed = seed * 1664525u + 1013904223u;
            auto raw = static_cast<typename gba::fixed_point_traits<T>::rep>((seed & mask) | floor);
            if (std::is_signed_v<decltype(raw)> && (seed & 0x80000000u)) {
                raw = -raw;
            }
            value = from_raw<T>(raw);
        }
        return values;
    }

    template<auto Strategy, typename T>
    void check_batch(std::uint32_t numerator_mask, std::uint32_t denominator_floor) {
        constexpr std::size_t count = 37;
        const auto numerators = raw_values<T, count>(0x1234u, numerator_mask, 0);
        const auto denominators = raw_values<T, count>(0x9876u, 0x7fffffffu, denominator_floor);
        std::array<T, count> out{};
        bool valid = true;

        Strategy(numerators, denominators, out);
        for (std::size_t i = 0; i < count; ++i) {
            valid &= gba::bit_cast(out[i]) == gba::bit_cast(Strategy(numerators[i], denominators[i]));
        }

        const auto divider = Strategy(denominators[3]);
        divider(numerators, out);
        for (std::size_t i = 0; i < count; ++i) {
            valid &= gba::bit_cast(out[i]) == gba::bit_cast(divider(numerators[i]));
        }

        auto in_place = numerators;
        divider(in_place, in_place);
        for (std::size_t i = 0; i < count; ++i) {
            valid &= gba::bit_cast(in_place[i]) == gba::bit_cast(out[i]);
        }

        in_place = numerators;
        Strategy(in_place, denominators, in_place);
        for (std::size_t i = 0; i < count; ++i) {
            valid &= gba::bit_cast(in_place[i]) == gba::bit_cast(Strategy(numerators[i], denominators[i]));
        }
        gba::test.expect.eq(valid, true);
    }

    template<auto Strategy, typename T>
    void check_direct_batch() {
        constexpr std::size_t count = 41;
        const auto numerators = raw_values<T, count>(0x5555u, 0x3fffffffu, 0);
        std::array<std::int32_t, count> denominators{};
        std::uint32_t seed = 0xabcdu;
        for (auto& d : denominators) {
            seed = seed * 1664525u + 1013904223u;
            d = static_cast<std::int32_t>(seed % 79u) + 1;
            if (std::is_signed_v<typename gba::fixed_point_traits<T>::rep> && (seed & 0x80000000u)) {
                d = -d;
            }
        }
        std::array<T, count> out{};
        bool valid = true;

        Strategy(numerators, denominators, out);
        for (std::size_t i = 0; i < count; ++i) {
            valid &= gba::bit_cast(out[i]) == gba::bit_cast(Strategy(numerators[i], denominators[i]));
        }
        for (std::int32_t d : {1, 2, 7, 79}) {
            const auto divider = Strategy(d);
            auto in_place = numerators;
            divider(in_place, in_place);
            for (std::size_t i = 0; i < count; ++i) {
                valid &= gba::bit_cast(in_place[i]) == gba::bit_cast(divider(numerators[i]));
            }
        }
        gba::test.expect.eq(valid, true);
    }

    consteval bool constexpr_batch() {
        std::array<q8, 3> numerators{q8_ten, from_raw<q8>(-10 * 256), from_raw<q8>(7)};
        std::array<q8, 3> denominators{q8_three, q8_three, from_raw<q8>(-2 * 256)};
        std::array<q8, 3> out{};
        exact_norm(numerators, denominators, out);
        bool valid = gba::bit_cast(out[0]) == (10 * 256) / 3 && gba::bit_cast(out[1]) == -(10 * 256) / 3 &&
                     gba::bit_cast(out[2]) == -(7 * 256) / (2 * 256);
        exact_norm(q8_three)(numerators, out);
        valid &= gba::bit_cast(out[1]) == -(10 * 256) / 3;
        std::array<std::int32_t, 3> rows{3, -3, 1};
        exact_rows(numerators, rows, out);
        valid &= gba::bit_cast(out[0]) == (10 * 256) / 3 && gba::bit_cast(out[1]) == (10 * 256) / 3 &&
                 gba::bit_cast(out[2]) == 7;
        return valid;
    }

    static_assert(constexpr_batch());

} // namespace

int main() {
    constexpr auto direct = rc::corrected<rc::direct<1, 79>>;
    constexpr auto normalized = rc::corrected<rc::normalized<8, 2>>;

    check_parity<rc::normalized<1, 0>, q8>();
    check_parity<rc::normalized<8, 1>, q16>();
    check_parity<rc::normalized<8, 2>, uq16>();
    check_parity<rc::affine<0>, q8>();
    check_parity<rc::affine<3>, uq16>();
    check_parity<exact_norm, q16>();
    check_extremes<q8>();
    check_extremes<q16>();
    check_extremes<q31>();
    check_extremes<uq16>();
    check_extremes<uq31>();
    check_apply_boundaries<rc::normalized<8, 1>, gba::fixed<std::int32_t, 1>>();
    check_apply_boundaries<rc::normalized<8, 2>, q16>();
    check_apply_boundaries<rc::normalized<8, 2>, q31>();
    check_apply_boundaries<rc::normalized<8, 2>, uq31>();
    check_apply_boundaries<rc::affine<3>, uq16>();
    check_direct_apply_boundaries<1, rc::entry_format::u32>();
    check_direct_apply_boundaries<2, rc::entry_format::u32>();
    check_direct_apply_boundaries<0x80000001u, rc::entry_format::u32>();
    check_direct_apply_boundaries<0xffffffffu, rc::entry_format::u32>();
    check_direct_apply_boundaries<1, rc::entry_format::packed_u16>();
    check_direct_apply_boundaries<2, rc::entry_format::packed_u16>();
    check_direct_apply_boundaries<0x80000001u, rc::entry_format::packed_u16>();
    check_direct_apply_boundaries<0xffffffffu, rc::entry_format::packed_u16>();
    check_error_bound<rc::normalized<1, 0>>();
    check_error_bound<rc::normalized<4, 2>>();
    check_error_bound<rc::normalized<8, 1>>();
    check_error_bound<rc::normalized<8, 2>>();
    check_error_bound<rc::normalized<10, 3>>();
    check_error_bound<rc::affine<0>>();
    check_error_bound<rc::affine<3>>();
    check_normalization_boundaries();
    check_batch<rc::normalized<8, 1>, q16>(0x00ffffffu, 0x00ffffffu);
    check_batch<rc::normalized<8, 2>, uq16>(0x00ffffffu, 0x00ffffffu);
    check_batch<rc::affine<3>, q8>(0x0000ffffu, 0x00ffffffu);
    check_batch<exact_norm, q16>(0x7fffffffu, 0x7fffffffu);
    check_batch<exact_norm, uq31>(0x7fffffffu, 0xffffffffu);
    check_batch<rc::corrected<rc::affine<3>>, q31>(0x0fffffffu, 0x7fffffffu);
    check_direct_batch<exact_rows, q8>();
    check_direct_batch<exact_rows, uq16>();
    check_direct_batch<rc::direct<1, 79>, q8>();
    check_direct_batch<packed_rows, q16>();

    for (std::int32_t denominator = 1; denominator <= 79; ++denominator) {
        const auto prepared = direct(denominator);
        constexpr std::array<std::int32_t, 7> numerators{
            static_cast<std::int32_t>(std::numeric_limits<std::int32_t>::min()),
            -1000001, -1, 0, 1, 1000001,
            static_cast<std::int32_t>(std::numeric_limits<std::int32_t>::max())
        };
        for (const std::int32_t numerator : numerators) {
            const auto actual = gba::bit_cast(prepared(from_raw<q8>(numerator)));
            gba::test.expect.eq(actual, numerator / denominator);
            if (numerator != std::numeric_limits<std::int32_t>::min() || denominator != 1) {
                gba::test.expect.eq(gba::bit_cast(direct(from_raw<q8>(numerator), -denominator)),
                                    -(numerator / denominator));
            }
            const auto unsigned_n = static_cast<std::uint32_t>(numerator);
            gba::test.expect.eq(
                gba::bit_cast(prepared(from_raw<uq16>(unsigned_n))),
                unsigned_n / static_cast<std::uint32_t>(denominator));
        }
    }

    for (std::int32_t denominator = 1; denominator <= 2048; denominator += 7) {
        const auto positive_denominator = from_raw<q8>(denominator);
        const auto negative_denominator = from_raw<q8>(-denominator);
        for (std::int32_t numerator = -12000; numerator <= 12000; numerator += 97) {
            const auto input = from_raw<q8>(numerator);
            const auto expected = static_cast<std::int32_t>(
                (static_cast<std::int64_t>(numerator) * 256) / denominator);
            gba::test.expect.eq(gba::bit_cast(normalized(input, positive_denominator)), expected);
            gba::test.expect.eq(gba::bit_cast(normalized(input, negative_denominator)), -expected);
        }
    }

    for (std::int32_t denominator = 2; denominator <= 79; ++denominator) {
        const auto approximate = packed_rows(from_raw<q8>(1234567), denominator);
        const auto expected = 1234567 / denominator;
        const auto error = gba::bit_cast(approximate) - expected;
        gba::test.expect.eq(error >= -1 && error <= 32768, true);
    }

    return gba::test.finish();
}
