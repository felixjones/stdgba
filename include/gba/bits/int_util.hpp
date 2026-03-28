#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace gba::bits {

    template<std::integral T>
    struct int_util {
        using signed_type = std::make_signed_t<T>;
        using unsigned_type = std::make_unsigned_t<T>;
        using promote_type = std::conditional_t<std::is_signed_v<T>, std::make_signed_t<decltype(T{} + T{})>,
                                                std::make_unsigned_t<decltype(T{} + T{})>>;
    };

    /// @brief Select smallest unsigned/signed types that hold N bits.
    template<unsigned int Bits>
    struct storage_for_bits {
        using unsigned_type =
            std::conditional_t<Bits <= 8, std::uint8_t, std::conditional_t<Bits <= 16, std::uint16_t, std::uint32_t>>;
        using signed_type =
            std::conditional_t<Bits <= 8, std::int8_t, std::conditional_t<Bits <= 16, std::int16_t, std::int32_t>>;
    };

    constexpr auto shift_right(std::integral auto x, int y) noexcept {
        if (y >= 0) {
            return x >> y;
        }
        return x << -y;
    }

    constexpr auto shift_left(std::integral auto x, int y) noexcept {
        if (y >= 0) {
            return x << y;
        }
        return x >> -y;
    }

    constexpr int bit_storage_length(std::unsigned_integral auto n) noexcept {
        if (n == 0) {
            return 1;
        }
        int bits = 0;
        while (n > 0) {
            n >>= 1;
            ++bits;
        }
        return bits;
    }

    /// @brief Check if an integral value is a positive power of two.
    template<std::integral T>
    constexpr bool is_positive_power_of_two(T value) noexcept {
        if constexpr (std::is_signed_v<T>) {
            if (value <= 0) return false;
        } else {
            if (value == 0) return false;
        }
        using unsigned_t = std::make_unsigned_t<T>;
        const auto u = static_cast<unsigned_t>(value);
        return (u & (u - 1)) == 0;
    }

    /// @brief Get the shift amount for a power-of-two value.
    template<std::unsigned_integral T>
    constexpr int power_of_two_shift(T value) noexcept {
        int shift = 0;
        while ((value & T{1}) == 0) {
            value >>= 1;
            ++shift;
        }
        return shift;
    }

} // namespace gba::bits
