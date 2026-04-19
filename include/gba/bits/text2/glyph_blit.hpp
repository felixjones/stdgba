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

#include <array>
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

        static constexpr auto two_plane_binary_transition_lut = [] {
            std::array<std::array<std::array<std::uint8_t, 2>, 2>, 16> lut{};
            constexpr bitplane_config cfg{
                .profile = bitplane_profile::two_plane_binary,
                .start_index = 1,
            };
            for (unsigned old = 0; old < 16; ++old) {
                for (unsigned plane = 0; plane < 2; ++plane) {
                    for (unsigned role = 0; role < 2; ++role) {
                        lut[old][plane][role] = cfg.update_role(
                            static_cast<std::uint8_t>(old),
                            static_cast<std::uint8_t>(plane),
                            static_cast<std::uint8_t>(role));
                    }
                }
            }
            return lut;
        }();

        static constexpr auto two_plane_three_color_transition_lut = [] {
            std::array<std::array<std::array<std::uint8_t, 3>, 2>, 16> lut{};
            constexpr bitplane_config cfg{
                .profile = bitplane_profile::two_plane_three_color,
                .start_index = 1,
            };
            for (unsigned old = 0; old < 16; ++old) {
                for (unsigned plane = 0; plane < 2; ++plane) {
                    for (unsigned role = 0; role < 3; ++role) {
                        lut[old][plane][role] = cfg.update_role(
                            static_cast<std::uint8_t>(old),
                            static_cast<std::uint8_t>(plane),
                            static_cast<std::uint8_t>(role));
                    }
                }
            }
            return lut;
        }();

        static constexpr auto three_plane_binary_transition_lut = [] {
            std::array<std::array<std::array<std::uint8_t, 2>, 3>, 16> lut{};
            constexpr bitplane_config cfg{
                .profile = bitplane_profile::three_plane_binary,
                .start_index = 1,
            };
            for (unsigned old = 0; old < 16; ++old) {
                for (unsigned plane = 0; plane < 3; ++plane) {
                    for (unsigned role = 0; role < 2; ++role) {
                        lut[old][plane][role] = cfg.update_role(
                            static_cast<std::uint8_t>(old),
                            static_cast<std::uint8_t>(plane),
                            static_cast<std::uint8_t>(role));
                    }
                }
            }
            return lut;
        }();

        static constexpr auto base_row_mask_lut = [] {
            std::array<std::uint32_t, 256> lut{};
            for (unsigned bits = 0; bits < lut.size(); ++bits) {
                std::uint32_t mask = 0;
                for (unsigned b = 0; b < 8u; ++b) {
                    if (((bits >> b) & 1u) == 0u) continue;
                    mask |= 0xFu << (b * 4u);
                }
                lut[bits] = mask;
            }
            return lut;
        }();

        static constexpr auto base_row_value_lut = [] {
            std::array<std::uint32_t, 256> lut{};
            for (unsigned bits = 0; bits < lut.size(); ++bits) {
                std::uint32_t value = 0;
                for (unsigned b = 0; b < 8u; ++b) {
                    if (((bits >> b) & 1u) == 0u) continue;
                    value |= 0x1u << (b * 4u);
                }
                lut[bits] = value;
            }
            return lut;
        }();

        [[nodiscard]]
        static constexpr std::uint32_t nibble_mask(int lx) noexcept {
            return 0xFu << (static_cast<unsigned>(lx) * 4u);
        }

        [[nodiscard]]
        static constexpr std::uint32_t nibble_value(int lx, std::uint8_t nibble) noexcept {
            return static_cast<std::uint32_t>(nibble) << (static_cast<unsigned>(lx) * 4u);
        }

        template<std::size_t PlaneCount, std::size_t RoleCount>
        static bool apply_row_with_transition_lut(tile4bpp& words, int ly, std::uint8_t bitmap_byte,
                                                  int tile_lx, std::uint8_t plane_idx,
                                                  std::uint8_t effective_role,
                                                  const std::array<std::array<std::array<std::uint8_t, RoleCount>, PlaneCount>, 16>& lut) noexcept {
            if (plane_idx >= PlaneCount || effective_role >= RoleCount) return false;
            auto& row = words[static_cast<unsigned>(ly)];
            for (int b = 0; b < 8 && (tile_lx + b) < 8; ++b) {
                if (((bitmap_byte >> b) & 1u) == 0u) continue;
                const auto lx = tile_lx + b;
                const auto shift = static_cast<unsigned>(lx) * 4u;
                const auto old_nibble = static_cast<std::uint8_t>((row >> shift) & 0xFu);
                const auto new_nibble = lut[old_nibble][plane_idx][effective_role];
                row = (row & ~nibble_mask(lx)) | nibble_value(lx, new_nibble);
            }
            return true;
        }

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
            const auto max_role = static_cast<std::uint8_t>(cfg.role_count() - 1u);
            const auto effective_role = (role > max_role) ? max_role : role;
            auto& row = words[static_cast<unsigned>(ly)];
            if (cfg.profile == bitplane_profile::one_plane_full_color && plane_idx == 0u) {
                const auto new_nibble = static_cast<std::uint8_t>(cfg.start_index + effective_role);
                row = (row & ~nibble_mask(lx)) | nibble_value(lx, new_nibble);
                dirty = true;
                return;
            }
            const auto shift = static_cast<unsigned>(lx) * 4u;
            const auto old_nibble = static_cast<unsigned char>((row >> shift) & 0xFu);
            std::uint8_t new_nibble = cfg.update_role(old_nibble, plane_idx, effective_role);
            if (cfg.start_index == 1u) {
                if (cfg.profile == bitplane_profile::two_plane_binary &&
                    plane_idx < 2u && effective_role < 2u) {
                    new_nibble = two_plane_binary_transition_lut[old_nibble][plane_idx][effective_role];
                } else if (cfg.profile == bitplane_profile::two_plane_three_color &&
                           plane_idx < 2u && effective_role < 3u) {
                    new_nibble = two_plane_three_color_transition_lut[old_nibble][plane_idx][effective_role];
                } else if (cfg.profile == bitplane_profile::three_plane_binary &&
                           plane_idx < 3u && effective_role < 2u) {
                    new_nibble = three_plane_binary_transition_lut[old_nibble][plane_idx][effective_role];
                }
            }
            row = (row & ~nibble_mask(lx)) | nibble_value(lx, new_nibble);
            dirty = true;
        }

        /// @brief Apply one 1bpp row byte across 8 pixels starting at tile_lx.
        void apply_row(int ly, std::uint8_t bitmap_byte, int tile_lx,
                       std::uint8_t plane_idx, std::uint8_t fg_role,
                       const bitplane_config& cfg) noexcept {
            if (ly < 0 || ly >= 8) return;
            if (bitmap_byte == 0) return;
            const auto max_role = static_cast<std::uint8_t>(cfg.role_count() - 1u);
            const auto effective_role = (fg_role > max_role) ? max_role : fg_role;
            if (cfg.profile == bitplane_profile::one_plane_full_color && plane_idx == 0u) {
                auto& row = words[static_cast<unsigned>(ly)];
                const auto new_nibble = static_cast<std::uint8_t>(cfg.start_index + effective_role);
                const auto base_mask = base_row_mask_lut[bitmap_byte];
                const auto base_value = base_row_value_lut[bitmap_byte] * new_nibble;
                const auto shift = static_cast<unsigned>(tile_lx) * 4u;
                const auto mask = (tile_lx == 0) ? base_mask : (base_mask << shift);
                const auto value = (tile_lx == 0) ? base_value : (base_value << shift);
                row = (row & ~mask) | value;
                dirty = true;
                return;
            }
            if (cfg.start_index == 1u && plane_idx < 2u &&
                cfg.profile == bitplane_profile::two_plane_binary && effective_role < 2u) {
                apply_row_with_transition_lut(words, ly, bitmap_byte, tile_lx, plane_idx,
                                              effective_role, two_plane_binary_transition_lut);
                dirty = true;
                return;
            }
            if (cfg.start_index == 1u && plane_idx < 2u &&
                cfg.profile == bitplane_profile::two_plane_three_color && effective_role < 3u) {
                apply_row_with_transition_lut(words, ly, bitmap_byte, tile_lx, plane_idx,
                                              effective_role, two_plane_three_color_transition_lut);
                dirty = true;
                return;
            }
            if (cfg.start_index == 1u && plane_idx < 3u &&
                cfg.profile == bitplane_profile::three_plane_binary && effective_role < 2u) {
                apply_row_with_transition_lut(words, ly, bitmap_byte, tile_lx, plane_idx,
                                              effective_role, three_plane_binary_transition_lut);
                dirty = true;
                return;
            }
            for (int b = 0; b < 8 && (tile_lx + b) < 8; ++b) {
                if ((bitmap_byte >> b) & 1u) {
                    set_pixel(tile_lx + b, ly, plane_idx, effective_role, cfg);
                }
            }
        }
    };



} // namespace gba::text2
