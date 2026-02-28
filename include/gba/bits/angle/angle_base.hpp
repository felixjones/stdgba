#pragma once

#include <gba/bits/int_util.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace gba {

    template<unsigned int Bits>
        requires(Bits >= 1 && Bits <= 32)
    class packed_angle;

    class angle;

    namespace literals {
        struct angle_literal;
    } // namespace literals

    template<typename>
    struct is_angle : std::false_type {};

    template<typename>
    struct is_packed_angle : std::false_type {};

    template<typename T>
    inline constexpr bool is_angle_v = is_angle<T>::value;

    template<typename T>
    inline constexpr bool is_packed_angle_v = is_packed_angle<T>::value;

    template<typename T>
    concept AngleType = is_angle<T>::value;

    template<typename T>
    concept PackedAngleType = is_packed_angle<T>::value;

    template<typename T>
    concept AnyAngleType = AngleType<T> || PackedAngleType<T>;

    /// @brief 32-bit angle type for arithmetic operations.
    ///
    /// This is the primary type for angle computations. Uses natural 32-bit
    /// unsigned overflow for wraparound behavior (no masking required).
    ///
    /// Full rotation = 2^32 steps. Convert to/from packed_angle<Bits> as needed.
    ///
    /// @note All arithmetic should use this type. packed_angle<Bits> types exist
    /// only for memory efficiency when storing many angles.
    class angle {
    public:
        /// @brief Underlying type (register-sized).
        using value_type = unsigned int;

        /// @brief Number of bits of precision.
        static constexpr unsigned int bits = 32;

        /// @brief Default constructor (zero angle).
        constexpr angle() noexcept = default;

        /// @brief Construct from raw value.
        constexpr explicit angle(value_type value) noexcept : m_data{value} {}

        /// @brief Construct from radians at compile time.
        template<std::floating_point F>
        consteval angle(F radians) noexcept;

        /// @brief Construct from angle literal.
        constexpr angle(literals::angle_literal lit) noexcept;

        /// @brief Convert from packed storage type.
        template<unsigned int Bits>
        constexpr angle(packed_angle<Bits> packed) noexcept;

        /// @brief Unary negation.
        [[nodiscard]]
        constexpr angle operator-() const noexcept {
            return angle{-m_data};
        }

        /// @brief Add angle.
        constexpr angle& operator+=(angle rhs) noexcept {
            m_data += rhs.m_data;
            return *this;
        }

        /// @brief Add raw value.
        constexpr angle& operator+=(value_type rhs) noexcept {
            m_data += rhs;
            return *this;
        }

        /// @brief Subtract angle.
        constexpr angle& operator-=(angle rhs) noexcept {
            m_data -= rhs.m_data;
            return *this;
        }

        /// @brief Subtract raw value.
        constexpr angle& operator-=(value_type rhs) noexcept {
            m_data -= rhs;
            return *this;
        }

        /// @brief Multiply by scalar.
        constexpr angle& operator*=(value_type rhs) noexcept {
            m_data *= rhs;
            return *this;
        }

        /// @brief Divide by scalar.
        constexpr angle& operator/=(value_type rhs) noexcept {
            m_data /= rhs;
            return *this;
        }

        /// @brief Shift left.
        constexpr angle& operator<<=(unsigned int shift) noexcept {
            m_data <<= shift;
            return *this;
        }

        /// @brief Shift right.
        constexpr angle& operator>>=(unsigned int shift) noexcept {
            m_data >>= shift;
            return *this;
        }

        /// @brief Addition.
        [[nodiscard]]
        friend constexpr angle operator+(angle lhs, angle rhs) noexcept {
            return angle{lhs.m_data + rhs.m_data};
        }

        /// @brief Add raw value.
        [[nodiscard]]
        friend constexpr angle operator+(angle lhs, value_type rhs) noexcept {
            return angle{lhs.m_data + rhs};
        }

        /// @brief Add raw value (reversed).
        [[nodiscard]]
        friend constexpr angle operator+(value_type lhs, angle rhs) noexcept {
            return angle{lhs + rhs.m_data};
        }

        /// @brief Subtraction.
        [[nodiscard]]
        friend constexpr angle operator-(angle lhs, angle rhs) noexcept {
            return angle{lhs.m_data - rhs.m_data};
        }

        /// @brief Subtract raw value.
        [[nodiscard]]
        friend constexpr angle operator-(angle lhs, value_type rhs) noexcept {
            return angle{lhs.m_data - rhs};
        }

        /// @brief Subtract from raw value.
        [[nodiscard]]
        friend constexpr angle operator-(value_type lhs, angle rhs) noexcept {
            return angle{lhs - rhs.m_data};
        }

        /// @brief Multiply by scalar.
        [[nodiscard]]
        friend constexpr angle operator*(angle lhs, value_type rhs) noexcept {
            return angle{lhs.m_data * rhs};
        }

        /// @brief Multiply by scalar (reversed).
        [[nodiscard]]
        friend constexpr angle operator*(value_type lhs, angle rhs) noexcept {
            return angle{lhs * rhs.m_data};
        }

        /// @brief Divide by scalar.
        [[nodiscard]]
        friend constexpr angle operator/(angle lhs, value_type rhs) noexcept {
            return angle{lhs.m_data / rhs};
        }

        /// @brief Shift left.
        [[nodiscard]]
        friend constexpr angle operator<<(angle lhs, unsigned int shift) noexcept {
            return angle{lhs.m_data << shift};
        }

        /// @brief Shift right.
        [[nodiscard]]
        friend constexpr angle operator>>(angle lhs, unsigned int shift) noexcept {
            return angle{lhs.m_data >> shift};
        }

        /// @brief Equality comparison.
        [[nodiscard]]
        friend constexpr bool operator==(angle, angle) noexcept = default;

        /// @brief Three-way comparison.
        [[nodiscard]]
        friend constexpr auto operator<=>(angle, angle) noexcept = default;

        /// @brief Get raw value.
        [[nodiscard]]
        constexpr value_type data() const noexcept {
            return m_data;
        }

    private:
        value_type m_data{};

        friend constexpr value_type bit_cast(angle) noexcept;
    };

    template<>
    struct is_angle<angle> : std::true_type {};

    /// @brief Extract raw value from angle.
    /// @param value The angle.
    /// @return The underlying 32-bit value.
    [[nodiscard]]
    constexpr angle::value_type bit_cast(angle value) noexcept {
        return value.m_data;
    }

} // namespace gba
