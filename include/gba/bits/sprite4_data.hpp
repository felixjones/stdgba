/// @file bits/sprite4_data.hpp
/// @brief Shared 4bpp sprite payload and OAM helpers.
#pragma once

#include <gba/video>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::bits {

    /// @brief OAM shape code from pixel dimensions.
    consteval int oam_shape_for(unsigned int w, unsigned int h) {
        if (w == h) return 0;
        if (w > h) return 1;
        return 2;
    }

    /// @brief OAM size code from pixel dimensions.
    consteval int oam_size_for(unsigned int w, unsigned int h) {
        // Square
        if (w == 8 && h == 8) return 0;
        if (w == 16 && h == 16) return 1;
        if (w == 32 && h == 32) return 2;
        if (w == 64 && h == 64) return 3;
        // Wide
        if (w == 16 && h == 8) return 0;
        if (w == 32 && h == 8) return 1;
        if (w == 32 && h == 16) return 2;
        if (w == 64 && h == 32) return 3;
        // Tall
        if (w == 8 && h == 16) return 0;
        if (w == 8 && h == 32) return 1;
        if (w == 16 && h == 32) return 2;
        if (w == 32 && h == 64) return 3;
        return 0;
    }

    /// @brief Check whether pixel dimensions match a valid GBA sprite size.
    consteval bool is_valid_sprite_size(unsigned int w, unsigned int h) {
        constexpr unsigned int valid[][2] = {
            {8, 8}, {16, 16}, {32, 32}, {64, 64},
            {16, 8}, {32, 8}, {32, 16}, {64, 32},
            {8, 16}, {8, 32}, {16, 32}, {32, 64}
        };
        for (const auto& v : valid)
            if (w == v[0] && h == v[1]) return true;
        return false;
    }

    /// @brief Shared 4bpp sprite tile payload used by shapes and embed APIs.
    template<unsigned int Width, unsigned int Height, std::size_t TileCount = (Width / 8u) * (Height / 8u)>
    struct sprite4_data {
        static_assert(Width > 0 && Height > 0, "Sprite dimensions must be positive");
        static_assert(Width % 8 == 0 && Height % 8 == 0, "Sprite dimensions must be multiples of 8");

        static constexpr unsigned int width = Width;
        static constexpr unsigned int height = Height;
        static constexpr std::size_t tile_count = TileCount;

        std::array<gba::tile4bpp, TileCount> tiles{};

        [[nodiscard]]
        static constexpr gba::object obj(unsigned short tile_idx = 0) {
            static_assert(is_valid_sprite_size(Width, Height),
                "Image dimensions do not match a valid GBA sprite size");
            return {
                .depth = depth_4bpp,
                .shape = static_cast<gba::shape>(oam_shape_for(Width, Height)),
                .size = static_cast<unsigned short>(oam_size_for(Width, Height)),
                .tile_index = tile_idx
            };
        }

        [[nodiscard]]
        static constexpr gba::object_affine obj_aff(unsigned short tile_idx = 0) {
            static_assert(is_valid_sprite_size(Width, Height),
                "Image dimensions do not match a valid GBA sprite size");
            return {
                .depth = depth_4bpp,
                .shape = static_cast<gba::shape>(oam_shape_for(Width, Height)),
                .size = static_cast<unsigned short>(oam_size_for(Width, Height)),
                .tile_index = tile_idx
            };
        }

        /// @brief Pointer to tile data as packed bytes (4bpp tile order).
        [[nodiscard]] constexpr const std::uint8_t* data() const {
            return reinterpret_cast<const std::uint8_t*>(tiles.data());
        }

        /// @brief Number of bytes occupied by the stored tiles.
        [[nodiscard]] static constexpr std::size_t size() {
            return TileCount * sizeof(gba::tile4bpp);
        }
    };

} // namespace gba::bits
