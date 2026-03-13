/// @file test_flash_ops.cpp
/// @brief Tests for Layer 0 Flash hardware operations.
///
/// Exercises the raw Flash primitives in gba::flash::bits against
/// the mgba Flash emulation. mgba emulates a 128KB Sanyo LE26FV10N1TS
/// by default (or 64KB depending on the save type string).

#include <gba/save>
#include <gba/testing>

#include <array>
#include <cstring>

namespace flash = gba::flash;
namespace ops = gba::flash::bits;

int main() {
    // detect() -- chip identification

    // Detect should populate global state
    {
        const auto info = flash::detect();

        // mgba should emulate a known Flash chip
        gba::test.is_true(info.chip_size != flash::size::detect);

        // Global state should match the returned info
        gba::test.eq(static_cast<int>(ops::g_state.info.chip_size), static_cast<int>(info.chip_size));
        gba::test.eq(static_cast<int>(ops::g_state.info.mfr), static_cast<int>(info.mfr));
        gba::test.eq(ops::g_state.info.device, info.device);

        // Bank should be initialized to 0
        gba::test.eq(ops::g_state.current_bank, 0);
    }

    // detect() with size hint should override auto-detection
    {
        const auto info = flash::detect(flash::size::flash_128k);
        gba::test.eq(static_cast<int>(info.chip_size), static_cast<int>(flash::size::flash_128k));
        gba::test.eq(static_cast<int>(ops::g_state.info.chip_size), static_cast<int>(flash::size::flash_128k));
    }

    // Re-detect without hint to restore actual chip info
    const auto chip = flash::detect();

    // read_bytes() -- basic Flash read

    // Reading from Flash should not crash and should return data
    {
        std::array<std::uint8_t, 16> buf{};
        ops::read_bytes(buf.data(), ops::flash_ptr(0), buf.size());
        // We can't assert specific values (Flash may contain anything),
        // but the call should succeed without hanging
        gba::test.is_true(true);
    }

    // erase_sector() + write_byte() + read_bytes() -- standard write cycle
    //
    // Skip if chip is Atmel (uses page writes, not byte writes)

    if (chip.mfr != flash::manufacturer::atmel) {
        // Erase sector 0
        {
            const int result = ops::erase_sector(0);
            gba::test.eq(result, 0);
        }

        // After erase, sector should read as a uniform value.
        // Real hardware erases to 0xFF; mgba erases to 0x00.
        // Determine the erased byte value by reading the first byte.
        std::uint8_t erased_byte = 0;
        {
            ops::read_bytes(&erased_byte, ops::flash_ptr(0), 1);
            gba::test.is_true(erased_byte == 0xFF || erased_byte == 0x00);
        }

        // Verify the full range is uniformly erased
        {
            std::array<std::uint8_t, 64> buf{};
            ops::read_bytes(buf.data(), ops::flash_ptr(0), buf.size());

            for (std::size_t i = 0; i < buf.size(); ++i) {
                gba::test.eq(buf[i], erased_byte);
            }
        }

        // Write test pattern: bytes 0..63 = i ^ 0xA5
        {
            for (std::size_t i = 0; i < 64; ++i) {
                const auto byte = static_cast<std::uint8_t>(i ^ 0xA5);
                const int result = ops::write_byte(static_cast<std::uint32_t>(i), byte);
                gba::test.eq(result, 0);
            }
        }

        // Read back and verify
        {
            std::array<std::uint8_t, 64> buf{};
            ops::read_bytes(buf.data(), ops::flash_ptr(0), buf.size());

            for (std::size_t i = 0; i < buf.size(); ++i) {
                const auto expected = static_cast<std::uint8_t>(i ^ 0xA5);
                gba::test.eq(buf[i], expected);
            }
        }

        // Write to a different offset within the same (already-erased) sector
        {
            constexpr std::uint32_t offset = 256;
            for (std::size_t i = 0; i < 16; ++i) {
                const auto byte = static_cast<std::uint8_t>(0xBB - i);
                gba::test.eq(ops::write_byte(offset + i, byte), 0);
            }

            std::array<std::uint8_t, 16> buf{};
            ops::read_bytes(buf.data(), ops::flash_ptr(offset), buf.size());

            for (std::size_t i = 0; i < buf.size(); ++i) {
                gba::test.eq(buf[i], static_cast<std::uint8_t>(0xBB - i));
            }
        }

        // Erase sector 1 (should not affect sector 0 data)
        {
            gba::test.eq(ops::erase_sector(flash::sector_size), 0);

            // Sector 1 should be erased
            std::array<std::uint8_t, 16> buf1{};
            ops::read_bytes(buf1.data(), ops::flash_ptr(flash::sector_size), buf1.size());
            for (std::size_t i = 0; i < buf1.size(); ++i) {
                gba::test.eq(buf1[i], erased_byte);
            }

            // Sector 0 data should still be intact
            std::array<std::uint8_t, 16> buf0{};
            ops::read_bytes(buf0.data(), ops::flash_ptr(0), buf0.size());
            for (std::size_t i = 0; i < buf0.size(); ++i) {
                gba::test.eq(buf0[i], static_cast<std::uint8_t>(i ^ 0xA5));
            }
        }
    }

    // write_atmel_page() -- Atmel 128-byte page write
    //
    // Only runs if chip is Atmel

    if (chip.mfr == flash::manufacturer::atmel) {
        // Prepare 128-byte test pattern
        alignas(4) std::array<std::uint8_t, flash::page_size_atmel> page{};
        for (std::size_t i = 0; i < page.size(); ++i) {
            page[i] = static_cast<std::uint8_t>(i ^ 0x5A);
        }

        // Write page 0
        {
            const int result = ops::write_atmel_page(0, page.data());
            gba::test.eq(result, 0);
        }

        // Read back and verify
        {
            std::array<std::uint8_t, flash::page_size_atmel> buf{};
            ops::read_bytes(buf.data(), ops::flash_ptr(0), buf.size());

            for (std::size_t i = 0; i < buf.size(); ++i) {
                gba::test.eq(buf[i], page[i]);
            }
        }
    }

    // switch_bank() -- bank switching (128KB chips only)

    if (chip.chip_size == flash::size::flash_128k) {
        // Write a marker to bank 0, sector 0
        if (chip.mfr != flash::manufacturer::atmel) {
            ops::erase_sector(0);
            ops::write_byte(0, 0xAA);
        } else {
            std::array<std::uint8_t, flash::page_size_atmel> page{};
            page[0] = 0xAA;
            for (std::size_t i = 1; i < page.size(); ++i) page[i] = 0xFF;
            ops::write_atmel_page(0, page.data());
        }

        // Switch to bank 1
        ops::switch_bank(1);
        ops::g_state.current_bank = 1;

        // Write a different marker to bank 1, sector 0
        if (chip.mfr != flash::manufacturer::atmel) {
            ops::erase_sector(0);
            ops::write_byte(0, 0x55);
        } else {
            std::array<std::uint8_t, flash::page_size_atmel> page{};
            page[0] = 0x55;
            for (std::size_t i = 1; i < page.size(); ++i) page[i] = 0xFF;
            ops::write_atmel_page(0, page.data());
        }

        // Read bank 1 -- should see 0x55
        {
            std::uint8_t val = 0;
            ops::read_bytes(&val, ops::flash_ptr(0), 1);
            gba::test.eq(val, 0x55);
        }

        // Switch back to bank 0
        ops::switch_bank(0);
        ops::g_state.current_bank = 0;

        // Read bank 0 -- should see 0xAA
        {
            std::uint8_t val = 0;
            ops::read_bytes(&val, ops::flash_ptr(0), 1);
            gba::test.eq(val, 0xAA);
        }
    }

    // erase_chip() -- full chip erase

    {
        // Determine erased byte value (0xFF on real hardware, 0x00 on mgba)
        // We need to erase first to discover it
        if (chip.mfr != flash::manufacturer::atmel) {
            ops::erase_sector(0);
            ops::write_byte(0, 0x42);

            std::uint8_t check = 0;
            ops::read_bytes(&check, ops::flash_ptr(0), 1);
            gba::test.eq(check, 0x42);
        }

        // Erase the entire chip
        gba::test.eq(ops::erase_chip(), 0);

        // Determine the erased value
        std::uint8_t erased = 0;
        ops::read_bytes(&erased, ops::flash_ptr(0), 1);
        gba::test.is_true(erased == 0xFF || erased == 0x00);

        // A byte deeper into the bank should also be erased
        {
            std::uint8_t val = 0x42;
            ops::read_bytes(&val, ops::flash_ptr(0x1000), 1);
            gba::test.eq(val, erased);
        }
    }

    // flash_ptr() / flash_cmd_ptr() -- address computation

    {
        // These are constexpr-friendly, but test them at runtime too
        const auto* ptr = ops::flash_ptr(0);
        gba::test.eq(reinterpret_cast<std::uintptr_t>(ptr), 0x0E000000u);

        const auto* ptr2 = ops::flash_ptr(0x1234);
        gba::test.eq(reinterpret_cast<std::uintptr_t>(ptr2), 0x0E001234u);

        auto* cmd = ops::flash_cmd_ptr(0x5555);
        gba::test.eq(reinterpret_cast<std::uintptr_t>(cmd), 0x0E005555u);
    }

    // Constants -- compile-time verification

    {
        static_assert(flash::sector_size == 4096);
        static_assert(flash::page_size_atmel == 128);
        static_assert(flash::bank_size == 0x10000);
        static_assert(flash::sectors_per_bank == 16);
        static_assert(flash::pages_per_bank == 512);
        static_assert(flash::sectors_per_bank * flash::sector_size == flash::bank_size);
        static_assert(flash::pages_per_bank * flash::page_size_atmel == flash::bank_size);
        gba::test.is_true(true); // static_asserts passed
    }
    return gba::test.finish();
}
