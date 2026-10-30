#pragma once

#include <gba/bits/reciprocal/divider.hpp>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal {

    /// @brief Approximate fixed-point division using a table-free affine seed.
    /// @tparam Refinements Number of Newton-Raphson steps, from 0 to 3.
    template<unsigned Refinements>
        requires(Refinements <= 3)
    struct affine_strategy {
        /// Table-free strategies require no ROM table.
        static constexpr std::size_t table_bytes = 0;
        /// Bound E on reciprocal relative error, starting at 1/17 + 2^-28
        /// and applying E = E*E + 2^-27 per refinement. For ideal raw quotient
        /// magnitude Q, magnitude error from trunc(Q) is at most ceil(Q*E),
        /// before checking result representability.
        static constexpr double relative_error_bound = bits::refined_error(1.0 / 17 + 0x1p-28, Refinements);

        /// @brief Prepare a reciprocal for reuse.
        /// @tparam T Fixed-point type with 32-bit storage.
        /// @param denominator Nonzero fixed-point denominator.
        /// @return An owning normalized_divider<T>.
        /// @pre denominator is nonzero.
        template<bits::SupportedFixed T>
        constexpr auto operator()(T denominator) const noexcept {
            const auto raw = bits::raw(denominator);
            const auto d = bits::magnitude<T>(raw);
            gba::bits::constexpr_assert(d == 0, "reciprocal denominator must not be zero");
            if !consteval {
                const auto prepared = bits::runtime_affine_prepare(d, fixed_point_traits<T>::frac_bits, Refinements);
                return normalized_divider<T>{prepared.multiplier, prepared.shift, bits::negative<T>(raw)};
            }
            const unsigned e = bits::exponent(static_cast<std::uint32_t>(d));
            const std::uint32_t normalized = static_cast<std::uint32_t>(d) << (31 - e);
            std::uint32_t estimate = 1515870810u -
                                     static_cast<std::uint32_t>((std::uint64_t{505290270u} * normalized) >> 31);
            for (unsigned i = 0; i < Refinements; ++i) {
                estimate = bits::refine(normalized, estimate);
            }
            return normalized_divider<T>{estimate, static_cast<int>(30u + e - fixed_point_traits<T>::frac_bits),
                                         bits::negative<T>(raw)};
        }

        /// @brief Divide two fixed-point values of the same type.
        /// @param numerator Value to divide.
        /// @param denominator Nonzero divisor.
        /// @return An approximate quotient in T.
        /// @pre denominator is nonzero and the returned quotient fits T.
        template<bits::SupportedFixed T>
        constexpr T operator()(T numerator, T denominator) const noexcept {
            if !consteval {
                const auto n_raw = bits::raw(numerator);
                const auto d_raw = bits::raw(denominator);
                const auto d = bits::magnitude<T>(d_raw);
                gba::bits::constexpr_assert(d == 0, "reciprocal denominator must not be zero");
                const auto result = bits::runtime_affine_divide(static_cast<std::uint32_t>(n_raw),
                                                                static_cast<std::uint32_t>(d_raw), divide_config<T>);
                return __builtin_bit_cast(T, result);
            }
            return (*this)(denominator)(numerator);
        }

        /// @brief Divide matching contiguous ranges element by element.
        /// @param numerators Fixed-point input range.
        /// @param denominators Denominator range with the same element type.
        /// @param out Writable result range with the same element type.
        /// @pre All ranges have equal size and each element meets the scalar preconditions.
        template<typename N, typename D, typename Out>
            requires(bits::BatchPair<N, Out> && bits::BatchPair<D, Out>)
        constexpr void operator()(N&& numerators, D&& denominators, Out&& out) const noexcept {
            using T = std::ranges::range_value_t<Out>;
            const auto count = bits::batch_count(numerators, out);
            bits::batch_count(denominators, out);
            if !consteval {
                const bits::batch_divide batch{count, nullptr, divide_config<T>};
                bits::runtime_affine_batch(std::ranges::data(numerators), std::ranges::data(denominators),
                                           std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i],
                                                    std::ranges::data(denominators)[i]);
            }
        }

        static constexpr const std::uint16_t* runtime_table() noexcept { return nullptr; }

        template<bits::SupportedFixed T>
        static constexpr std::uint32_t divide_config = bits::divide_config<T>(0, Refinements);
    };

    /// @brief Callable table-free reciprocal strategy.
    /// @tparam Refinements Number of refinement steps, from 0 to 3.
    /// @see affine_strategy
    template<unsigned Refinements>
    inline constexpr affine_strategy<Refinements> affine{};

} // namespace gba::reciprocal

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
