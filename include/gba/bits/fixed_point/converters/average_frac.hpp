#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

    /**
     * @brief Average fractional bits conversion
     *
     * Result has (lhs_frac + rhs_frac) / 2 fractional bits.
     * Balances precision between operands.
     *
     * Usage:
     *   fixed<int, 8> a = 3.5;   // 8 frac bits
     *   fixed<int, 4> b = 1.25;  // 4 frac bits
     *   auto result = as_average_frac(a) + b;  // Result: fixed<int, 6>
     */
    template<fixed_point T>
    struct average_frac_wrapper : conversion_wrapper_base<T, average_frac_wrapper<T>> {
        using base = conversion_wrapper_base<T, average_frac_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
            using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;

            constexpr auto avg_frac = (lhs_traits::fractional_digits + rhs_traits::fractional_digits) / 2;

            using lhs_rep = typename lhs_traits::underlying_type;
            using rhs_rep = typename rhs_traits::underlying_type;
            using result_rep = std::conditional_t<(sizeof(lhs_rep) > sizeof(rhs_rep)), lhs_rep, rhs_rep>;

            using result_type = fixed<result_rep, avg_frac>;
            return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
        }
    };

    template<fixed_point T>
    constexpr average_frac_wrapper<T> as_average_frac(const T& value) noexcept {
        return average_frac_wrapper<T>(value);
    }

} // namespace gba
