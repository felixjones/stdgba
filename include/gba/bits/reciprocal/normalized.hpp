#pragma once

#include <gba/bits/reciprocal/divider.hpp>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal::bits {

    template<unsigned IndexBits>
    consteval auto make_seeds() {
        static_assert(IndexBits >= 1 && IndexBits <= 10);
        std::array<std::uint16_t, 1u << IndexBits> table{};
        for (u32 i = 0; i < table.size(); ++i) {
            const u32 denominator = (2u << IndexBits) + 2u * i + 1u;
            table[i] = static_cast<std::uint16_t>(((u64{1} << (17 + IndexBits)) + denominator / 2u) / denominator);
        }
        return table;
    }

    template<unsigned IndexBits>
    inline constexpr auto seeds = make_seeds<IndexBits>();

    template<unsigned IndexBits, unsigned Refinements, typename T>
    constexpr auto prepare_normalized(T denominator) noexcept {
        const auto raw = bits::raw(denominator);
        const auto d = magnitude<T>(raw);
        gba::bits::constexpr_assert(d == 0, "reciprocal denominator must not be zero");

        if !consteval {
            return runtime_prepare(d, fixed_point_traits<T>::frac_bits, seeds<IndexBits>.data(), IndexBits,
                                   Refinements);
        }
        const unsigned e = exponent(static_cast<u32>(d));
        const u32 normalized = static_cast<u32>(d) << (31 - e);
        u32 estimate = u32{seeds<IndexBits>[(normalized >> (31 - IndexBits)) & ((1u << IndexBits) - 1)]} << 14;
        for (unsigned i = 0; i < Refinements; ++i) {
            estimate = refine(normalized, estimate);
        }
        return normalized_parts{estimate,
                                30 + static_cast<int>(e) - static_cast<int>(fixed_point_traits<T>::frac_bits)};
    }

} // namespace gba::reciprocal::bits

namespace gba::reciprocal {

    /// @brief Approximate fixed-point division using a normalised table seed.
    /// @tparam IndexBits Log2 of the seed count, from 1 to 10.
    /// @tparam Refinements Number of Newton-Raphson steps, from 0 to 3.
    /// @details Configurations with the same IndexBits share a ROM seed table.
    template<unsigned IndexBits, unsigned Refinements>
        requires(IndexBits >= 1 && IndexBits <= 10 && Refinements <= 3)
    struct normalized_strategy {
        /// Shared seed table size in bytes.
        static constexpr std::size_t table_bytes = sizeof(bits::seeds<IndexBits>);
        /// Bound E on reciprocal relative error, starting at 2^(-IndexBits-1) + 2^-16
        /// and applying E = E*E + 2^-27 per refinement. For ideal raw quotient
        /// magnitude Q, magnitude error from trunc(Q) is at most ceil(Q*E),
        /// before checking result representability.
        static constexpr double relative_error_bound = bits::refined_error(1.0 / (2u << IndexBits) + 0x1p-16,
                                                                           Refinements);

        /// @brief Prepare a reciprocal for reuse.
        /// @tparam T Fixed-point type with 32-bit storage.
        /// @param denominator Nonzero fixed-point denominator.
        /// @return An owning normalized_divider<T>.
        /// @pre denominator is nonzero.
        template<bits::SupportedFixed T>
        constexpr auto operator()(T denominator) const noexcept {
            const auto prepared = bits::prepare_normalized<IndexBits, Refinements>(denominator);
            const auto raw = bits::raw(denominator);
            return normalized_divider<T>{prepared.multiplier, prepared.shift, bits::negative<T>(raw)};
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
                const auto result = bits::runtime_divide(static_cast<std::uint32_t>(n_raw),
                                                         static_cast<std::uint32_t>(d_raw), runtime_table(),
                                                         divide_config<T>);
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
                const bits::batch_divide batch{count, runtime_table(), divide_config<T>};
                bits::runtime_divide_batch(std::ranges::data(numerators), std::ranges::data(denominators),
                                           std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i],
                                                    std::ranges::data(denominators)[i]);
            }
        }

        static constexpr const std::uint16_t* runtime_table() noexcept { return bits::seeds<IndexBits>.data(); }

        template<bits::SupportedFixed T>
        static constexpr std::uint32_t divide_config = bits::divide_config<T>(IndexBits, Refinements);
    };

    /// @brief Callable normalised reciprocal strategy.
    /// @tparam IndexBits Log2 of the seed count, from 1 to 10.
    /// @tparam Refinements Number of refinement steps, from 0 to 3.
    /// @see normalized_strategy
    template<unsigned IndexBits, unsigned Refinements>
    inline constexpr normalized_strategy<IndexBits, Refinements> normalized{};

} // namespace gba::reciprocal

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
