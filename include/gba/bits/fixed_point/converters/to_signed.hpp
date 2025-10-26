#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

/**
 * @brief Convert to signed representation
 *
 * Converts to a signed integer type with the next larger size if needed.
 * Useful for operations that require signed values (e.g., negation).
 * Progression: unsigned char -> short, unsigned short -> int, unsigned int -> long long
 *
 * Usage:
 *   fixed<unsigned char, 4> a = 3.5;
 *   auto result = -as_signed(a);  // Result: fixed<short, 4> with value -3.5
 */
template<fixed_point T>
struct signed_wrapper : conversion_wrapper_base<T, signed_wrapper<T>> {
    using base = conversion_wrapper_base<T, signed_wrapper<T>>;
    using base::base;

    template<typename Rep>
    struct make_signed_safe {
        // If already signed, keep it; if unsigned, promote to next size and make signed
        using type = std::conditional_t<
            std::is_signed_v<Rep>,
            Rep,
            std::conditional_t<
                sizeof(Rep) >= sizeof(int), long long,
                std::conditional_t<
                    sizeof(Rep) >= sizeof(short), int,
                    short
                >
            >
        >;
    };

    template<typename Rep>
    using make_signed_safe_t = typename make_signed_safe<Rep>::type;

    template<fixed_point L, fixed_point R, typename Op>
    static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
        using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
        using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;
        using lhs_rep = typename lhs_traits::underlying_type;
        using rhs_rep = typename rhs_traits::underlying_type;

        // Convert LHS to signed (safe promotion if unsigned)
        using lhs_signed = make_signed_safe_t<lhs_rep>;

        // Result representation: use larger of signed LHS or RHS
        using result_rep = std::conditional_t<
            (sizeof(lhs_signed) > sizeof(rhs_rep)), lhs_signed,
            std::conditional_t<
                std::is_signed_v<rhs_rep>, rhs_rep, lhs_signed
            >
        >;

        // Keep maximum fractional bits
        constexpr auto max_frac = (lhs_traits::fractional_digits > rhs_traits::fractional_digits)
            ? lhs_traits::fractional_digits
            : rhs_traits::fractional_digits;

        using result_type = fixed<result_rep, max_frac>;
        return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
    }
};

template<fixed_point T>
constexpr signed_wrapper<T> as_signed(const T& value) noexcept {
    return signed_wrapper<T>(value);
}

} // namespace gba
