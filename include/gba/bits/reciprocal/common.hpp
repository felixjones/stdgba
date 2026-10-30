#pragma once

#include <gba/bits/constexpr_assert.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal::bits {

    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    template<FixedPoint T>
    [[gnu::always_inline]] constexpr auto raw(T value) noexcept {
        return __builtin_bit_cast(typename fixed_point_traits<T>::rep, value);
    }

    template<typename T>
    concept SupportedFixed = FixedPoint<T> && sizeof(typename fixed_point_traits<T>::rep) == sizeof(u32);

    template<SupportedFixed T>
    using rep_type = fixed_point_traits<T>::rep;

    template<SupportedFixed T>
    using unsigned_rep_t = std::make_unsigned_t<rep_type<T>>;

    template<SupportedFixed T>
    constexpr unsigned_rep_t<T> magnitude(rep_type<T> value) noexcept {
        using unsigned_rep = unsigned_rep_t<T>;
        const auto bits = static_cast<unsigned_rep>(value);
        if constexpr (std::is_signed_v<rep_type<T>>) {
            return value < 0 ? unsigned_rep{0} - bits : bits;
        } else {
            return bits;
        }
    }

    template<std::integral T>
        requires(sizeof(T) <= sizeof(u32) && !std::same_as<T, bool>)
    constexpr u32 magnitude(T value) noexcept {
        const auto bits = static_cast<u32>(value);
        if constexpr (std::is_signed_v<T>) {
            return value < 0 ? u32{0} - bits : bits;
        } else {
            return bits;
        }
    }

    template<SupportedFixed T>
    constexpr bool negative(rep_type<T> value) noexcept {
        if constexpr (std::is_signed_v<rep_type<T>>) {
            return value < 0;
        } else {
            return false;
        }
    }

    template<std::integral T>
        requires(sizeof(T) <= sizeof(u32) && !std::same_as<T, bool>)
    constexpr bool negative(T value) noexcept {
        if constexpr (std::is_signed_v<T>) {
            return value < 0;
        } else {
            return false;
        }
    }

    [[gnu::always_inline]] constexpr unsigned exponent(u32 value) noexcept {
        unsigned result = 0;
        if (value >= 0x10000u) {
            value >>= 16;
            result += 16;
        }
        if (value >= 0x100u) {
            value >>= 8;
            result += 8;
        }
        if (value >= 0x10u) {
            value >>= 4;
            result += 4;
        }
        if (value >= 4u) {
            value >>= 2;
            result += 2;
        }
        if (value >= 2u) {
            ++result;
        }
        return result;
    }

    struct normalized_parts {
        u32 multiplier;
        int shift;
    };

    namespace config {
        inline constexpr u32 shift_mask = 0xffu;
        inline constexpr u32 denominator_negative = 1u << 8;
        inline constexpr u32 signed_numerator = 1u << 9;
        inline constexpr unsigned frac_shift = 10;
        inline constexpr unsigned index_bits_shift = 16;
        inline constexpr unsigned refinements_shift = 20;
        inline constexpr u32 signed_denominator = 1u << 22;
        inline constexpr u32 corrected = 1u << 23;
        inline constexpr u32 packed_u16 = 1u << 24;
        inline constexpr u32 single_step = 1u << 25;
    } // namespace config

    template<SupportedFixed T>
    inline constexpr u32 numerator_config = std::is_signed_v<rep_type<T>> ? config::signed_numerator : 0u;

    constexpr u32 apply_config(int shift, bool denominatorNegative, u32 base, unsigned frac = 0) noexcept {
        return (static_cast<u32>(shift) & config::shift_mask) |
               (denominatorNegative ? config::denominator_negative : 0u) | (u32{frac} << config::frac_shift) | base;
    }

    struct batch_apply {
        u32 count;
        u32 multiplier;
        u32 config;
        u32 denominator;
    };

    struct batch_divide {
        u32 count;
        const std::uint16_t* table;
        u32 config;
    };

    struct batch_direct {
        u32 count;
        const void* table;
        u32 min;
        u32 max;
        u32 config;
    };

    normalized_parts runtime_prepare(u32 denominator, unsigned frac, const std::uint16_t* table, unsigned indexBits,
                                     unsigned refinements) noexcept;
    u32 runtime_apply(u32 numerator, u32 multiplier, u32 config) noexcept asm("_stdgba_reciprocal_apply");
    u32 runtime_apply_corrected(u32 numerator, u32 multiplier, u32 config, u32 denominator) noexcept;
    u32 runtime_divide(u32 numerator, u32 denominator, const std::uint16_t* table, u32 config) noexcept;
    u32 runtime_direct_corrected(u32 numerator, u32 reciprocal, u32 denominator, u32 config) noexcept;
    void runtime_apply_batch(const void* numerators, void* out, const batch_apply& batch) noexcept;
    void runtime_divide_batch(const void* numerators, const void* denominators, void* out,
                              const batch_divide& batch) noexcept;
    void runtime_direct_batch(const void* numerators, const void* denominators, void* out,
                              const batch_direct& batch) noexcept;

    template<typename R>
    concept FixedInputRange = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
                              SupportedFixed<std::ranges::range_value_t<R>>;

    template<typename R>
    concept FixedOutputRange =
        FixedInputRange<R> &&
        std::is_same_v<std::remove_reference_t<std::ranges::range_reference_t<R>>, std::ranges::range_value_t<R>>;

    template<typename R>
    concept IntegerInputRange = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
                                std::integral<std::ranges::range_value_t<R>> &&
                                sizeof(std::ranges::range_value_t<R>) == sizeof(u32);

    template<typename In, typename Out>
    concept BatchPair = FixedInputRange<In> && FixedOutputRange<Out> &&
                        std::same_as<std::ranges::range_value_t<In>, std::ranges::range_value_t<Out>>;

    template<SupportedFixed T>
    constexpr u32 divide_config(unsigned indexBits, unsigned refinements) noexcept {
        return numerator_config<T> | (numerator_config<T> ? config::signed_denominator : 0u) |
               (u32{fixed_point_traits<T>::frac_bits} << config::frac_shift) |
               (u32{indexBits} << config::index_bits_shift) | (u32{refinements} << config::refinements_shift);
    }

    template<typename In, typename Out>
    constexpr u32 batch_count(In& in, Out& out) noexcept {
        const auto count = std::ranges::size(in);
        gba::bits::constexpr_assert(count != std::ranges::size(out), "reciprocal batch sizes must match");
        return static_cast<u32>(count);
    }

    constexpr double refined_error(double seedError, unsigned refinements) noexcept {
        for (unsigned i = 0; i < refinements; ++i) {
            seedError = (seedError * seedError) + 0x1p-27;
        }
        return seedError;
    }

    [[gnu::always_inline]] constexpr u32 refine(u32 normalized, u32 estimate) noexcept {
        const u32 product = static_cast<u32>((u64{normalized} * estimate) >> 31);
        return static_cast<u32>((u64{estimate} * (0x80000000u - product)) >> 30);
    }

    template<SupportedFixed T>
    constexpr T from_magnitude(u64 value, bool isNegative) noexcept {
        using rep = rep_type<T>;
        using unsigned_rep = unsigned_rep_t<T>;
        constexpr auto positive_max = static_cast<unsigned_rep>(std::numeric_limits<rep>::max());
        constexpr unsigned_rep negative_max = std::is_signed_v<rep> ? positive_max + unsigned_rep{1} : positive_max;

        gba::bits::constexpr_assert(value > (isNegative ? negative_max : positive_max),
                                    "reciprocal quotient is not representable");
        if constexpr (!std::is_signed_v<rep>) {
            gba::bits::constexpr_assert(isNegative && value != 0, "negative reciprocal quotient is not representable");
        }
        const auto narrowed = static_cast<unsigned_rep>(value);
        const auto rawBits = isNegative ? unsigned_rep{0} - narrowed : narrowed;
        const rep raw = [&] {
            if constexpr (std::is_signed_v<rep>) {
                return std::bit_cast<rep>(rawBits);
            } else {
                return static_cast<rep>(rawBits);
            }
        }();
        return __builtin_bit_cast(T, raw);
    }

    template<SupportedFixed T>
    constexpr u64 normalized_estimate(T numerator, u32 multiplier, int shift) noexcept {
        const auto raw = bits::raw(numerator);
        const auto product = u64{magnitude<T>(raw)} * multiplier;
        return shift >= 0 ? product >> static_cast<unsigned>(shift) : product << static_cast<unsigned>(-shift);
    }

    constexpr u32 correct(u64 q, u64 target, u32 d, u32 limit) noexcept {
        gba::bits::constexpr_assert(target >= (u64{limit} + 1) * d, "reciprocal quotient is not representable");
        q = std::min<u64>(q, limit);
        while (q * d > target) {
            --q;
        }
        while ((q + 1) * d <= target) {
            ++q;
        }
        return static_cast<u32>(q);
    }

    template<SupportedFixed T>
    constexpr u32 quotient_limit(bool isNegative) noexcept {
        const auto max = static_cast<u32>(std::numeric_limits<rep_type<T>>::max());
        if constexpr (std::is_signed_v<rep_type<T>>) {
            return max + (isNegative ? 1u : 0u);
        } else {
            return isNegative ? 0u : max;
        }
    }

    template<SupportedFixed T>
    constexpr T correct_magnitude(u64 q, T numerator, T denominator) noexcept {
        const auto nRaw = bits::raw(numerator);
        const auto dRaw = bits::raw(denominator);
        const u64 target = u64{magnitude<T>(nRaw)} << fixed_point_traits<T>::frac_bits;
        const bool isNegative = negative<T>(nRaw) != negative<T>(dRaw);
        return from_magnitude<T>(correct(q, target, magnitude<T>(dRaw), quotient_limit<T>(isNegative)), isNegative);
    }

    template<std::integral Denominator, SupportedFixed T>
        requires(sizeof(Denominator) <= sizeof(u32) && !std::same_as<Denominator, bool>)
    constexpr T correct_magnitude(u64 q, T numerator, Denominator denominator) noexcept {
        const auto nRaw = bits::raw(numerator);
        const bool isNegative = negative<T>(nRaw) != negative(denominator);
        return from_magnitude<T>(correct(q, magnitude<T>(nRaw), magnitude(denominator), quotient_limit<T>(isNegative)),
                                 isNegative);
    }

    normalized_parts runtime_affine_prepare(u32 denominator, unsigned frac, unsigned refinements) noexcept;
    u32 runtime_affine_divide(u32 numerator, u32 denominator, u32 config) noexcept;
    void runtime_affine_batch(const void* numerators, const void* denominators, void* out,
                              const batch_divide& batch) noexcept;

} // namespace gba::reciprocal::bits

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
