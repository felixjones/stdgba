#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>

namespace gba {

    /**
     * @brief Conversion wrapper for widening conversions
     *
     * Widens to match the RHS operand's type if it has more fractional bits.
     * Never loses precision.
     *
     * Usage:
     *   fixed<int, 4> low = 1.25;
     *   fixed<int, 8> high = 3.5;
     *   auto result = as_widening(low) + high;  // Result: fixed<int, 8>
     */
    template<fixed_point T>
    struct widening_wrapper : conversion_wrapper_base<T, widening_wrapper<T>> {
        using base = conversion_wrapper_base<T, widening_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
            using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;

            if constexpr (rhs_traits::fractional_digits >= lhs_traits::fractional_digits) {
                using rhs_type = std::remove_cvref_t<R>;
                return op(convert_fixed<rhs_type>(lhs), rhs);
            } else {
                using lhs_type = std::remove_cvref_t<L>;
                return op(lhs, convert_fixed<lhs_type>(rhs));
            }
        }
    };

    template<fixed_point T>
    constexpr widening_wrapper<T> as_widening(const T& value) noexcept {
        return widening_wrapper<T>(value);
    }

} // namespace gba
