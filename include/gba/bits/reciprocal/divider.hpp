#pragma once

#include <gba/bits/reciprocal/common.hpp>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal {

    /// @brief Owning reciprocal prepared by a normalised or affine strategy.
    /// @tparam T Fixed-point numerator and denominator type with 32-bit storage.
    /// @details Obtain a valid divider by calling a strategy with a nonzero denominator.
    template<bits::SupportedFixed T>
    struct normalized_divider {
        std::uint32_t multiplier;  ///< Prepared reciprocal multiplier.
        int shift;                 ///< Product shift accounting for denominator scale.
        bool denominator_negative; ///< Sign of the original denominator.

        /// @brief Estimate the unsigned raw quotient magnitude before range checking.
        /// @param numerator Value to divide.
        /// @return Shifted reciprocal product without applying the quotient sign.
        constexpr std::uint64_t estimate_magnitude(T numerator) const noexcept {
            return bits::normalized_estimate(numerator, multiplier, shift);
        }

        /// @brief Divide using the prepared reciprocal.
        /// @param numerator Value of the original denominator's type.
        /// @return An approximate quotient in T.
        /// @pre The returned quotient is representable in T.
        template<std::same_as<T> Numerator>
        constexpr T operator()(Numerator numerator) const noexcept {
            const auto raw = bits::raw(numerator);
            const bool is_negative = bits::negative<T>(raw) != denominator_negative;
            if !consteval {
                const auto result =
                    bits::runtime_apply(static_cast<std::uint32_t>(raw), multiplier,
                                        bits::apply_config(shift, denominator_negative, bits::numerator_config<T>));
                return __builtin_bit_cast(T, result);
            }
            return bits::from_magnitude<T>(estimate_magnitude(numerator), is_negative);
        }

        /// @brief Apply the prepared divider to a contiguous range.
        /// @param numerators Input range with element type T.
        /// @param out Writable result range with element type T.
        /// @pre Ranges have equal size and each quotient is representable in T.
        template<typename In, typename Out>
            requires(bits::BatchPair<In, Out> && std::same_as<std::ranges::range_value_t<Out>, T>)
        constexpr void operator()(In&& numerators, Out&& out) const noexcept {
            const auto count = bits::batch_count(numerators, out);
            if !consteval {
                const bits::batch_apply batch{
                    count, multiplier, bits::apply_config(shift, denominator_negative, bits::numerator_config<T>), 0};
                bits::runtime_apply_batch(std::ranges::data(numerators), std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i]);
            }
        }
    };

} // namespace gba::reciprocal

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
