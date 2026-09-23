/// @file bits/save/crc16.hpp
/// @brief CRC16-CCITT used to detect corrupt/torn save records.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace gba::bits::backup {

    /// @brief Initial CRC16-CCITT accumulator value.
    inline constexpr std::uint16_t crc16_initial = 0xFFFF;

    /// @brief Fold one byte into a running CRC16-CCITT (poly 0x1021) accumulator.
    [[nodiscard]] constexpr std::uint16_t crc16_update(std::uint16_t crc, const std::byte byte) noexcept {
        crc ^= static_cast<std::uint16_t>(std::to_integer<unsigned char>(byte)) << 8u;
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc << 1u) ^ ((crc & 0x8000u) != 0 ? 0x1021 : 0));
        }
        return crc;
    }

    /// @brief CRC16-CCITT over `bytes`, continuing from `crc` (default: the initial accumulator).
    [[nodiscard]] constexpr std::uint16_t crc16(const std::span<const std::byte> bytes,
                                                std::uint16_t crc = crc16_initial) noexcept {
        for (const auto b : bytes) {
            crc = crc16_update(crc, b);
        }
        return crc;
    }

} // namespace gba::bits::backup
