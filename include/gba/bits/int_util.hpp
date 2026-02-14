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

} // namespace gba::bits
