/// @file bits/eeprom/eeprom_base.hpp
/// @brief EEPROM DMA3 transfer primitives and stream templates.
///
/// GBA EEPROM is accessed via DMA3 serial bit-banging to 0x0DFFFF00.
/// Each bit is transferred as bit 0 of a 16-bit halfword.
///
/// Unlike Flash, EEPROM does not share the ROM bus -- code can execute
/// from ROM during transfers. DMA3 halts the CPU for the transfer
/// duration.
///
/// @warning Interrupts that use DMA3 must be disabled during EEPROM
///          access, as DMA3 is used exclusively for the serial protocol.
#pragma once

#include <gba/dma>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>

namespace gba::eeprom {

    /// One 64-bit EEPROM block (8 bytes). The fundamental read/write unit.
    using block = std::array<std::byte, 8>;

    /// Size of one block in bytes.
    inline constexpr std::size_t block_size = 8;

    namespace bits {

        /// EEPROM serial bus endpoint.
        inline constexpr auto reg_eeprom_bus = registral<unsigned short>{0x0DFFFF00};

        /// Send halfwords to EEPROM via DMA3 (src increment, dst fixed).
        inline void dma3_send(const std::uint16_t* src, std::uint32_t count) noexcept {
            reg_dma[3] = {
                .source = src,
                .destination = memory_map(reg_eeprom_bus),
                .units = static_cast<unsigned short>(count),
                .control = {.dest_op = dest_op_fixed, .dma_type = dma_type::half, .enable = true}
            };
        }

        /// Receive halfwords from EEPROM via DMA3 (src fixed, dst increment).
        inline void dma3_recv(std::uint16_t* dst, std::uint32_t count) noexcept {
            reg_dma[3] = {
                .source = memory_map(reg_eeprom_bus),
                .destination = dst,
                .units = static_cast<unsigned short>(count),
                .control = {.src_op = src_op_fixed, .dma_type = dma_type::half, .enable = true}
            };
        }

        /// Poll EEPROM until a write operation completes.
        inline void poll_ready() noexcept {
            while ((static_cast<unsigned short>(reg_eeprom_bus) & 1) == 0) {}
        }

        /// Read one 64-bit block from EEPROM.
        /// @tparam AddrBits Address width (6 for 512B, 14 for 8KB).
        template<int AddrBits>
        [[gnu::noinline]]
        block read_block(int address) noexcept {
            constexpr int cmd_bits = 2 + AddrBits + 1; // +1 for end bit
            std::uint16_t cmd[cmd_bits];
            int idx = 0;

            // Read command: "11"
            cmd[idx++] = 1;
            cmd[idx++] = 1;

            // Address (MSB first)
            for (int i = AddrBits - 1; i >= 0; --i) cmd[idx++] = static_cast<std::uint16_t>((address >> i) & 1);

            // End bit
            cmd[idx++] = 0;

            dma3_send(cmd, cmd_bits);

            // Receive 4 padding + 64 data = 68 bits
            std::uint16_t resp[68];
            dma3_recv(resp, 68);

            block result{};
            for (int b = 0; b < 8; ++b) {
                unsigned val = 0;
                for (int bit = 7; bit >= 0; --bit) val |= (resp[4 + b * 8 + (7 - bit)] & 1u) << bit;
                result[b] = static_cast<std::byte>(val);
            }
            return result;
        }

        /// Write one 64-bit block to EEPROM.
        /// @tparam AddrBits Address width (6 for 512B, 14 for 8KB).
        template<int AddrBits>
        [[gnu::noinline]]
        void write_block(int address, const block& data) noexcept {
            constexpr int cmd_bits = 2 + AddrBits + 64 + 1;
            std::uint16_t cmd[cmd_bits];
            int idx = 0;

            // Write command: "10"
            cmd[idx++] = 1;
            cmd[idx++] = 0;

            // Address (MSB first)
            for (int i = AddrBits - 1; i >= 0; --i) cmd[idx++] = static_cast<std::uint16_t>((address >> i) & 1);

            // Data (64 bits, MSB first -- byte[0] bit 7 sent first)
            for (int b = 0; b < 8; ++b)
                for (int bit = 7; bit >= 0; --bit)
                    cmd[idx++] = static_cast<std::uint16_t>((static_cast<unsigned>(data[b]) >> bit) & 1);

            // Stop bit
            cmd[idx++] = 0;

            dma3_send(cmd, cmd_bits);
            poll_ready();
        }

        /// Sequential block reader.
        /// @tparam AddrBits Address width.
        /// @tparam BlockCount Total blocks on chip.
        template<int AddrBits, int BlockCount>
        struct basic_istream {
            /// Read blocks from current position.
            basic_istream& read(block* data, std::streamsize count) noexcept {
                for (std::streamsize i = 0; i < count && m_pos < BlockCount; ++i, ++m_pos)
                    data[i] = read_block<AddrBits>(m_pos);
                return *this;
            }

            /// Current read position (block index).
            [[nodiscard]] std::size_t tellg() const noexcept { return m_pos; }

            /// Seek read position.
            basic_istream& seekg(std::size_t pos) noexcept {
                m_pos = static_cast<int>(pos);
                return *this;
            }

        private:
            int m_pos{};
        };

        /// Sequential block writer.
        /// @tparam AddrBits Address width.
        /// @tparam BlockCount Total blocks on chip.
        template<int AddrBits, int BlockCount>
        struct basic_ostream {
            /// Write blocks at current position.
            basic_ostream& write(const block* data, std::streamsize count) noexcept {
                for (std::streamsize i = 0; i < count && m_pos < BlockCount; ++i, ++m_pos)
                    write_block<AddrBits>(m_pos, data[i]);
                return *this;
            }

            /// Current write position (block index).
            [[nodiscard]] std::size_t tellp() const noexcept { return m_pos; }

            /// Seek write position.
            basic_ostream& seekp(std::size_t pos) noexcept {
                m_pos = static_cast<int>(pos);
                return *this;
            }

        private:
            int m_pos{};
        };

        /// Bidirectional block stream.
        /// @tparam AddrBits Address width.
        /// @tparam BlockCount Total blocks on chip.
        template<int AddrBits, int BlockCount>
        struct basic_iostream {
            /// Read blocks from current get position.
            basic_iostream& read(block* data, std::streamsize count) noexcept {
                for (std::streamsize i = 0; i < count && m_gpos < BlockCount; ++i, ++m_gpos)
                    data[i] = read_block<AddrBits>(m_gpos);
                return *this;
            }

            /// Write blocks at current put position.
            basic_iostream& write(const block* data, std::streamsize count) noexcept {
                for (std::streamsize i = 0; i < count && m_ppos < BlockCount; ++i, ++m_ppos)
                    write_block<AddrBits>(m_ppos, data[i]);
                return *this;
            }

            /// Current read position (block index).
            [[nodiscard]] std::size_t tellg() const noexcept { return m_gpos; }

            /// Current write position (block index).
            [[nodiscard]] std::size_t tellp() const noexcept { return m_ppos; }

            /// Seek read position.
            basic_iostream& seekg(std::size_t pos) noexcept {
                m_gpos = static_cast<int>(pos);
                return *this;
            }

            /// Seek write position.
            basic_iostream& seekp(std::size_t pos) noexcept {
                m_ppos = static_cast<int>(pos);
                return *this;
            }

        private:
            int m_gpos{};
            int m_ppos{};
        };

    } // namespace bits
} // namespace gba::eeprom
