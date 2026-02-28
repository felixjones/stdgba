/// @file bits/eeprom/eeprom_8k.hpp
/// @brief 8KB (64Kbit) EEPROM chip API.
///
/// 1024 blocks of 8 bytes each. Uses 14-bit addressing (lower 10 bits
/// are the block address, upper 4 bits should be zero).
///
/// @code{.cpp}
/// #include <gba/save>
///
/// namespace ee = gba::eeprom::eeprom_8k;
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

namespace gba::eeprom::eeprom_8k {

    /// Number of 64-bit blocks on chip.
    inline constexpr int block_count = 1024;

    /// Total capacity in bytes.
    inline constexpr std::size_t capacity = 8192;

    /// Address width in bits.
    inline constexpr int addr_bits = 14;

    /// Read one block by address (0-1023).
    inline block read_block(int address) noexcept {
        return bits::read_block<addr_bits>(address);
    }

    /// Write one block by address (0-1023).
    inline void write_block(int address, const block& data) noexcept {
        bits::write_block<addr_bits>(address, data);
    }

    /// Sequential block reader (0-1023).
    using istream = bits::basic_istream<addr_bits, block_count>;

    /// Sequential block writer (0-1023).
    using ostream = bits::basic_ostream<addr_bits, block_count>;

    /// Bidirectional block stream (0-1023).
    using iostream = bits::basic_iostream<addr_bits, block_count>;

} // namespace gba::eeprom::eeprom_8k
