#pragma once

#include <gba/bits/angle/packed_angle.hpp>

#include <numbers>

namespace gba::literals {

    /// @brief Compile-time angle literal type.
    ///
    /// Represents an angle value that converts to `angle` or any `packed_angle<Bits>`.
    /// Created by the `_deg` and `_rad` literals.
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
