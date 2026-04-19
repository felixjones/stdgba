/// @file bits/text/tile_allocator.hpp
/// @brief text linear tile allocator.

#pragma once

#include <cstdint>

namespace gba::text {

    static constexpr std::uint16_t no_tile = 0xFFFFu;
    static constexpr std::uint8_t no_plane = 255u;

    /// @brief Encode (vram_tile, plane_index) into a cell state word.
    [[nodiscard]]
    constexpr std::uint16_t pack_cell_state(std::uint16_t vram_tile, std::uint8_t plane) noexcept {
        if (vram_tile == no_tile) return 0xFFFFu;
        return static_cast<std::uint16_t>((vram_tile & 0x3FFu) | ((plane & 0x3u) << 10));
    }

    /// @brief Decode VRAM tile from cell state.
    [[nodiscard]]
    constexpr std::uint16_t unpack_tile(std::uint16_t state) noexcept {
        if (state == 0xFFFFu) return no_tile;
        return state & 0x3FFu;
    }

    /// @brief Decode plane index from cell state.
    [[nodiscard]]
    constexpr std::uint8_t unpack_plane(std::uint16_t state) noexcept {
        if (state == 0xFFFFu) return no_plane;
        return static_cast<std::uint8_t>((state >> 10) & 0x3u);
    }

    struct linear_tile_allocator {
        std::uint16_t next_tile = 0;
        std::uint16_t end_tile = 1024;

        [[nodiscard]]
        constexpr std::uint16_t allocate(std::uint16_t count = 1) noexcept {
            if (count == 0 || next_tile + count > end_tile) return no_tile;
            const auto base = next_tile;
            next_tile = static_cast<std::uint16_t>(next_tile + count);
            return base;
        }

        [[nodiscard]]
        constexpr std::uint16_t current() const noexcept {
            return next_tile;
        }

        constexpr void reset(std::uint16_t start = 0) noexcept {
            next_tile = start;
        }
    };

} // namespace gba::text
