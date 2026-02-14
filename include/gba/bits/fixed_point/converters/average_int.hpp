#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

    /**
     * @brief Average integer bits conversion
     *
     * Result has (lhs_int + rhs_int) / 2 integer bits.
     * Minimizes fractional bits to average the integer range.
     *
     * Usage:
     *   fixed<int, 8> a = 3.5;   // 24 int bits (32-8)
     *   fixed<int, 4> b = 1.25;  // 28 int bits (32-4)
     *   auto result = as_average_int(a) + b;  // Result: fixed<int, 6> (26 int bits)
     */
    template<fixed_point T>
    struct average_int_wrapper : conversion_wrapper_base<T, average_int_wrapper<T>> {
        using base = conversion_wrapper_base<T, average_int_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
            using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;
            using lhs_rep = typename lhs_traits::underlying_type;
            using rhs_rep = typename rhs_traits::underlying_type;

            constexpr auto lhs_int_bits = std::numeric_limits<std::make_unsigned_t<lhs_rep>>::digits -
                                          lhs_traits::fractional_digits;
            constexpr auto rhs_int_bits = std::numeric_limits<std::make_unsigned_t<rhs_rep>>::digits -
                                          rhs_traits::fractional_digits;
            constexpr auto avg_int_bits = (lhs_int_bits + rhs_int_bits) / 2;

            using result_rep = std::conditional_t<(sizeof(lhs_rep) > sizeof(rhs_rep)), lhs_rep, rhs_rep>;

            constexpr auto result_frac_bits = std::numeric_limits<std::make_unsigned_t<result_rep>>::digits -
                                              avg_int_bits;

            using result_type = fixed<result_rep, result_frac_bits>;
            return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
        }
    };

    template<fixed_point T>
    constexpr average_int_wrapper<T> as_average_int(const T& value) noexcept {
        return average_int_wrapper<T>(value);
    }

} // namespace gba
