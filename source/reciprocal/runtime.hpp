#pragma once

#include <gba/bits/reciprocal/common.hpp>

namespace gba::reciprocal::bits::runtime {
    using alias_u32 [[gnu::may_alias]] = u32;
    using alias_u16 [[gnu::may_alias]] = std::uint16_t;

    struct signed_magnitude {
        u32 magnitude;
        bool negative;
    };

    [[gnu::always_inline]] inline signed_magnitude split(u32 raw, bool isSigned) noexcept {
        const bool negative = isSigned && static_cast<std::int32_t>(raw) < 0;
        return {.magnitude = negative ? 0u - raw : raw, .negative = negative};
    }

    [[gnu::always_inline]] inline u32 limit(bool signedNumerator, bool negative) noexcept {
        if (signedNumerator) {
            return 0x7fffffffu + (negative ? 1u : 0u);
        }
        return negative ? 0u : 0xffffffffu;
    }

    [[gnu::always_inline]] inline u32 finish(u64 q, bool negative, bool signedNumerator) noexcept {
        gba::bits::constexpr_assert(q > limit(signedNumerator, negative), "reciprocal quotient is not representable");
        return negative ? 0u - static_cast<u32>(q) : static_cast<u32>(q);
    }

    [[gnu::always_inline]] inline int config_shift(u32 config) noexcept {
        return static_cast<std::int8_t>(config & config::shift_mask);
    }

    [[gnu::always_inline]] inline unsigned config_frac(u32 config) noexcept {
        return (config >> config::frac_shift) & 31u;
    }

    // Both operands are written by inline assembly; const-correctness analysis cannot see this.
    // NOLINTBEGIN(misc-const-correctness,readability-non-const-parameter)
    [[gnu::always_inline]] inline unsigned runtime_normalize(u32& value) noexcept {
        unsigned result = 0;
        asm("mov %[result], #31\n\t"
            "cmp %[value], #0x10000\n\t"
            "movlo %[value], %[value], lsl #16\n\t"
            "sublo %[result], %[result], #16\n\t"
            "tst %[value], #0xff000000\n\t"
            "moveq %[value], %[value], lsl #8\n\t"
            "subeq %[result], %[result], #8\n\t"
            "tst %[value], #0xf0000000\n\t"
            "moveq %[value], %[value], lsl #4\n\t"
            "subeq %[result], %[result], #4\n\t"
            "tst %[value], #0xc0000000\n\t"
            "moveq %[value], %[value], lsl #2\n\t"
            "subeq %[result], %[result], #2\n\t"
            "tst %[value], #0x80000000\n\t"
            "moveq %[value], %[value], lsl #1\n\t"
            "subeq %[result], %[result], #1"
            : [result] "=&r"(result), [value] "+r"(value)
            :
            : "cc");
        return result;
    }

    [[gnu::always_inline]] inline u32 runtime_refine(u32 normalized, u32 estimate) noexcept {
        u32 lo = 0;
        u32 hi = 0;
        u32 result = 0;
        asm("umull %[lo], %[hi], %[normalized], %[estimate]\n\t"
            "mov %[lo], %[lo], lsr #31\n\t"
            "orr %[hi], %[lo], %[hi], lsl #1\n\t"
            "rsb %[hi], %[hi], #0x80000000\n\t"
            "umull %[lo], %[result], %[estimate], %[hi]\n\t"
            "mov %[lo], %[lo], lsr #30\n\t"
            "orr %[result], %[lo], %[result], lsl #2"
            : [lo] "=&r"(lo), [hi] "=&r"(hi), [result] "=&r"(result)
            : [normalized] "r"(normalized), [estimate] "r"(estimate));
        return result;
    }

    // NOLINTEND(misc-const-correctness,readability-non-const-parameter)

    [[gnu::always_inline]] inline normalized_parts prepare(u32 denominator, unsigned frac, const std::uint16_t* table,
                                                           unsigned indexBits, unsigned refinements) noexcept {
        u32 normalized = denominator;
        const unsigned e = runtime_normalize(normalized);
        u32 estimate = table ? u32{table[(normalized >> (31 - indexBits)) & ((1u << indexBits) - 1)]} << 14
                             : 1515870810u - static_cast<u32>((u64{505290270u} * normalized) >> 31);
        for (unsigned i = 0; i < refinements; ++i) {
            estimate = runtime_refine(normalized, estimate);
        }
        return {.multiplier = estimate, .shift = 30 + static_cast<int>(e) - static_cast<int>(frac)};
    }

    [[gnu::always_inline]] inline u64 estimate(u32 numerator, u32 multiplier, int shift) noexcept {
        const u64 product = u64{numerator} * multiplier;
        return shift >= 0 ? product >> static_cast<unsigned>(shift) : product << static_cast<unsigned>(-shift);
    }

    [[gnu::always_inline]] inline u32 correct_estimate(u64 estimate, u64 target, u32 denominator, u32 limit) noexcept {
        gba::bits::constexpr_assert(target >= (u64{limit} + 1) * denominator,
                                    "reciprocal quotient is not representable");
        u64 q = estimate > limit ? limit : estimate;
        u64 product = q * denominator;
        while (product > target) {
            --q;
            product -= denominator;
        }
        while (target - product >= denominator) {
            ++q;
            product += denominator;
        }
        return static_cast<u32>(q);
    }

    [[gnu::always_inline]] inline u32 apply(u32 raw, u32 multiplier, u32 config, u32 denominator) noexcept {
        const bool signedNumerator = (config & config::signed_numerator) != 0;
        const auto n = split(raw, signedNumerator);
        const bool negative = n.negative != ((config & config::denominator_negative) != 0);
        const u64 q = estimate(n.magnitude, multiplier, config_shift(config));
        if ((config & config::corrected) == 0) {
            return finish(q, negative, signedNumerator);
        }
        const u32 corrected = correct_estimate(q, u64{n.magnitude} << config_frac(config), denominator,
                                               limit(signedNumerator, negative));
        return negative ? 0u - corrected : corrected;
    }

    [[gnu::always_inline]] inline u32 divide(u32 nRaw, u32 dRaw, const std::uint16_t* table, u32 config) noexcept {
        const auto d = split(dRaw, (config & config::signed_denominator) != 0);
        gba::bits::constexpr_assert(d.magnitude == 0, "reciprocal denominator must not be zero");
        const auto prepared = prepare(d.magnitude, config_frac(config), table,
                                      (config >> config::index_bits_shift) & 15u,
                                      (config >> config::refinements_shift) & 3u);
        const u32 applyConfig = (static_cast<u32>(prepared.shift) & config::shift_mask) |
                                (d.negative ? config::denominator_negative : 0u) |
                                (config & ~(config::shift_mask | config::denominator_negative));
        return apply(nRaw, prepared.multiplier, applyConfig, d.magnitude);
    }

} // namespace gba::reciprocal::bits::runtime
