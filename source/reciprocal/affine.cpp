#include "gba/bits/reciprocal/common.hpp"
#include "runtime.hpp"

#pragma GCC optimize("O3")

namespace gba::reciprocal::bits {

    using namespace runtime;

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_affine_prepare"), gnu::noinline]]
    normalized_parts runtime_affine_prepare(u32 denominator, unsigned frac, unsigned refinements) noexcept {
        return prepare(denominator, frac, nullptr, 0, refinements);
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_affine_divide"), gnu::noinline]]
    u32 runtime_affine_divide(u32 numerator, u32 denominator, u32 config) noexcept {
        return divide(numerator, denominator, nullptr, config);
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_affine_batch"), gnu::noinline]]
    void runtime_affine_batch(const void* numerators, const void* denominators, void* out,
                              const batch_divide& batch) noexcept {
        const auto* n = static_cast<const alias_u32*>(numerators);
        const auto* d = static_cast<const alias_u32*>(denominators);
        auto* dst = static_cast<alias_u32*>(out);
        const u32 count = batch.count;
        const u32 config = batch.config;
        for (u32 i = 0; i < count; ++i) {
            dst[i] = divide(n[i], d[i], nullptr, config);
        }
    }

} // namespace gba::reciprocal::bits
