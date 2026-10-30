#include "gba/bits/reciprocal/common.hpp"
#include "gba/bits/constexpr_assert.hpp"
#include "runtime.hpp"

#pragma GCC optimize("O3")

namespace gba::reciprocal::bits {

    using namespace runtime;

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_direct_corrected"), gnu::noinline]]
    u32 runtime_direct_corrected(u32 numerator, u32 reciprocal, u32 denominator, u32 config) noexcept {
        const bool signedNumerator = (config & config::signed_numerator) != 0;
        const auto n = split(numerator, signedNumerator);
        const bool negative = n.negative != ((config & config::denominator_negative) != 0);
        u32 q = n.magnitude;
        if (denominator != 1) {
            q = static_cast<u32>((u64{n.magnitude} * reciprocal) >> 32);
            if (u64{q} * denominator > n.magnitude) {
                --q;
            }
        }
        return finish(q, negative, signedNumerator);
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_direct_batch"), gnu::noinline]]
    void runtime_direct_batch(const void* numerators, const void* denominators, void* out,
                              const batch_direct& batch) noexcept {
        const auto* nRaw = static_cast<const alias_u32*>(numerators);
        const auto* dRaw = static_cast<const alias_u32*>(denominators);
        auto* dst = static_cast<alias_u32*>(out);
        const u32 count = batch.count;
        const u32 config = batch.config;
        const u32 min = batch.min;
        const u32 span = batch.max - min;
        const bool signedNumerator = (config & config::signed_numerator) != 0;
        const bool signedDenominator = (config & config::signed_denominator) != 0;
        const bool corrected = (config & config::corrected) != 0;
        const bool packed = (config & config::packed_u16) != 0;
        const auto* table32 = static_cast<const alias_u32*>(batch.table);
        const auto* table16 = static_cast<const alias_u16*>(batch.table);

        for (u32 i = 0; i < count; ++i) {
            const auto n = split(nRaw[i], signedNumerator);
            const auto d = split(dRaw[i], signedDenominator);
            const u32 index = d.magnitude - min;
            gba::bits::constexpr_assert(index > span,
                                        "integer denominator must be nonzero and within reciprocal table range");
            const bool negative = n.negative != d.negative;
            u32 q = n.magnitude;
            if (d.magnitude != 1) {
                if (packed) {
                    q = static_cast<u32>((u64{n.magnitude} * table16[index]) >> 16);
                } else {
                    q = static_cast<u32>((u64{n.magnitude} * table32[index]) >> 32);
                }
                if (corrected && u64{q} * d.magnitude > n.magnitude) {
                    --q;
                }
            }
            dst[i] = finish(q, negative, signedNumerator);
        }
    }

} // namespace gba::reciprocal::bits
