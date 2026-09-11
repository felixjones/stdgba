/// @file bits/embed/ppm_header.hpp
/// @brief Compile-time PPM (P6) header parsing.
#pragma once

#include <gba/bits/constexpr_assert.hpp>
#include <gba/bits/embed/common.hpp>

namespace gba::embed::bits {

    template<std::size_t Size>
    consteval image_header parse_ppm_header(const std::array<unsigned char, Size>& data) {
        std::size_t pos = 2;
        auto w = parse_uint(data.data(), Size, pos);
        auto h = parse_uint(data.data(), Size, pos);
        auto maxval = parse_uint(data.data(), Size, pos);
        ::gba::bits::constexpr_assert(w == 0 || h == 0, "PPM: invalid dimensions");
        ::gba::bits::constexpr_assert(maxval != 255, "PPM: only maxval 255 supported");
        if (pos < Size && is_ws(data[pos])) ++pos;
        return {w, h, pos, false, false, 0, 24, false, 0, 0, 0, 0};
    }

} // namespace gba::embed::bits
