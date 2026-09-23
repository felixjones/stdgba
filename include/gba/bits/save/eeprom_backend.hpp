/// @file bits/save/eeprom_backend.hpp
/// @brief Save backend adapting block-addressable EEPROM to `gba::bits::backup::Backend`.
#pragma once

#include <gba/bits/eeprom/eeprom_base.hpp>
#include <gba/bits/save/backend.hpp>

#include <algorithm>
#include <cstddef>
#include <span>

namespace gba::bits::backup {

    /// @brief `Backend` over block-addressable EEPROM. EEPROM only transfers whole 8-byte
    /// blocks, so any `read`/`write` whose offset or size is not block-aligned is serviced
    /// with a read-modify-write on the straddled block(s).
    ///
    /// @tparam AddrBits Address width (6 for 512B EEPROM, 14 for 8KB EEPROM).
    /// @tparam BlockCount Total 8-byte blocks on chip (64 or 1024).
    template<int AddrBits, int BlockCount>
    struct eeprom_backend {
        /// @brief Total addressable bytes.
        static constexpr std::size_t capacity = static_cast<std::size_t>(BlockCount) * eeprom::block_size;

        /// @brief EEPROM transfers whole 8-byte blocks; that's the natural placement unit.
        static constexpr std::size_t granularity = eeprom::block_size;

        static void read(const std::size_t offset, std::span<std::byte> out) noexcept {
            std::size_t pos = 0;
            while (pos < out.size()) {
                const std::size_t absolute = offset + pos;
                const auto blockIndex = static_cast<int>(absolute / eeprom::block_size);
                const std::size_t blockOffset = absolute % eeprom::block_size;
                const eeprom::block block = eeprom::bits::read_block<AddrBits>(blockIndex);

                const std::size_t chunk = std::min(out.size() - pos, eeprom::block_size - blockOffset);
                for (std::size_t i = 0; i < chunk; ++i) {
                    out[pos + i] = block[blockOffset + i];
                }
                pos += chunk;
            }
        }

        static void write(const std::size_t offset, const std::span<const std::byte> in) noexcept {
            std::size_t pos = 0;
            while (pos < in.size()) {
                const std::size_t absolute = offset + pos;
                const auto blockIndex = static_cast<int>(absolute / eeprom::block_size);
                const std::size_t blockOffset = absolute % eeprom::block_size;
                const std::size_t chunk = std::min(in.size() - pos, eeprom::block_size - blockOffset);

                eeprom::block block = (blockOffset != 0 || chunk != eeprom::block_size)
                                          ? eeprom::bits::read_block<AddrBits>(blockIndex)
                                          : eeprom::block{};
                for (std::size_t i = 0; i < chunk; ++i) {
                    block[blockOffset + i] = in[pos + i];
                }
                eeprom::bits::write_block<AddrBits>(blockIndex, block);
                pos += chunk;
            }
        }
    };

    /// @brief `Backend` over 512B (64-block) EEPROM.
    using eeprom_backend_512b = eeprom_backend<6, 64>;

    /// @brief `Backend` over 8KB (1024-block) EEPROM.
    using eeprom_backend_8k = eeprom_backend<14, 1024>;

    static_assert(Backend<eeprom_backend_512b>);
    static_assert(Backend<eeprom_backend_8k>);

} // namespace gba::bits::backup
