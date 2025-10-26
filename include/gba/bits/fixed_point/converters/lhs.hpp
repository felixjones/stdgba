#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>

namespace gba {

/**
 * @brief LHS (left-hand side) conversion - result matches wrapped value's type
 *
 * Best for performance when you want to keep the LHS type.
 * Converts RHS to match LHS type.
 *
 * Usage:
 *   fixed<int, 8> a = 3.5;
 *   fixed<int, 4> b = 1.25;
 *   auto result = as_lhs(a) + b;  // Result: fixed<int, 8>
 */
template<fixed_point T>
struct lhs_wrapper : conversion_wrapper_base<T, lhs_wrapper<T>> {
    using base = conversion_wrapper_base<T, lhs_wrapper<T>>;
    using base::base;

    template<fixed_point L, fixed_point R, typename Op>
    static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
        using lhs_type = std::remove_cvref_t<L>;
        return op(lhs, convert_fixed<lhs_type>(rhs));
    }
};

template<fixed_point T>
constexpr lhs_wrapper<T> as_lhs(const T& value) noexcept {
    return lhs_wrapper<T>(value);
}

} // namespace gba
