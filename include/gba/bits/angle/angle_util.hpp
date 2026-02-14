#pragma once

#include <gba/bits/angle/angle_literal.hpp>

namespace gba {

    /// @brief Get lookup table index from angle.
    /// @tparam TableBits Number of bits for table indexing.
    /// @param value The angle.
    /// @return Index into a lookup table of size 2^TableBits.
    template<unsigned int TableBits>
    [[nodiscard]]
    constexpr unsigned int lut_index(angle value) noexcept {
        static_assert(TableBits <= 32, "Table size cannot exceed angle precision");
        constexpr auto shift = 32 - TableBits;
        return static_cast<unsigned int>(bit_cast(value) >> shift);
    }

    /// @brief Interpret angle as signed for range comparisons.
    /// @param value The angle.
    /// @return Signed interpretation of the angle.
    [[nodiscard]]
    constexpr int as_signed(angle value) noexcept {
        return static_cast<int>(bit_cast(value));
    }

    /// @brief Counter-clockwise distance from a to b.
    /// @param a Starting angle.
    /// @param b Ending angle.
    /// @return Distance in the counter-clockwise direction.
    [[nodiscard]]
    constexpr unsigned int ccw_distance(angle a, angle b) noexcept {
        return bit_cast(b - a);
    }

    /// @brief Clockwise distance from a to b.
    /// @param a Starting angle.
    /// @param b Ending angle.
    /// @return Distance in the clockwise direction.
    [[nodiscard]]
    constexpr unsigned int cw_distance(angle a, angle b) noexcept {
        return bit_cast(a - b);
    }

    /// @brief Check if test lies in the CCW arc from start to end.
    /// @param start Arc start angle.
    /// @param end Arc end angle.
    /// @param test Angle to test.
    /// @return True if test is in the arc.
    [[nodiscard]]
    constexpr bool is_ccw_between(angle start, angle end, angle test) noexcept {
        return ccw_distance(start, test) <= ccw_distance(start, end);
    }

} // namespace gba
