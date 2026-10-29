#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

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
    template<FixedPoint T>
    struct rhs_wrapper : conversion_wrapper_base<T, rhs_wrapper<T>> {
        using base = conversion_wrapper_base<T, rhs_wrapper<T>>;
        using base::base;

        template<FixedPoint L, FixedPoint R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using rhs_type = std::remove_cvref_t<R>;
            return op(convert_fixed<rhs_type>(lhs), rhs);
        }
    };

    template<FixedPoint T>
    constexpr rhs_wrapper<T> as_rhs(const T& value) noexcept {
        return rhs_wrapper<T>(value);
    }

} // namespace gba
