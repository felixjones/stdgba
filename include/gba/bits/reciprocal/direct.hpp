#pragma once

#include <gba/bits/reciprocal/common.hpp>

#if !defined(__OPTIMIZE__) && !defined(__clang__)
#define GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC push_options
#pragma GCC optimize("O2")
#endif

namespace gba::reciprocal::bits {

    template<unsigned Min, unsigned Max, typename Entry>
    consteval auto make_direct_table() {
        static_assert(Min >= 1 && Max >= Min);
        std::array<Entry, Max - Min + 1> table{};
        for (unsigned i = 0; i < table.size(); ++i) {
            const u32 denominator = Min + i;
            if (denominator == 1) {
                table[i] = 0;
            } else if constexpr (sizeof(Entry) == sizeof(u32)) {
                table[i] = static_cast<Entry>(((u64{1} << 32) + denominator - 1) / denominator);
            } else {
                table[i] = static_cast<Entry>((u64{65536} + denominator - 1) / denominator);
            }
        }
        return table;
    }

} // namespace gba::reciprocal::bits

namespace gba::reciprocal {

    /// @brief Storage precision of direct reciprocal table entries.
    enum class entry_format {
        u32,       ///< 32-bit entries storing ceil(2^32 / d).
        packed_u16 ///< 16-bit entries storing ceil(2^16 / d), with larger error.
    };

    /// @brief Owning reciprocal prepared by a direct_strategy.
    /// @tparam Min Minimum denominator magnitude, at least 1.
    /// @tparam Max Maximum denominator magnitude, at least Min.
    /// @tparam Format Reciprocal table entry precision.
    /// @details Obtain a valid divider by calling direct(denominator).
    template<unsigned Min, unsigned Max, entry_format Format>
        requires(Min >= 1 && Max >= Min)
    struct direct_divider {
        using entry_type = std::conditional_t<Format == entry_format::u32, std::uint32_t, std::uint16_t>;

        std::uint32_t denominator; ///< Denominator magnitude.
        entry_type reciprocal;     ///< Table entry; zero is a sentinel for denominator 1.
        bool denominator_negative; ///< Sign of the original denominator.

        /// @brief Estimate the unsigned raw quotient magnitude before range checking.
        /// @param numerator Fixed-point value with 32-bit storage.
        /// @return Truncated reciprocal product without applying the quotient sign.
        template<bits::SupportedFixed T>
        constexpr std::uint64_t estimate_magnitude(T numerator) const noexcept {
            const auto raw = bits::raw(numerator);
            const auto n = bits::magnitude<T>(raw);
            if (denominator == 1) {
                return n;
            } else if constexpr (Format == entry_format::u32) {
                return (std::uint64_t{n} * reciprocal) >> 32;
            } else {
                return (std::uint64_t{n} * reciprocal) >> 16;
            }
        }

        /// @brief Divide using the prepared integer denominator.
        /// @param numerator Fixed-point value with 32-bit storage.
        /// @return An approximate quotient with the numerator's type.
        /// @pre The returned quotient is representable in the numerator's type.
        template<bits::SupportedFixed T>
        constexpr T operator()(T numerator) const noexcept {
            const auto raw = bits::raw(numerator);
            const bool is_negative = bits::negative<T>(raw) != denominator_negative;
            if !consteval {
                const auto result = bits::runtime_apply(static_cast<std::uint32_t>(raw), runtime_multiplier(),
                                                        runtime_config<T>());
                return __builtin_bit_cast(T, result);
            }
            return bits::from_magnitude<T>(estimate_magnitude(numerator), is_negative);
        }

        /// @brief Apply the prepared divider to a contiguous range.
        /// @param numerators Fixed-point input range.
        /// @param out Writable range with the same element type.
        /// @pre Ranges have equal size and each quotient is representable.
        template<typename In, typename Out>
            requires(bits::BatchPair<In, Out>)
        constexpr void operator()(In&& numerators, Out&& out) const noexcept {
            using T = std::ranges::range_value_t<Out>;
            const auto count = bits::batch_count(numerators, out);
            if !consteval {
                const bits::batch_apply batch{count, runtime_multiplier(), runtime_config<T>(), 0};
                bits::runtime_apply_batch(std::ranges::data(numerators), std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                std::ranges::data(out)[i] = (*this)(std::ranges::data(numerators)[i]);
            }
        }

        constexpr std::uint32_t runtime_multiplier() const noexcept { return denominator == 1 ? 1u : reciprocal; }

        template<bits::SupportedFixed T>
        constexpr std::uint32_t runtime_config() const noexcept {
            return bits::apply_config(denominator == 1 ? 0 : (Format == entry_format::u32 ? 32 : 16),
                                      denominator_negative, bits::numerator_config<T>);
        }
    };

