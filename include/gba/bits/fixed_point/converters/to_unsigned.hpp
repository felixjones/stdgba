#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

    /// @brief Convert to unsigned representation
    ///
    /// Converts to an unsigned integer type with the same size.
    /// Lossy for negative values (they wrap around).
    /// Useful when you know values are non-negative and want to maximize range.
    ///
    /// Usage:
    ///   fixed<char, 4> a = 3.5;
    ///   auto result = as_unsigned(a) + b;  // Result uses unsigned char
    ///
    /// Warning: Negative values will wrap around!
    ///   fixed<char, 4> neg = -3.5;
    ///   auto bad = as_unsigned(neg);  // Wraps to large positive value!
    template<fixed_point T>
    struct unsigned_wrapper : conversion_wrapper_base<T, unsigned_wrapper<T>> {
        using base = conversion_wrapper_base<T, unsigned_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
            using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;
            using lhs_rep = typename lhs_traits::underlying_type;
            using rhs_rep = typename rhs_traits::underlying_type;

            using lhs_unsigned = std::make_unsigned_t<lhs_rep>;

            using result_rep =
                std::conditional_t<(sizeof(lhs_unsigned) > sizeof(rhs_rep)), lhs_unsigned,
                                   std::conditional_t<std::is_unsigned_v<rhs_rep>, rhs_rep, lhs_unsigned>>;

            constexpr auto max_frac = (lhs_traits::fractional_digits > rhs_traits::fractional_digits)
                                          ? lhs_traits::fractional_digits
                                          : rhs_traits::fractional_digits;

            using result_type = fixed<result_rep, max_frac>;
            return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
        }
    };

    template<fixed_point T>
    constexpr unsigned_wrapper<T> as_unsigned(const T& value) noexcept {
        return unsigned_wrapper<T>(value);
    }

} // namespace gba
