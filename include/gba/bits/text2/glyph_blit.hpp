/// @file bits/text2/glyph_blit.hpp
/// @brief text2 IWRAM tile cache — plane-aware nibble accumulator.
///
/// A single IWRAM buffer holds one full VRAM tile's worth of 4bpp nibble data.
/// All pixel writes operate against this buffer via bitplane_config::update_role();
/// VRAM is touched only once on load and once on flush.  This replaces the legacy
/// per-pixel VRAM read-modify-write with a per-glyph VRAM read + VRAM write.

#pragma once

#include <gba/video>
#include <gba/bits/text2/tile_allocator.hpp>
#include <gba/bits/text2/types.hpp>

#include <cstdint>

namespace gba::text2 {

    /// @brief IWRAM-resident tile cache for one VRAM tile.
    ///
    /// Accumulates nibble updates for any number of planes on the same VRAM tile.
    /// Plane identity is supplied per-pixel via the plane_idx parameter so one cache
    /// can serve a tile shared by multiple planes (mixed-radix encoding is preserved).
    struct tile_plane_cache {
        tile4bpp words{};
        bool dirty = false;
        std::uint16_t vram_tile = no_tile;

        void init_to_background(std::uint16_t tile_idx, const bitplane_config& cfg) noexcept {
            const auto bg_nibble = cfg.background_nibble();
            const auto bg_row = static_cast<std::uint32_t>(0x11111111u) * bg_nibble;
            for (auto& row : words) row = bg_row;
            vram_tile = tile_idx;
            dirty = false;
        }

        void load_from_vram(std::uint16_t tile_idx) noexcept {
            auto* tiles = memory_map(gba::registral_cast<tile4bpp[2048]>(mem_tile_4bpp));
            words = tiles[tile_idx];
            vram_tile = tile_idx;
            dirty = false;
        }

        void flush() noexcept {
            if (!dirty || vram_tile == no_tile) return;
            auto* tiles = memory_map(gba::registral_cast<tile4bpp[2048]>(mem_tile_4bpp));
            tiles[vram_tile] = words;
            dirty = false;
        }

        void invalidate() noexcept {
            flush();
            vram_tile = no_tile;
            dirty = false;
        }

        /// @brief Update one pixel nibble for the given plane.
        ///
        /// Reads the existing nibble from the IWRAM words (which already encodes other
        /// planes' contributions), applies update_role() for this plane, and writes back.
        void set_pixel(int lx, int ly, std::uint8_t plane_idx, std::uint8_t role,
                       const bitplane_config& cfg) noexcept {
            if (lx < 0 || ly < 0 || lx >= 8 || ly >= 8) return;
            auto& row = words[static_cast<unsigned>(ly)];
            const auto shift = static_cast<unsigned>(lx) * 4u;
            const auto old_nibble = static_cast<unsigned char>((row >> shift) & 0xFu);
            const auto new_nibble = cfg.update_role(old_nibble, plane_idx, role);
            row = (row & ~(0xFu << shift)) | (static_cast<std::uint32_t>(new_nibble) << shift);
            dirty = true;
        }

        /// @brief Apply one 1bpp row byte across 8 pixels starting at tile_lx.
        void apply_row(int ly, std::uint8_t bitmap_byte, int tile_lx,
                       std::uint8_t plane_idx, std::uint8_t fg_role,
                       const bitplane_config& cfg) noexcept {
            if (ly < 0 || ly >= 8) return;
            for (int b = 0; b < 8 && (tile_lx + b) < 8; ++b) {
                if ((bitmap_byte >> b) & 1u) {
                    set_pixel(tile_lx + b, ly, plane_idx, fg_role, cfg);
                }
            }
        }
    };

} // namespace gba::text2