    /// @brief Approximate division by bounded integer denominator magnitudes.
    /// @tparam Min Minimum denominator magnitude, at least 1.
    /// @tparam Max Maximum denominator magnitude, at least Min.
    /// @tparam Format Precision of each shared ROM table entry.
    /// @details For P = 32 (u32) or 16 (packed_u16), the estimated raw magnitude
    /// is at least the exact truncated magnitude and exceeds it by at most
    /// ceil(abs(numerator_raw) / 2^P). Negative results reverse the error direction.
    /// Denominator 1 is handled exactly without using its sentinel table entry.
    template<unsigned Min, unsigned Max, entry_format Format>
        requires(Min >= 1 && Max >= Min)
    struct direct_strategy {
        using entry_type = std::conditional_t<Format == entry_format::u32, std::uint32_t, std::uint16_t>;

        /// Shared compile-time reciprocal table.
        inline static constexpr auto table = bits::make_direct_table<Min, Max, entry_type>();
        /// Table payload size in bytes.
        static constexpr std::size_t table_bytes = sizeof(table);

        /// @brief Prepare an integer denominator for reuse.
        /// @param denominator Signed or unsigned integer of at most 32 bits, excluding bool.
        /// @return An owning direct_divider retaining the magnitude and sign.
        /// @pre Min <= abs(denominator) <= Max.
        template<std::integral Denominator>
            requires(sizeof(Denominator) <= sizeof(std::uint32_t) && !std::same_as<Denominator, bool>)
        constexpr auto operator()(Denominator denominator) const noexcept {
            const auto d = bits::magnitude(denominator);
            gba::bits::constexpr_assert(d == 0 || d < Min || d > Max,
                                        "integer denominator must be nonzero and within reciprocal table range");
            return direct_divider<Min, Max, Format>{d, table[d - Min], bits::negative(denominator)};
        }

        /// @brief Divide a fixed-point value by an integer.
        /// @param numerator Fixed-point value with 32-bit storage.
        /// @param denominator Integer divisor whose magnitude is within the table.
        /// @return An approximate quotient in T.
        /// @pre Min <= abs(denominator) <= Max and the returned quotient fits T.
        template<bits::SupportedFixed T, std::integral Denominator>
            requires(sizeof(Denominator) <= sizeof(std::uint32_t) && !std::same_as<Denominator, bool>)
        constexpr T operator()(T numerator, Denominator denominator) const noexcept {
            return (*this)(denominator)(numerator);
        }

        /// @brief Divide a contiguous fixed-point range by a 32-bit integer range.
        /// @param numerators Fixed-point input range.
        /// @param denominators Signed or unsigned 32-bit integer range.
        /// @param out Writable range with the same element type as numerators.
        /// @pre All ranges have equal size and each element meets the scalar preconditions.
        template<typename N, typename D, typename Out>
            requires(bits::BatchPair<N, Out> && bits::IntegerInputRange<D>)
        constexpr void operator()(N&& numerators, D&& denominators, Out&& out) const noexcept {
            batch<0>(numerators, denominators, out);
        }

        template<std::uint32_t Flags, typename N, typename D, typename Out>
        constexpr void batch(N& numerators, D& denominators, Out& out) const noexcept {
            using T = std::ranges::range_value_t<Out>;
            using Denominator = std::ranges::range_value_t<D>;
            const auto count = bits::batch_count(numerators, out);
            bits::batch_count(denominators, out);
            if !consteval {
                const bits::batch_direct batch{
                    count, table.data(), Min, Max,
                    Flags | bits::numerator_config<T> |
                        (std::is_signed_v<Denominator> ? bits::config::signed_denominator : 0u) |
                        (Format == entry_format::packed_u16 ? bits::config::packed_u16 : 0u)};
                bits::runtime_direct_batch(std::ranges::data(numerators), std::ranges::data(denominators),
                                           std::ranges::data(out), batch);
                return;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto divider = (*this)(std::ranges::data(denominators)[i]);
                const auto numerator = std::ranges::data(numerators)[i];
                if constexpr ((Flags & bits::config::corrected) != 0) {
                    std::ranges::data(out)[i] = bits::correct_magnitude(divider.estimate_magnitude(numerator),
                                                                        numerator, std::ranges::data(denominators)[i]);
                } else {
                    std::ranges::data(out)[i] = divider(numerator);
                }
            }
        }
    };

    /// @brief Callable direct reciprocal strategy.
    /// @tparam Min Minimum denominator magnitude, at least 1.
    /// @tparam Max Maximum denominator magnitude, at least Min.
    /// @tparam Format Entry precision; defaults to 32 bits.
    /// @see direct_strategy
    template<unsigned Min, unsigned Max, entry_format Format = entry_format::u32>
    inline constexpr direct_strategy<Min, Max, Format> direct{};

} // namespace gba::reciprocal

#ifdef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#undef GBA_RECIPROCAL_PUSHED_OPTIMIZE
#pragma GCC pop_options
#endif
