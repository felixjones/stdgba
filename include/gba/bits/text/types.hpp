/// @file bits/text/types.hpp
/// @brief Shared text-stream and bitplane-palette types.
#pragma once

#include <gba/color>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::text {

    enum class break_policy : unsigned char {
        whitespace,
        whitespace_and_hyphen,
    };

    struct stream_metrics {
        unsigned short letter_spacing_px = 0;
        unsigned short tab_width_px = 16;
        break_policy break_chars = break_policy::whitespace;
    };

    enum class bitplane_profile : unsigned char {
        two_plane_binary,
        two_plane_three_color,
        three_plane_binary,
    };

    // Forward declaration for bitplane_config validation helpers.
    [[nodiscard]]
    constexpr unsigned short required_entries(bitplane_profile profile) noexcept;

    enum class bitplane_role : unsigned char {
        background = 0,
        foreground = 1,
        shadow = 2,
    };

    struct bitplane_theme {
        gba::color background{};
        gba::color foreground{};
        gba::color shadow{};
    };

    /// @brief Get the number of planes for a profile.
    [[nodiscard]]
    constexpr unsigned char profile_plane_count(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary:
            case bitplane_profile::two_plane_three_color: return 2;
            case bitplane_profile::three_plane_binary: return 3;
        }
        return 0;
    }

    /// @brief Get the number of color roles for a profile.
    [[nodiscard]]
    constexpr unsigned char profile_role_count(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary:
            case bitplane_profile::three_plane_binary: return 2;    // bg, fg only
            case bitplane_profile::two_plane_three_color: return 3; // bg, fg, shadow
        }
        return 0;
    }

    /// @brief Calculate stride for a given plane in the mixed-radix encoding.
    ///
    /// For example, with two planes and 3 roles:
    /// - plane 0 stride = 1
    /// - plane 1 stride = 1 * 3 = 3
    [[nodiscard]]
    constexpr unsigned char plane_stride(bitplane_profile profile, unsigned char plane) noexcept {
        const auto role_count = profile_role_count(profile);
        if (plane == 0) return 1;
        if (plane == 1) return role_count;
        if (plane == 2) return static_cast<unsigned char>(role_count * role_count);
        return 0;
    }

    /// @brief Per-plane palette-bank configuration for bitplane text rendering.
    ///
    /// Uses designated initializers for clarity. Specify which palette bank each
    /// bitplane uses and the start_index for palette entries.
    ///
    /// @code{.cpp}
    /// const auto config = gba::text::bitplane_config{
    ///     .profile = gba::text::bitplane_profile::two_plane_three_color,
    ///     .palbank_0 = 1,
    ///     .palbank_1 = 2,
    ///     .start_index = 1,
    /// };
    /// @endcode
    struct bitplane_config {
        bitplane_profile profile = bitplane_profile::two_plane_three_color;
        unsigned char palbank_0 = 255u;
        unsigned char palbank_1 = 255u;
        unsigned char palbank_2 = 255u;
        unsigned char start_index = 1;

        // Internal: convert to array format
        [[nodiscard]]
        constexpr std::array<unsigned char, 3> banks() const noexcept {
            return {palbank_0, palbank_1, palbank_2};
        }

        /// Count active planes (non-255 entries).
        [[nodiscard]]
        constexpr unsigned char plane_count() const noexcept {
            unsigned char count = 0;
            for (auto b : banks()) {
                if (b != 255u) ++count;
            }
            return count;
        }

        /// Get palette bank for a given plane. Returns 255 if plane is unallocated.
        [[nodiscard]]
        constexpr unsigned char bank_for_plane(unsigned char plane) const noexcept {
            if (plane >= 3) return 255u;
            const auto b = banks();
            return b[plane];
        }

        /// Get global palette index for a given plane and local color index.
        [[nodiscard]]
        constexpr unsigned short global_index(unsigned char plane, unsigned short local_idx) const noexcept {
            const auto bank = bank_for_plane(plane);
            if (bank == 255u) return 0xFFFFu; // invalid
            return static_cast<unsigned short>(bank * 16 + start_index + local_idx);
        }

        /// Encode plane roles into a nibble value (4-bit color index).
        [[nodiscard]]
        constexpr unsigned char encode_roles(const std::array<unsigned char, 3>& roles) const noexcept {
            auto result = start_index;
            for (unsigned char p = 0; p < profile_plane_count(profile); ++p) {
                result = static_cast<unsigned char>(result + roles[p] * plane_stride(profile, p));
            }
            return result;
        }

        /// Decode a nibble to extract a specific plane's role.
        [[nodiscard]]
        constexpr unsigned char decode_role(unsigned char nibble, unsigned char plane) const noexcept {
            const auto adjusted = static_cast<unsigned char>(nibble - start_index);
            const auto stride = plane_stride(profile, plane);
            const auto role_count = profile_role_count(profile);
            return static_cast<unsigned char>((adjusted / stride) % role_count);
        }

        /// @brief Update the role for a single plane in an existing nibble.
        ///
        /// Mixed-radix encoding means we can update one plane by adjusting that
        /// plane's digit in-place: old + (new - old) * stride.
        [[nodiscard]]
        constexpr unsigned char update_role(unsigned char nibble, unsigned char plane,
                                            unsigned char new_role) const noexcept {
            const auto role_count = profile_role_count(profile);
            const auto stride = plane_stride(profile, plane);
            const auto adjusted = static_cast<unsigned char>(nibble - start_index);
            const auto old_role = static_cast<unsigned char>((adjusted / stride) % role_count);

            const int delta = static_cast<int>(new_role) - static_cast<int>(old_role);
            const auto updated =
                static_cast<unsigned char>(static_cast<int>(adjusted) + delta * static_cast<int>(stride));
            return static_cast<unsigned char>(start_index + updated);
        }

        /// Get the background nibble (all planes set to background role).
        [[nodiscard]]
        constexpr unsigned char background_nibble() const noexcept {
            std::array<unsigned char, 3> roles{};
            roles.fill(0); // all background
            return encode_roles(roles);
        }
    };

    /// @brief Validate a bitplane config.
    ///
    /// Rules:
    /// - For 2-plane profiles, palbank_0 and palbank_1 must be set; palbank_2 must be unset.
    /// - For 3-plane profiles, palbank_0..2 must all be set.
    /// - All used palette banks must be in range [0, 15].
    /// - start_index + required_entries(profile) must fit within a single palette bank (<= 16).
    [[nodiscard]]
    constexpr bool valid_config(const bitplane_config& cfg) noexcept {
        const auto expectedPlanes = profile_plane_count(cfg.profile);

        for (unsigned char p = 0; p < expectedPlanes; ++p) {
            const auto bank = cfg.bank_for_plane(p);
            if (bank == 255u) return false;
            if (bank >= 16u) return false;
        }

        for (unsigned char p = expectedPlanes; p < 3; ++p) {
            if (cfg.bank_for_plane(p) != 255u) return false;
        }

        return cfg.start_index + required_entries(cfg.profile) <= 16u;
    }

    /// @brief Consteval validation helper.
    ///
    /// Use this when you want invalid configs to become compile errors.
    [[nodiscard]]
    consteval bitplane_config checked_bitplane_config(bitplane_config cfg) {
        if (!valid_config(cfg)) throw "invalid bitplane_config";
        return cfg;
    }

    [[nodiscard]]
    constexpr bool is_break_char(char c, break_policy policy) noexcept {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return true;
        return policy == break_policy::whitespace_and_hyphen && c == '-';
    }

    [[nodiscard]]
    constexpr unsigned short required_entries(bitplane_profile profile) noexcept {
        switch (profile) {
            case bitplane_profile::two_plane_binary: return 4;
            case bitplane_profile::two_plane_three_color: return 9;
            case bitplane_profile::three_plane_binary: return 8;
        }
        return 0;
    }

} // namespace gba::text
