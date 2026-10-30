#include "gba/bits/reciprocal/common.hpp"
#include <cstdint>
#include "runtime.hpp"

#pragma GCC optimize("O3")

namespace gba::reciprocal::bits {

    using namespace runtime;

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_prepare"), gnu::noinline]]
    normalized_parts runtime_prepare(u32 denominator, unsigned frac, const std::uint16_t* table, unsigned indexBits,
                                     unsigned refinements) noexcept {
        return prepare(denominator, frac, table, indexBits, refinements);
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_divide"), gnu::noinline]]
    u32 runtime_divide(u32 numerator, u32 denominator, const std::uint16_t* table, u32 config) noexcept {
        return divide(numerator, denominator, table, config);
    }

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_divide_batch"), gnu::noinline]]
    void runtime_divide_batch(const void* numerators, const void* denominators, void* out,
                              const batch_divide& batch) noexcept {
        const auto* n = static_cast<const alias_u32*>(numerators);
        const auto* d = static_cast<const alias_u32*>(denominators);
        auto* dst = static_cast<alias_u32*>(out);
        const u32 count = batch.count;
        const auto* table = batch.table;
        const u32 config = batch.config;
        for (u32 i = 0; i < count; ++i) {
            dst[i] = divide(n[i], d[i], table, config);
        }
    }

} // namespace gba::reciprocal::bits
