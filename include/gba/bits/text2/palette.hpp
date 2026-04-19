/// @file bits/text2/palette.hpp
/// @brief Bitplane text2 palette helper API.

#pragma once

#include <gba/bits/text2/types.hpp>
#include <gba/peripherals>

#include <bit>

namespace gba::text2 {

    [[nodiscard]]
    constexpr unsigned short required_entries(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary:      return 4;
            case bitplane_profile::two_plane_three_color: return 9;
            case bitplane_profile::three_plane_binary:    return 8;
            case bitplane_profile::one_plane_full_color:  return 16;
        }
        return 0;
    }

    [[nodiscard]]
    constexpr unsigned short global_index(const bitplane_config& config, unsigned char plane,
                                          unsigned short local_idx) noexcept {
        const auto bank = config.bank_for_plane(plane);
        if (bank == 255u) return 0xFFFFu;
        return static_cast<unsigned short>(bank * 16u + config.start_index + local_idx);
    }

    [[nodiscard]]
    constexpr unsigned char decode_role(const bitplane_config& config, unsigned char nibble,
                                        unsigned char plane) noexcept {
        if (nibble < config.start_index) return 0;
        const auto adjusted = static_cast<unsigned char>(nibble - config.start_index);
        const auto stride = plane_stride(config.profile, plane);
        const auto role_count = profile_role_count(config.profile);
        return static_cast<unsigned char>((adjusted / stride) % role_count);
    }

    /// @brief Fill palette RAM for a given plane using the bitplane config and theme.
    inline void set_palette_for_plane(const bitplane_config& config, unsigned char plane,
                                      const bitplane_theme& theme) noexcept {
        const auto bank = config.bank_for_plane(plane);
        if (bank == 255u) return;

        auto* pal = gba::memory_map(gba::mem_pal_bg);
        const auto num_entries = required_entries(config.profile);

        for (unsigned short local_idx = 0; local_idx < num_entries; ++local_idx) {
            const auto nibble = static_cast<unsigned char>(config.start_index + local_idx);
            const auto role = decode_role(config, nibble, plane);

            if (config.profile == bitplane_profile::one_plane_full_color &&
                role > static_cast<unsigned char>(bitplane_role::shadow)) {
                continue;
            }

            gba::color color{};
            switch (role) {
                case static_cast<unsigned char>(bitplane_role::background):
                    color = theme.background;
                    break;
                case static_cast<unsigned char>(bitplane_role::foreground):
                    color = theme.foreground;
                    break;
                case static_cast<unsigned char>(bitplane_role::shadow):
                    color = theme.shadow;
                    break;
                default:
                    color = theme.background;
                    break;
            }

            const auto idx = global_index(config, plane, local_idx);
            if (idx != 0xFFFFu) {
                pal[idx] = static_cast<short>(std::bit_cast<unsigned short>(color));
            }
        }
    }

    /// @brief Fill all plane palettes using the bitplane config and theme.
    inline void set_theme(const bitplane_config& config, const bitplane_theme& theme) noexcept {
        for (unsigned char plane = 0; plane < profile_plane_count(config.profile); ++plane) {
            set_palette_for_plane(config, plane, theme);
        }
    }

} // namespace gba::text2
