#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>

namespace gba {

    /// @brief Conversion wrapper for narrowing conversions
    ///
    /// Narrows to match the RHS (unwrapped) operand's type.
    /// May lose precision if RHS has fewer fractional bits.
    ///
    /// Usage:
    ///   fixed<int, 8> high = 3.75;
    ///   fixed<int, 4> low = 1.25;
    ///   auto result = as_narrowing(high) + low;  // Result: fixed<int, 4>
    template<fixed_point T>
    struct narrowing_wrapper : conversion_wrapper_base<T, narrowing_wrapper<T>> {
        using base = conversion_wrapper_base<T, narrowing_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using rhs_type = std::remove_cvref_t<R>;
            return op(convert_fixed<rhs_type>(lhs), rhs);
        }
    };

    template<fixed_point T>
    constexpr narrowing_wrapper<T> as_narrowing(const T& value) noexcept {
        return narrowing_wrapper<T>(value);
    }

} // namespace gba
