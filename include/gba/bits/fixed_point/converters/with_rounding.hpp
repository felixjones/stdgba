#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

    /**
     * @brief Meta-converter that adds rounding to any conversion wrapper
     *
     * Wraps another converter and applies rounding during the conversion.
     * This implements the behavior that was previously in the Round template parameter.
     *
     * Usage:
     *   // Round when narrowing
     *   auto result = with_rounding(as_narrowing(high)) + low;
     *
     *   // Round when using LHS converter
     *   auto result2 = with_rounding(as_lhs(a)) + b;
     *
     * Rounding behavior:
     *   - Adds 0.5 (in source precision) before truncating for positive values
     *   - Subtracts 0.5 before truncating for negative values
     */
    template<conversion_wrapper W>
    struct rounding_wrapper {
        const W& wrapped;

        constexpr rounding_wrapper(const W& w) noexcept : wrapped(w) {}

        constexpr decltype(auto) get() const noexcept { return wrapped.get(); }

        using wrapped_type = typename W::wrapped_type;

        template<fixed_point From, fixed_point To>
        static constexpr To convert_with_rounding(const From& from) noexcept {
            using from_traits = fixed_point_traits<std::remove_cvref_t<From>>;
            using to_traits = fixed_point_traits<std::remove_cvref_t<To>>;
            using from_rep = typename from_traits::underlying_type;

            const auto from_data = __builtin_bit_cast(from_rep, from);

            constexpr auto shift_amount = from_traits::fractional_digits - to_traits::fractional_digits;

            if constexpr (shift_amount > 0) {
                const auto half = from_rep{1} << (shift_amount - 1);

                from_rep rounded_data;
                if constexpr (std::is_signed_v<from_rep>) {
                    rounded_data = from_data + (from_data >= 0 ? half : -half);
                } else {
                    rounded_data = from_data + half;
                }

                using to_rep = typename to_traits::underlying_type;
                const auto shifted = rounded_data >> shift_amount;
                const auto target_data = static_cast<to_rep>(shifted);
                return __builtin_bit_cast(To, target_data);
            } else {
                return static_cast<To>(from);
            }
        }

        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using underlying_result = decltype(W::convert_binary_result(lhs, rhs, op));

            auto result = W::convert_binary_result(lhs, rhs, op);

            return result;
        }
    };

    /**
     * @brief Helper function to create a rounding meta-wrapper
     *
     * @param wrapper Any conversion wrapper to apply rounding to
     * @return A rounding_wrapper that adds rounding behavior
     */
    template<conversion_wrapper W>
    constexpr rounding_wrapper<W> with_rounding(const W& wrapper) noexcept {
        return rounding_wrapper<W>(wrapper);
    }

    template<conversion_wrapper W>
    struct is_conversion_wrapper<rounding_wrapper<W>> : std::true_type {};

} // namespace gba
