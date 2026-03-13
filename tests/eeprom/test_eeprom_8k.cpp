/// @file test_eeprom_8k.cpp
/// @brief Tests for 8KB EEPROM chip API.

#include <gba/save>
#include <gba/testing>

#include <cstddef>

namespace ee = gba::eeprom::eeprom_8k;
using gba::eeprom::block;

// mgba uses this string to detect EEPROM save type and size.
// The 8KB variant requires a ROM >= 16MB so the emulator uses 14-bit addressing.
[[gnu::used, gnu::section(".rodata")]]
static const char eeprom_marker[] = "EEPROM_V126";

// Force ROM size to 16MB+ so mGBA selects 8KB EEPROM (14-bit addressing).
// Without this, mGBA auto-detects 512B EEPROM (6-bit addressing).
[[gnu::used, gnu::section(".rompad")]]
static const char rom_pad[16 * 1024 * 1024] = {};

int main() {
    // Compile-time checks
    static_assert(ee::block_count == 1024);
    static_assert(ee::capacity == 8192);
    static_assert(ee::addr_bits == 14);
    static_assert(sizeof(block) == 8);

    // Read block 0 (fresh EEPROM should be 0xFF)
    {
        auto fresh = ee::read_block(0);
        for (int i = 0; i < 8; ++i) gba::test.eq(fresh[i], std::byte{0xFF});
    }

    // Raw write + read
    {
        block data = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF},
                      std::byte{0xCA}, std::byte{0xFE}, std::byte{0xBA}, std::byte{0xBE}};
        ee::write_block(0, data);

        auto read = ee::read_block(0);
        for (int i = 0; i < 8; ++i) gba::test.eq(read[i], data[i]);
    }

    // Write to different addresses, read back
    {
        block a = {std::byte{0x11}};
        block b = {std::byte{0x22}};
        ee::write_block(1, a);
        ee::write_block(2, b);

        auto ra = ee::read_block(1);
        auto rb = ee::read_block(2);
        gba::test.eq(ra[0], std::byte{0x11});
        gba::test.eq(rb[0], std::byte{0x22});
    }

    // High address (address 1023 - last block)
    {
        block data = {std::byte{0xFF}};
        ee::write_block(1023, data);
        auto read = ee::read_block(1023);
        gba::test.eq(read[0], std::byte{0xFF});
    }

    // ostream write + istream read
    {
        block w1 = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
                    std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08}};
        block w2 = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
                    std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80}};

        ee::ostream out;
        gba::test.eq(out.tellp(), 0u);
        out.write(&w1, 1);
        gba::test.eq(out.tellp(), 1u);
        out.write(&w2, 1);
        gba::test.eq(out.tellp(), 2u);

        ee::istream in;
        gba::test.eq(in.tellg(), 0u);
        block r1, r2;
        in.read(&r1, 1);
        gba::test.eq(in.tellg(), 1u);
        in.read(&r2, 1);
        gba::test.eq(in.tellg(), 2u);

        for (int i = 0; i < 8; ++i) {
            gba::test.eq(r1[i], w1[i]);
            gba::test.eq(r2[i], w2[i]);
        }
    }

    // seekg / seekp
    {
        block data = {std::byte{0xAB}};
        ee::ostream out;
        out.seekp(100);
        gba::test.eq(out.tellp(), 100u);
        out.write(&data, 1);
        gba::test.eq(out.tellp(), 101u);

        ee::istream in;
        in.seekg(100);
        gba::test.eq(in.tellg(), 100u);
        block read;
        in.read(&read, 1);
        gba::test.eq(in.tellg(), 101u);
        gba::test.eq(read[0], std::byte{0xAB});
    }

    // iostream bidirectional
    {
        block w = {std::byte{0x77}};
        ee::iostream io;
        gba::test.eq(io.tellp(), 0u);
        gba::test.eq(io.tellg(), 0u);

        io.seekp(50);
        io.write(&w, 1);
        gba::test.eq(io.tellp(), 51u);
        gba::test.eq(io.tellg(), 0u);

        io.seekg(50);
        block r;
        io.read(&r, 1);
        gba::test.eq(io.tellg(), 51u);
        gba::test.eq(r[0], std::byte{0x77});
    }

    // Write multiple blocks in one call
    {
        block blocks[3] = {{std::byte{0xA0}}, {std::byte{0xB0}}, {std::byte{0xC0}}};

        ee::ostream out;
        out.seekp(200);
        out.write(blocks, 3);
        gba::test.eq(out.tellp(), 203u);

        block read[3];
        ee::istream in;
        in.seekg(200);
        in.read(read, 3);
        gba::test.eq(in.tellg(), 203u);

        gba::test.eq(read[0][0], std::byte{0xA0});
        gba::test.eq(read[1][0], std::byte{0xB0});
        gba::test.eq(read[2][0], std::byte{0xC0});
    }

    // Chaining
    {
        block a = {std::byte{0x33}};
        block b = {std::byte{0x44}};
        ee::ostream out;
        out.seekp(300).write(&a, 1).write(&b, 1);
        gba::test.eq(out.tellp(), 302u);

        block ra, rb;
        ee::istream in;
        in.seekg(300).read(&ra, 1).read(&rb, 1);
        gba::test.eq(ra[0], std::byte{0x33});
        gba::test.eq(rb[0], std::byte{0x44});
    }

    // Full 8-byte block round trip
    {
        block data;
        for (int i = 0; i < 8; ++i) data[i] = static_cast<std::byte>(i * 0x11);

        ee::write_block(500, data);
        auto read = ee::read_block(500);

        for (int i = 0; i < 8; ++i) gba::test.eq(read[i], data[i]);
    }
    return gba::test.finish();
}
