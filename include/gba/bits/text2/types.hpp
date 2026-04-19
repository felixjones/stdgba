/// @file bits/text2/types.hpp
/// @brief text2 bitplane and stream types.

#pragma once

#include <gba/color>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::text2 {
    enum class bitplane_profile : unsigned char {
        two_plane_binary,
        two_plane_three_color,
        three_plane_binary,
        one_plane_full_color,
    };

    enum class bitplane_role : unsigned char {
        background = 0,
        foreground = 1,
        shadow = 2,
    };

    struct bitplane_theme {
        color background{};
        color foreground{};
        color shadow{};
    };

    struct stream_metrics {
        unsigned short letter_spacing_px = 0;
        unsigned short line_spacing_px   = 0;
        unsigned short tab_width_px      = 32;
        unsigned short wrap_width_px     = 0xFFFFu;
    };

    [[nodiscard]]
    constexpr unsigned char profile_plane_count(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary:
            case bitplane_profile::two_plane_three_color:
                return 2;
            case bitplane_profile::three_plane_binary:
                return 3;
            case bitplane_profile::one_plane_full_color:
                return 1;
        }
        return 0;
    }

    [[nodiscard]]
    constexpr unsigned char profile_role_count(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary:
            case bitplane_profile::three_plane_binary:
                return 2;
            case bitplane_profile::two_plane_three_color:
                return 3;
            case bitplane_profile::one_plane_full_color:
                return 16;
        }
        return 0;
    }

    [[nodiscard]]
    constexpr unsigned char plane_stride(bitplane_profile profile, unsigned char plane) noexcept {
        const auto role_count = profile_role_count(profile);
        if (plane == 0) return 1;
        if (plane == 1) return role_count;
        if (plane == 2) return static_cast<unsigned char>(role_count * role_count);
        return 0;
    }

    struct bitplane_config {
        bitplane_profile profile = bitplane_profile::two_plane_three_color;
        unsigned char palbank_0 = 255u;
        unsigned char palbank_1 = 255u;
        unsigned char palbank_2 = 255u;
        unsigned char start_index = 1;

        [[nodiscard]]
        constexpr std::array<unsigned char, 3> banks() const noexcept {
            return {palbank_0, palbank_1, palbank_2};
        }

        [[nodiscard]]
        constexpr unsigned char plane_count() const noexcept {
            return profile_plane_count(profile);
        }

        [[nodiscard]]
        constexpr unsigned char role_count() const noexcept {
            return profile_role_count(profile);
        }

        /// @brief Palette bank for a given plane index (255 = unset).
        [[nodiscard]]
        constexpr unsigned char bank_for_plane(unsigned char plane) const noexcept {
            if (plane == 0) return palbank_0;
            if (plane == 1) return palbank_1;
            if (plane == 2) return palbank_2;
            return 255u;
        }

        /// @brief Nibble value for a pixel in the fully-background state.
        [[nodiscard]]
        constexpr unsigned char background_nibble() const noexcept {
            return start_index;
        }

        /// @brief Update the contribution of one plane inside an existing nibble.
        ///
        /// Preserves all other planes' roles stored in @p old_nibble using the
        /// mixed-radix encoding (start_index + sum(role_i * stride_i)).
        ///
        /// @param old_nibble  Current nibble value from VRAM/cache.
        /// @param plane_idx   Plane (0, 1 or 2) whose role is being changed.
        /// @param new_role    New role to write (e.g. foreground = 1, background = 0).
        /// @return New nibble value.
        [[nodiscard]]
        constexpr unsigned char update_role(unsigned char old_nibble,
                                             unsigned char plane_idx,
                                             unsigned char new_role) const noexcept {
            if (old_nibble < start_index) return old_nibble;
            const auto rc = role_count();
            const auto stride = plane_stride(profile, plane_idx);
            const auto adjusted = static_cast<unsigned char>(old_nibble - start_index);
            const auto old_role = static_cast<unsigned char>((adjusted / stride) % rc);
            const auto delta    = static_cast<int>(new_role) - static_cast<int>(old_role);
            const auto updated  = static_cast<unsigned char>(
                static_cast<int>(adjusted) + delta * static_cast<int>(stride));
            return static_cast<unsigned char>(start_index + updated);
        }
    };
} // namespace gba::text2
