#include "gba/bits/reciprocal/common.hpp"

#include "gba/bits/constexpr_assert.hpp"
#include "runtime.hpp"

#pragma GCC optimize("O3")

namespace gba::reciprocal::bits {

    using namespace runtime;

    static_assert(config::shift_mask == 0xffu && config::denominator_negative == 0x100u &&
                  config::signed_numerator == 0x200u);

    // The assembly kernel branches to this exact symbol on an unrepresentable quotient.
    extern "C" [[noreturn]] void stdgba_reciprocal_range_error() noexcept asm("_stdgba_reciprocal_range_error");

    extern "C" [[noreturn]] void stdgba_reciprocal_range_error() noexcept {
        gba::bits::runtime_fail("reciprocal quotient is not representable");
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_apply_batch"), gnu::noinline]]
    void runtime_apply_batch(const void* numerators, void* out, const batch_apply& batch) noexcept {
        const auto* in = static_cast<const alias_u32*>(numerators);
        auto* dst = static_cast<alias_u32*>(out);
        const u32 count = batch.count;
        const u32 multiplier = batch.multiplier;
        const u32 config = batch.config;
        const bool signedNumerator = (config & config::signed_numerator) != 0;
        const bool denominatorNegative = (config & config::denominator_negative) != 0;
        const int shift = config_shift(config);

        if ((config & config::single_step) != 0) {
            const u32 denominator = batch.denominator;
            const auto right = static_cast<unsigned>(shift);
            for (u32 i = 0; i < count; ++i) {
                const auto n = split(in[i], signedNumerator);
                const bool negative = n.negative != denominatorNegative;
                u32 q = static_cast<u32>((u64{n.magnitude} * multiplier) >> right);
                if (u64{q} * denominator > n.magnitude) {
                    --q;
                }
                dst[i] = finish(q, negative, signedNumerator);
            }
        } else if ((config & config::corrected) != 0) {
            const u32 denominator = batch.denominator;
            const unsigned frac = config_frac(config);
            for (u32 i = 0; i < count; ++i) {
                const auto n = split(in[i], signedNumerator);
                const bool negative = n.negative != denominatorNegative;
                const u32 q = correct_estimate(estimate(n.magnitude, multiplier, shift), u64{n.magnitude} << frac,
                                               denominator, limit(signedNumerator, negative));
                dst[i] = negative ? 0u - q : q;
            }
        } else if (shift >= 0) {
            const auto right = static_cast<unsigned>(shift);
            for (u32 i = 0; i < count; ++i) {
                const auto n = split(in[i], signedNumerator);
                const bool negative = n.negative != denominatorNegative;
                dst[i] = finish((u64{n.magnitude} * multiplier) >> right, negative, signedNumerator);
            }
        } else {
            const auto left = static_cast<unsigned>(-shift);
            for (u32 i = 0; i < count; ++i) {
                const auto n = split(in[i], signedNumerator);
                const bool negative = n.negative != denominatorNegative;
                dst[i] = finish((u64{n.magnitude} * multiplier) << left, negative, signedNumerator);
            }
        }
    }

} // namespace gba::reciprocal::bits
