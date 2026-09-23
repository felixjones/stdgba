/// @file bits/save/sram_backend.hpp
/// @brief Save backend adapting byte-addressable SRAM to `gba::bits::backup::Backend`.
#pragma once

#include <gba/bits/save/backend.hpp>
#include <gba/save>

#include <cstddef>
#include <span>

namespace gba::bits::backup {

    /// @brief `Backend` over the 32KB SRAM chip at 0x0E000000.
    ///
    /// SRAM is directly byte-addressable, so this backend is a thin span
    /// copy with no buffering or read-modify-write required.
    struct sram_backend {
        /// @brief Total addressable bytes.
        static constexpr std::size_t capacity = 0x8000;

        /// @brief SRAM has no transfer/erase granularity; any byte can be written alone.
        static constexpr std::size_t granularity = 1;

        static void read(const std::size_t offset, const std::span<std::byte> out) noexcept {
            for (std::size_t i = 0; i < out.size(); ++i) {
                out[i] = mem_sram[offset + i];
            }
        }

        static void write(const std::size_t offset, const std::span<const std::byte> in) noexcept {
            for (std::size_t i = 0; i < in.size(); ++i) {
                mem_sram[offset + i] = in[i];
            }
        }
    };

    static_assert(Backend<sram_backend>);

} // namespace gba::bits::backup
