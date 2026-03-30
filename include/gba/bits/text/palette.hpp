/// @file bits/text/palette.hpp
/// @brief Bitplane text palette helper API.
#pragma once

#include <gba/bits/text/types.hpp>
#include <gba/peripherals>

#include <bit>

namespace gba::text {

    /// @brief Fill palette RAM for a given plane using the bitplane config and theme.
    ///
    /// For each possible nibble value in the theme range, decodes this plane's role
    /// and sets the corresponding palette entry to the correct color.
    inline void set_palette_for_plane(const bitplane_config& config, unsigned char plane,
                                      const bitplane_theme& theme) noexcept {
        const auto bank = config.bank_for_plane(plane);
        if (bank == 255u) return; // plane not used

        auto* pal = gba::memory_map(gba::mem_pal_bg);
        const auto num_entries = required_entries(config.profile);

        for (unsigned short local_idx = 0; local_idx < num_entries; ++local_idx) {
            const auto nibble = static_cast<unsigned char>(config.start_index + local_idx);
            const auto role = config.decode_role(nibble, plane);

            gba::color color{};
            switch (role) {
                case static_cast<unsigned char>(bitplane_role::background): color = theme.background; break;
                case static_cast<unsigned char>(bitplane_role::foreground): color = theme.foreground; break;
                case static_cast<unsigned char>(bitplane_role::shadow): color = theme.shadow; break;
                default: color = theme.background; break;
            }

            const auto global_idx = config.global_index(plane, local_idx);
            if (global_idx != 0xFFFFu) {
                pal[global_idx] = static_cast<short>(std::bit_cast<unsigned short>(color));
            }
        }
    }

    /// @brief Fill all plane palettes using the bitplane config and theme.
    inline void set_theme(const bitplane_config& config, const bitplane_theme& theme) noexcept {
        for (unsigned char plane = 0; plane < profile_plane_count(config.profile); ++plane) {
            set_palette_for_plane(config, plane, theme);
        }
    }

} // namespace gba::text
