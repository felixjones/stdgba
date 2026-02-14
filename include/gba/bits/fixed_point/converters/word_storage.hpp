#pragma once

#include <gba/bits/fixed_point/conversion_wrapper.hpp>
#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

    /**
     * @brief Conversion wrapper that uses word-sized storage for GBA performance
     *
     * On GBA, word-sized types (int/unsigned int, 32-bit) are fastest.
     * This converter promotes storage to word size while preserving fractional bits.
     *
     * Usage:
     *   fixed<char, 4> small = 3.5;     // 8-bit storage
     *   fixed<short, 6> medium = 1.25;  // 16-bit storage
     *   auto result = as_word_storage(small) + medium;  // Uses int (32-bit) for performance
     */
    template<fixed_point T>
    struct word_storage_wrapper : conversion_wrapper_base<T, word_storage_wrapper<T>> {
        using base = conversion_wrapper_base<T, word_storage_wrapper<T>>;
        using base::base;

        /**
         * @brief Convert binary operation result to use word-sized storage
         *
         * Promotes both operands to use int (signed) or unsigned int storage
         * while preserving their fractional bit precision.
         */
        template<fixed_point L, fixed_point R, typename Op>
        static constexpr auto convert_binary_result(const L& lhs, const R& rhs, Op op) noexcept {
            using lhs_traits = fixed_point_traits<std::remove_cvref_t<L>>;
            using rhs_traits = fixed_point_traits<std::remove_cvref_t<R>>;
            using lhs_rep = typename lhs_traits::underlying_type;
            using rhs_rep = typename rhs_traits::underlying_type;

            constexpr bool needs_signed = std::is_signed_v<lhs_rep> || std::is_signed_v<rhs_rep>;
            using word_type = std::conditional_t<needs_signed, int, unsigned int>;

            constexpr auto max_frac = (lhs_traits::fractional_digits > rhs_traits::fractional_digits)
                                          ? lhs_traits::fractional_digits
                                          : rhs_traits::fractional_digits;

            using result_type = fixed<word_type, max_frac>;

            return op(convert_fixed<result_type>(lhs), convert_fixed<result_type>(rhs));
        }
    };

    /**
     * @brief Helper function to create a word-storage wrapper
     *
     * Use this for GBA performance when working with sub-word types.
     *
     * @param value The fixed-point value to wrap for word-storage conversion
     * @return A word_storage_wrapper that uses int/unsigned int for calculations
     */
    template<fixed_point T>
    constexpr word_storage_wrapper<T> as_word_storage(const T& value) noexcept {
        return word_storage_wrapper<T>(value);
    }

} // namespace gba
