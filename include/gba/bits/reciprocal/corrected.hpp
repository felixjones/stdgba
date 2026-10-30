#pragma once

#include <gba/bits/reciprocal/affine.hpp>
#include <gba/bits/reciprocal/direct.hpp>
#include <gba/bits/reciprocal/normalized.hpp>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal {

    namespace bits {

        template<typename Strategy>
        struct supports_correction : std::false_type {};

        template<unsigned IndexBits, unsigned Refinements>
        struct supports_correction<normalized_strategy<IndexBits, Refinements>>
            : std::bool_constant<(IndexBits >= 8 && Refinements >= 2)> {};

        template<unsigned Refinements>
        struct supports_correction<affine_strategy<Refinements>> : std::bool_constant<(Refinements >= 3)> {};

        template<unsigned Min, unsigned Max>
        struct supports_correction<direct_strategy<Min, Max, entry_format::u32>> : std::true_type {};

        template<typename Strategy>
        inline constexpr bool supports_correction_v = supports_correction<Strategy>::value;

    } // namespace bits

    /// @brief Owning prepared divider with exact truncating correction.
    /// @tparam Prepared Divider type produced by the wrapped strategy.
    /// @tparam Denominator Original fixed-point or integer denominator type.
    /// @details Obtain a valid divider by calling a corrected_strategy.
    template<typename Prepared, typename Denominator>
    struct corrected_divider {
        Prepared prepared;       ///< Approximate reciprocal used as the correction seed.
        Denominator denominator; ///< Copy of the original denominator.

        /// @brief Divide and correct to the exact widened quotient.
        /// @param numerator Fixed-point value with 32-bit storage.
        /// @return Quotient truncated towards zero in T.
        /// @pre The quotient fits T; fixed-point denominator and numerator types match.
        template<bits::SupportedFixed T>
            requires(!FixedPoint<Denominator> || std::same_as<T, Denominator>)
        constexpr T operator()(T numerator) const noexcept {
            if !consteval {
                const auto n_raw = bits::raw(numerator);
                std::uint32_t result;
                if constexpr (FixedPoint<Denominator>) {
                    result = bits::runtime_apply_corrected(static_cast<std::uint32_t>(n_raw), prepared.multiplier,
                                                           runtime_config<T>(),
                                                           bits::magnitude<Denominator>(bits::raw(denominator)));
                } else {
                    result = bits::runtime_direct_corrected(static_cast<std::uint32_t>(n_raw), prepared.reciprocal,
                                                            prepared.denominator, runtime_config<T>());
                }
                return __builtin_bit_cast(T, result);
            }
            const auto q = prepared.estimate_magnitude(numerator);
            return bits::correct_magnitude(q, numerator, denominator);
        }

        /// @brief Apply exact division to a contiguous range.
        /// @param numerators Fixed-point input range compatible with the denominator.
        /// @param out Writable range with the same element type.
        /// @pre Ranges have equal size and each element meets the scalar preconditions.
        template<typename In, typename Out>
            requires(bits::BatchPair<In, Out> &&
                     (!FixedPoint<Denominator> || std::same_as<std::ranges::range_value_t<Out>, Denominator>))
        constexpr void operator()(In&& numerators, Out&& out) const noexcept {
            using T = std::ranges::range_value_t<Out>;
            const auto count = bits::batch_count(numerators, out);
            if !consteval {
                std::uint32_t multiplier;
                std::uint32_t d;
                if constexpr (FixedPoint<Denominator>) {
                    multiplier = prepared.multiplier;
                    d = bits::magnitude<Denominator>(bits::raw(denominator));
                } else {
                    multiplier = prepared.runtime_multiplier();
                    d = prepared.denominator;
                }
                const bits::batch_apply batch{count, multiplier, runtime_config<T>(), d};
                bits::runtime_apply_batch(std::ranges::data(numerators), std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i]);
            }
        }

        template<bits::SupportedFixed T>
        constexpr std::uint32_t runtime_config() const noexcept {
            if constexpr (FixedPoint<Denominator>) {
                return bits::apply_config(prepared.shift, prepared.denominator_negative,
                                          bits::numerator_config<T> | bits::config::corrected,
                                          fixed_point_traits<T>::frac_bits);
            } else {
                return prepared.template runtime_config<T>() | bits::config::corrected | bits::config::single_step;
            }
        }
    };

    /// @brief Correct a reciprocal strategy to exact truncation towards zero.
    /// @tparam StrategyValue normalised strategy with IndexBits >= 8 and
    /// Refinements >= 2, affine strategy with Refinements >= 3, or u32 direct strategy.
    /// @details Uses widened quotient arithmetic and shares the wrapped table.
    /// Unsupported strategy configurations fail to compile.
    template<auto StrategyValue>
        requires(bits::supports_correction_v<decltype(StrategyValue)>)
    struct corrected_strategy {
        /// Shared wrapped table size in bytes; correction adds no table.
        static constexpr std::size_t table_bytes = StrategyValue.table_bytes;
        /// @brief Prepare an exact divider for reuse.
        /// @param denominator Nonzero divisor accepted by the wrapped strategy.
        /// @return An owning corrected_divider with a copy of the denominator.
        /// @pre The denominator meets the wrapped strategy's preconditions.
        template<typename Denominator>
            requires requires(Denominator denominator) { StrategyValue(denominator); }
        constexpr auto operator()(Denominator denominator) const noexcept {
            auto prepared = StrategyValue(denominator);
            return corrected_divider<decltype(prepared), Denominator>{prepared, denominator};
        }

        /// @brief Divide with exact truncation towards zero.
        /// @param numerator Fixed-point value with 32-bit storage.
        /// @param denominator Divisor accepted by the wrapped strategy.
        /// @return The exact widened quotient truncated towards zero in T.
        /// @pre The denominator is valid for the wrapped strategy and the quotient fits T.
        template<bits::SupportedFixed T, typename Denominator>
            requires requires(Denominator denominator, T numerator) { StrategyValue(denominator)(numerator); }
        constexpr T operator()(T numerator, Denominator denominator) const noexcept {
            return (*this)(denominator)(numerator);
        }

        /// @brief Divide matching contiguous ranges with exact truncation.
        /// @param numerators Fixed-point input range.
        /// @param denominators Compatible fixed-point or 32-bit integer range.
        /// @param out Writable range with the same element type as numerators.
        /// @pre All ranges have equal size and each element meets the scalar preconditions.
        template<typename N, typename D, typename Out>
            requires(requires(N&& n, D&& d, Out&& out) { StrategyValue(n, d, out); })
        constexpr void operator()(N&& numerators, D&& denominators, Out&& out) const noexcept {
            if constexpr (requires {
                              StrategyValue.template batch<bits::config::corrected>(numerators, denominators, out);
                          }) {
                StrategyValue.template batch<bits::config::corrected>(numerators, denominators, out);
            } else {
                using T = std::ranges::range_value_t<Out>;
                const auto count = bits::batch_count(numerators, out);
                bits::batch_count(denominators, out);
                if !consteval {
                    const bits::batch_divide batch{count, StrategyValue.runtime_table(),
                                                   decltype(StrategyValue)::template divide_config<T> |
                                                       bits::config::corrected};
                    bits::runtime_divide_batch(std::ranges::data(numerators), std::ranges::data(denominators),
                                               std::ranges::data(out), batch);
                    return;
                }
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i],
                                                        std::ranges::data(denominators)[i]);
                }
            }
        }
    };

    /// @brief Callable exact reciprocal strategy.
    /// @tparam StrategyValue Strategy configuration supported by corrected_strategy.
    /// @see corrected_strategy
    template<auto StrategyValue>
        requires(bits::supports_correction_v<decltype(StrategyValue)>)
    inline constexpr corrected_strategy<StrategyValue> corrected{};

} // namespace gba::reciprocal

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
