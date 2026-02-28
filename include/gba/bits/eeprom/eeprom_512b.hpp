/// @file bits/eeprom/eeprom_512b.hpp
/// @brief 512-byte (4Kbit) EEPROM chip API.
///
/// 64 blocks of 8 bytes each. Uses 6-bit addressing.
///
/// @code{.cpp}
/// #include <gba/save>
///
/// namespace ee = gba::eeprom::eeprom_512b;
///
/// // Raw block access
/// ee::block data = {std::byte{1}, std::byte{2}, ...};
/// ee::write_block(0, data);
/// auto read = ee::read_block(0);
///
/// // Stream access
/// ee::ostream out;
/// out.write(&data, 1);
///
/// ee::istream in;
/// ee::block buf;
/// in.read(&buf, 1);
/// @endcode
///
/// @par tonclib equivalent
/// EEPROM_ReadData(offset, data) / EEPROM_WriteData(offset, data)
#pragma once

#include <gba/bits/eeprom/eeprom_base.hpp>

namespace gba::eeprom::eeprom_512b {

    /// Number of 64-bit blocks on chip.
    inline constexpr int block_count = 64;

    /// Total capacity in bytes.
    inline constexpr std::size_t capacity = 512;

    /// Address width in bits.
    inline constexpr int addr_bits = 6;

    /// Read one block by address (0-63).
    inline block read_block(int address) noexcept {
        return bits::read_block<addr_bits>(address);
    }

    /// Write one block by address (0-63).
    inline void write_block(int address, const block& data) noexcept {
        bits::write_block<addr_bits>(address, data);
    }

    /// Sequential block reader (0-63).
    using istream = bits::basic_istream<addr_bits, block_count>;

    /// Sequential block writer (0-63).
    using ostream = bits::basic_ostream<addr_bits, block_count>;

    /// Bidirectional block stream (0-63).
    using iostream = bits::basic_iostream<addr_bits, block_count>;

} // namespace gba::eeprom::eeprom_512b
