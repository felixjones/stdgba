#include "gba/bits/reciprocal/common.hpp"
#include "runtime.hpp"

#pragma GCC optimize("O3")

namespace gba::reciprocal::bits {

    using namespace runtime;

    [[gnu::target("arm"), gnu::section(".iwram.reciprocal_apply_corrected"), gnu::noinline]]
    u32 runtime_apply_corrected(u32 numerator, u32 multiplier, u32 config, u32 denominator) noexcept {
        return apply(numerator, multiplier, config | config::corrected, denominator);
    }

} // namespace gba::reciprocal::bits
