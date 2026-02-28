#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>

namespace gba {

    /// @brief RHS (right-hand side) conversion - result matches unwrapped operand's type
    ///
    /// Best for performance when you want the result to match the RHS type.
    /// Converts wrapped LHS to match RHS type.
    ///
    /// Usage:
    ///   fixed<int, 8> a = 3.5;
    ///   fixed<int, 4> b = 1.25;
    ///   auto result = as_rhs(a) + b;  // Result: fixed<int, 4>
    template<fixed_point T>
    struct rhs_wrapper : conversion_wrapper_base<T, rhs_wrapper<T>> {
        using base = conversion_wrapper_base<T, rhs_wrapper<T>>;
        using base::base;

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using rhs_type = std::remove_cvref_t<R>;
            return op(convert_fixed<rhs_type>(lhs), rhs);
        }
    };

    template<fixed_point T>
    constexpr rhs_wrapper<T> as_rhs(const T& value) noexcept {
        return rhs_wrapper<T>(value);
    }

} // namespace gba
