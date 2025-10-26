#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

/**
 * @brief Next largest container conversion
 *
 * Uses the next largest integer type that can hold both operands.
 * Progression: char (8) -> short (16) -> int (32) -> long long (64)
 * Preserves maximum fractional bits from either operand.
 *
 * Usage:
 *   fixed<char, 4> a = 3.5;
 *   fixed<short, 8> b = 1.25;
 *   auto result = as_next_container(a) + b;  // Result: fixed<short, 8> or fixed<int, 8>
 */
template<fixed_point T>
struct next_container_wrapper : conversion_wrapper_base<T, next_container_wrapper<T>> {
    using base = conversion_wrapper_base<T, next_container_wrapper<T>>;
    using base::base;

    template<typename Rep>
    struct next_size {
        using type = std::conditional_t<
            sizeof(Rep) >= sizeof(long long), long long,
            std::conditional_t<
                sizeof(Rep) >= sizeof(int), long long,
                std::conditional_t<
                    sizeof(Rep) >= sizeof(short), int,
                    std::conditional_t<
                        sizeof(Rep) >= sizeof(char), short,
                        short
                    >
                >
            >
        >;
    };

    template<typename Rep>
    using next_size_t = typename next_size<Rep>::type;

    template<fixed_point L, fixed_point R, typename Op>
    static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
        using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
        using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;
        using lhs_rep = typename lhs_traits::underlying_type;
        using rhs_rep = typename rhs_traits::underlying_type;

        // Find the larger representation
        using larger_rep = std::conditional_t<
            (sizeof(lhs_rep) > sizeof(rhs_rep)), lhs_rep, rhs_rep
        >;

        // Get next larger container
        using result_rep = std::conditional_t<
            std::is_signed_v<lhs_rep> || std::is_signed_v<rhs_rep>,
            std::make_signed_t<next_size_t<larger_rep>>,
            std::make_unsigned_t<next_size_t<larger_rep>>
        >;

        // Use maximum fractional bits
        constexpr auto max_frac = (lhs_traits::fractional_digits > rhs_traits::fractional_digits)
            ? lhs_traits::fractional_digits
            : rhs_traits::fractional_digits;

        using result_type = fixed<result_rep, max_frac>;
        return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
    }
};

template<fixed_point T>
constexpr next_container_wrapper<T> as_next_container(const T& value) noexcept {
    return next_container_wrapper<T>(value);
}

} // namespace gba
