#pragma once

#include <gba/bits/angle/packed_angle.hpp>

#include <numbers>

namespace gba::literals {

    /// @brief Compile-time angle literal type.
    ///
    /// Represents an angle value that converts to `angle` or any `packed_angle<Bits>`.
    /// Created by the `_deg` and `_rad` literals.
    ///
    /// Supports compile-time arithmetic operations (consteval only) to combine and
    /// scale angle literals:
    /// @code{.cpp}
    /// using namespace gba::literals;
    /// constexpr auto sum = 45_deg + 45_deg;  // 90 degrees
    /// constexpr auto doubled = 45_deg * 2;    // 90 degrees
    /// @endcode
    struct angle_literal {
        long double turns; ///< Angle as fraction of full rotation [0, 1).

        /// @brief Convert to angle.
        [[nodiscard]]
        constexpr operator angle() const noexcept {
            return angle{static_cast<angle::value_type>(turns * 4294967296.0L)};
        }

        /// @brief Convert to packed_angle.
        template<unsigned int Bits>
        [[nodiscard]]
        constexpr operator packed_angle<Bits>() const noexcept {
            return packed_angle<Bits>{static_cast<angle>(*this)};
        }

        /// @brief Add two literals at compile time.
        [[nodiscard]]
        consteval angle_literal operator+(const angle_literal& rhs) const noexcept {
            return {turns + rhs.turns};
        }

        /// @brief Subtract two literals at compile time.
        [[nodiscard]]
        consteval angle_literal operator-(const angle_literal& rhs) const noexcept {
            return {turns - rhs.turns};
        }

        /// @brief Multiply literal by scalar at compile time.
        [[nodiscard]]
        consteval angle_literal operator*(long double scalar) const noexcept {
            return {turns * scalar};
        }

        /// @brief Divide literal by scalar at compile time.
        [[nodiscard]]
        consteval angle_literal operator/(long double scalar) const noexcept {
            return {turns / scalar};
        }

        /// @brief Negate the angle literal.
        [[nodiscard]]
        consteval angle_literal operator-() const noexcept {
            return {-turns};
        }

        /// @brief Multiply literal by unsigned integer at runtime.
        ///
        /// Implicitly converts the literal to angle and multiplies.
        [[nodiscard]]
        constexpr angle operator*(unsigned int scalar) const noexcept {
            return angle{*this} * scalar;
        }

        /// @brief Divide literal by unsigned integer at runtime.
        ///
        /// Implicitly converts the literal to angle and divides.
        [[nodiscard]]
        constexpr angle operator/(unsigned int scalar) const noexcept {
            return angle{*this} / scalar;
        }
    };

    /// @brief Literal for angle from degrees.
    /// @param degrees Angle in degrees.
    /// @return angle_literal that converts to any angle type.
    ///
    /// Example:
    /// @code{.cpp}
    /// using namespace gba::literals;
    ///
    /// gba::angle heading = 45_deg;         // Full precision
    /// gba::packed_angle16 stored = 90_deg; // 16-bit storage
    /// auto rotated = heading + 180_deg;    // Arithmetic
    /// @endcode
    [[nodiscard]]
    consteval angle_literal operator""_deg(long double degrees) noexcept {
        return {degrees / 360.0L};
    }

    /// @copydoc operator""_deg(long double)
    [[nodiscard]]
    consteval angle_literal operator""_deg(unsigned long long degrees) noexcept {
        return {static_cast<long double>(degrees) / 360.0L};
    }

    /// @brief Multiply scalar by literal at compile time (reverse operand order).
    [[nodiscard]]
    consteval angle_literal operator*(long double scalar, const angle_literal& rhs) noexcept {
        return {rhs.turns * scalar};
    }

    /// @brief Multiply unsigned integer by literal (reverse operand order).
    ///
    /// Implicitly converts the literal to angle and multiplies.
    [[nodiscard]]
    constexpr angle operator*(unsigned int scalar, const angle_literal& rhs) noexcept {
        return rhs * scalar;
    }

    /// @brief Divide scalar by literal at compile time (reverse operand order).
    ///
    /// Note: dividing a scalar by an angle (e.g., `1.0 / 45_deg`) is unusual.
    /// Typically used only for special angle ratio calculations.
    [[nodiscard]]
    consteval angle_literal operator/(long double scalar, const angle_literal& rhs) noexcept {
        if (rhs.turns == 0.0L) throw "division by zero angle";
        return {scalar / rhs.turns};
    }

    /// @brief Literal for angle from radians.
    /// @param radians Angle in radians.
    /// @return angle_literal that converts to any angle type.
    ///
    /// Example:
    /// @code{.cpp}
    /// using namespace gba::literals;
    /// gba::angle quarter = 1.5708_rad; // pi/2
    /// @endcode
    [[nodiscard]]
    consteval angle_literal operator""_rad(long double radians) noexcept {
        constexpr auto two_pi = 2.0L * std::numbers::pi_v<long double>;
        return {radians / two_pi};
    }

} // namespace gba::literals

namespace gba {

    template<std::floating_point F>
    consteval angle::angle(F radians) noexcept
        : m_data{static_cast<value_type>((radians / (2 * std::numbers::pi_v<F>)) * 4294967296.0)} {}

    constexpr angle::angle(literals::angle_literal lit) noexcept
        : m_data{static_cast<value_type>(lit.turns * 4294967296.0L)} {}

    template<unsigned int Bits>
        requires(Bits >= 1 && Bits <= 32)
    consteval packed_angle<Bits>::packed_angle(literals::angle_literal lit) noexcept
        : packed_angle{static_cast<angle>(lit)} {}

    template<unsigned int Bits>
        requires(Bits >= 1 && Bits <= 32)
    constexpr packed_angle<Bits>& packed_angle<Bits>::operator=(literals::angle_literal lit) noexcept {
        const auto full = static_cast<angle::value_type>(lit.turns * 4294967296.0L);
        m_data = static_cast<storage_type>(full >> shift);
        return *this;
    }

} // namespace gba
