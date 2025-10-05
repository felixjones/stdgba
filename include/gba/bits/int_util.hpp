#pragma once

#include <concepts>
#include <type_traits>

namespace gba::bits {

template<std::integral T>
struct int_util {
    using signed_type = std::make_signed_t<T>;
    using unsigned_type = std::make_unsigned_t<T>;
    using promote_type = std::conditional_t<std::is_signed_v<T>, std::make_signed_t<decltype(T{} + T{})>, std::make_unsigned_t<decltype(T{} + T{})>>;
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

}
