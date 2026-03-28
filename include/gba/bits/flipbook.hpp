/// @file bits/flipbook.hpp
/// @brief Compile-time animation frame sequence and helpers.
#pragma once

#include <cstddef>

namespace gba::embed {

    namespace bits {

        /// @brief Compile-time string literal NTTP for flipbook sequence definitions.
        ///
        /// Supports single-character frame indices:
        /// - `'0'..'9'` -> frame 0..9
        /// - `'a'..'z'` / `'A'..'Z'` -> frame 10..35
        template<std::size_t N>
        struct ct_string {
            char data[N]{};
            static constexpr std::size_t length = N - 1;

            consteval ct_string(const char (&str)[N]) {
                for (std::size_t i = 0; i < N; ++i) data[i] = str[i];
            }
        };

    } // namespace bits

    /// @brief Compile-time animation frame sequence.
    ///
    /// Stores an ordered list of frame indices. Wraps on access so
    /// `frame(step)` and `tile_offset(step, tpf)` never go out of bounds.
    ///
    /// Example:
    /// @code{.cpp}
    /// static constexpr auto walk = sheet.forward<0, 4>();
    /// // walk.frame(0) == 0, walk.frame(3) == 3, walk.frame(4) == 0 (wraps)
    ///
    /// renderable.tile_index = base_tile + walk.tile_offset(counter, sheet.tiles_per_frame);
    /// @endcode
    template<std::size_t N>
    struct flipbook {
        static_assert(N > 0, "Flipbook must have at least one frame");

        static constexpr std::size_t length = N;
        unsigned int frames[N]{};

        /// @brief Get the frame index for a given animation step (wraps).
        [[nodiscard]] constexpr unsigned int frame(std::size_t step) const noexcept { return frames[step % N]; }

        /// @brief Get the tile offset for a given animation step (wraps).
        /// @param step            Current animation step.
        /// @param tiles_per_frame Number of tiles per frame (from the sheet).
        [[nodiscard]] constexpr unsigned int tile_offset(std::size_t step,
                                                         unsigned int tiles_per_frame) const noexcept {
            return frames[step % N] * tiles_per_frame;
        }
    };

} // namespace gba::embed
