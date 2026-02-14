#pragma once

#include <gba/bits/angle/angle_base.hpp>

namespace gba {

    /**
     * @brief Compact angle storage with specified bit precision.
     *
     * Storage type for angles when memory efficiency matters (arrays, structs).
     * Implicitly converts to `angle` for arithmetic operations.
     *
     * @tparam Bits Number of bits of precision (1-32).
     *
     * Common configurations:
     * - `packed_angle<16>` / `angle16` - BIOS compatible, 65536 steps
     * - `packed_angle<8>` / `angle8` - Compact, 256 steps
     * - `packed_angle<12>` - 4096 steps (common for lookup tables)
     *
     * Example:
     * @code{.cpp}
     * using namespace gba::literals;
     *
     * // Array of compact angles
     * std::array<gba::angle8, 100> directions;
     *
     * // Convert to angle for computation
     * gba::angle current = directions[0];
     * current += 45_deg;
     *
     * // Store back
     * directions[0] = current;
     * @endcode
     */
    template<unsigned int Bits>
        requires(Bits >= 1 && Bits <= 32)
    class packed_angle {
    public:
        /// @brief Underlying storage type (minimal size).
        using storage_type = typename bits::storage_for_bits<Bits>::unsigned_type;

        /// @brief Number of bits of precision.
        static constexpr unsigned int bits = Bits;

        /// @brief Shift amount for conversion to/from 32-bit.
        static constexpr unsigned int shift = 32 - Bits;

        /// @brief Default constructor (zero angle).
        constexpr packed_angle() noexcept = default;

        /// @brief Construct from raw storage value.
        constexpr explicit packed_angle(storage_type value) noexcept : m_data{value} {}

        /// @brief Construct from angle (truncates to precision).
        constexpr packed_angle(angle a) noexcept : m_data{static_cast<storage_type>(a.data() >> shift)} {}

        /// @brief Construct from radians at compile time.
        template<std::floating_point F>
        consteval packed_angle(F radians) noexcept : packed_angle{angle{radians}} {}

        /// @brief Construct from angle literal.
        consteval packed_angle(literals::angle_literal lit) noexcept;

        /// @brief Convert from different precision (explicit).
        template<unsigned int OtherBits>
            requires(OtherBits != Bits)
        constexpr explicit packed_angle(packed_angle<OtherBits> other) noexcept : packed_angle{angle{other}} {}

        /// @brief Assign from angle (truncates to precision).
        constexpr packed_angle& operator=(angle a) noexcept {
            m_data = static_cast<storage_type>(a.data() >> shift);
            return *this;
        }

        /// @brief Assign from angle literal.
        constexpr packed_angle& operator=(literals::angle_literal lit) noexcept;

        /// @brief Convert to angle.
        [[nodiscard]]
        constexpr operator angle() const noexcept {
            return angle{static_cast<angle::value_type>(m_data) << shift};
        }

        /// @brief Get raw storage value.
        [[nodiscard]]
        constexpr storage_type data() const noexcept {
            return m_data;
        }

        /// @brief Equality comparison.
        [[nodiscard]]
        friend constexpr bool operator==(packed_angle, packed_angle) noexcept = default;

        /// @brief Three-way comparison.
        [[nodiscard]]
        friend constexpr auto operator<=>(packed_angle, packed_angle) noexcept = default;

    private:
        storage_type m_data{};
    };

    template<unsigned int Bits>
    struct is_packed_angle<packed_angle<Bits>> : std::true_type {};

    template<unsigned int Bits>
    constexpr angle::angle(packed_angle<Bits> packed) noexcept
        : m_data{static_cast<value_type>(packed.data()) << packed_angle<Bits>::shift} {}

    /// @brief 32-bit packed angle (same precision as angle).
    using packed_angle32 = packed_angle<32>;

    /// @brief 16-bit packed angle (GBA BIOS format).
    using packed_angle16 = packed_angle<16>;

    /// @brief 8-bit packed angle (compact, 256 steps).
    using packed_angle8 = packed_angle<8>;

    /// @brief Extract raw storage value from packed_angle.
    /// @param value The packed_angle.
    /// @return The underlying storage value.
    template<unsigned int Bits>
    [[nodiscard]]
    constexpr typename packed_angle<Bits>::storage_type bit_cast(packed_angle<Bits> value) noexcept {
        return value.data();
    }

    /// @brief Traits for angle types.
    template<typename T>
    struct angle_traits;

    template<>
    struct angle_traits<angle> {
        using value_type = angle::value_type;
        static constexpr unsigned int bits = 32;
    };

    template<unsigned int Bits>
    struct angle_traits<packed_angle<Bits>> {
        using storage_type = typename packed_angle<Bits>::storage_type;
        static constexpr unsigned int bits = Bits;
    };

} // namespace gba
