/**
 * @file test_flash_standard.cpp
 * @brief Tests for Layer 1 standard Flash compiled command sequences.
 *
 * Exercises the consteval command compiler and runtime execution
 * against mgba's Flash emulation.
 */

#include <gba/save>

#include <mgba_test.hpp>

#include <array>
#include <cstddef>

namespace flash = gba::flash;
namespace sf = gba::flash::standard;

// Global test data — callback functions reference these

static std::array<std::byte, 64> g_write_data;
static std::array<std::byte, 64> g_read_data;
static std::byte g_marker;
static std::byte g_s0_val, g_s1_val;
static std::byte g_b0_val, g_b1_val;
static std::array<std::byte, flash::sector_size> g_full_write;
static std::array<std::byte, flash::sector_size> g_full_read;

// Callback functions (function pointers, valid as consteval arguments)

void write_test_data(sf::sector_span buf) {
    for (std::size_t i = 0; i < g_write_data.size(); ++i) buf[i] = g_write_data[i];
}
void read_test_data(sf::const_sector_span buf) {
    for (std::size_t i = 0; i < g_read_data.size(); ++i) g_read_data[i] = buf[i];
}
void write_marker(sf::sector_span buf) { buf[0] = g_marker; }
void read_marker(sf::const_sector_span buf) { g_marker = buf[0]; }
void write_s0(sf::sector_span buf) { buf[0] = std::byte{0xAA}; }
void write_s1(sf::sector_span buf) { buf[0] = std::byte{0x55}; }
void read_s0(sf::const_sector_span buf) { g_s0_val = buf[0]; }
void read_s1(sf::const_sector_span buf) { g_s1_val = buf[0]; }
void write_aa(sf::sector_span buf) { buf[0] = std::byte{0xAA}; }
void write_55(sf::sector_span buf) { buf[0] = std::byte{0x55}; }
void read_b0(sf::const_sector_span buf) { g_b0_val = buf[0]; }
void read_b1(sf::const_sector_span buf) { g_b1_val = buf[0]; }
void write_full(sf::sector_span buf) {
    for (std::size_t i = 0; i < buf.size(); ++i) buf[i] = g_full_write[i];
}
void read_full(sf::const_sector_span buf) {
    for (std::size_t i = 0; i < buf.size(); ++i) g_full_read[i] = buf[i];
}

int main() {
    const auto chip = flash::detect();
    ASSERT_TRUE(chip.chip_size != flash::size::detect);

    // Skip if Atmel (this test is for standard chips)
    if (chip.mfr == flash::manufacturer::atmel) return 0;

    // Basic erase + write + read cycle
    {
        for (std::size_t i = 0; i < g_write_data.size(); ++i)
            g_write_data[i] = static_cast<std::byte>(i ^ 0xA5);
        g_read_data = {};

        constexpr auto cmds = sf::compile(
            sf::erase_sector(0),
            sf::write_sector(0, write_test_data),
            sf::read_sector(0, read_test_data)
        );

        const auto err = cmds.execute();
        ASSERT_EQ(static_cast<int>(err), static_cast<int>(flash::error::success));

        for (std::size_t i = 0; i < g_write_data.size(); ++i)
            ASSERT_EQ(g_read_data[i], g_write_data[i]);
    }

    // Multiple sectors in one sequence
    {
        constexpr auto cmds = sf::compile(
            sf::erase_sector(0),
            sf::write_sector(0, write_s0),
            sf::erase_sector(1),
            sf::write_sector(1, write_s1),
            sf::read_sector(0, read_s0),
            sf::read_sector(1, read_s1)
        );

        ASSERT_EQ(static_cast<int>(cmds.execute()), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_s0_val, std::byte{0xAA});
        ASSERT_EQ(g_s1_val, std::byte{0x55});
    }

    // erase_chip enables writes to any sector
    {
        g_marker = std::byte{0xBE};

        constexpr auto cmds = sf::compile(
            sf::erase_chip(),
            sf::write_sector(3, write_marker),
            sf::read_sector(3, read_marker)
        );

        ASSERT_EQ(static_cast<int>(cmds.execute()), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0xBE});
    }

    // Read-only sequence (no erase needed)
    {
        g_marker = std::byte{0};

        constexpr auto cmds = sf::compile(
            sf::read_sector(3, read_marker)
        );

        ASSERT_EQ(static_cast<int>(cmds.execute()), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0xBE});
    }

    // Bank switching (128KB chips only)
    if (chip.chip_size == flash::size::flash_128k) {
        constexpr auto write_cmds = sf::compile(
            sf::switch_bank(0),
            sf::erase_sector(15),
            sf::write_sector(15, write_aa),
            sf::switch_bank(1),
            sf::erase_sector(15),
            sf::write_sector(15, write_55)
        );

        auto err = write_cmds.execute();
        ASSERT_EQ(static_cast<int>(err), static_cast<int>(flash::error::success));

        constexpr auto read_cmds = sf::compile(
            sf::switch_bank(0),
            sf::read_sector(15, read_b0),
            sf::switch_bank(1),
            sf::read_sector(15, read_b1)
        );

        err = read_cmds.execute();
        ASSERT_EQ(static_cast<int>(err), static_cast<int>(flash::error::success));

        ASSERT_EQ(g_b0_val, std::byte{0xAA});
        ASSERT_EQ(g_b1_val, std::byte{0x55});

        flash::detail::switch_bank(0);
        flash::detail::g_state.current_bank = 0;
    }

    // Compile-time validation via detail::validate()
    {
        using sf::detail::validate;

        // Valid: erase then write
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(0), sf::write_sector(0, write_marker)};
            static_assert(validate(seq, 2));
        }
        // Valid: erase_chip then write any sector
        {
            constexpr sf::cmd seq[] = {sf::erase_chip(), sf::write_sector(5, write_marker), sf::write_sector(10, write_marker)};
            static_assert(validate(seq, 3));
        }
        // Valid: read without erase
        {
            constexpr sf::cmd seq[] = {sf::read_sector(0, read_marker)};
            static_assert(validate(seq, 1));
        }
        // Valid: erase, write, re-erase, re-write same sector
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(0), sf::write_sector(0, write_marker), sf::erase_sector(0), sf::write_sector(0, write_marker)};
            static_assert(validate(seq, 4));
        }
        // Invalid: write without erase
        {
            constexpr sf::cmd seq[] = {sf::write_sector(0, write_marker)};
            static_assert(!validate(seq, 1));
        }
        // Invalid: erase sector 0, write sector 1
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(0), sf::write_sector(1, write_marker)};
            static_assert(!validate(seq, 2));
        }
        // Invalid: erase on bank 0, switch to bank 1, write
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(0), sf::switch_bank(1), sf::write_sector(0, write_marker)};
            static_assert(!validate(seq, 3));
        }
        // Valid: switch bank, erase, write
        {
            constexpr sf::cmd seq[] = {sf::switch_bank(1), sf::erase_sector(0), sf::write_sector(0, write_marker)};
            static_assert(validate(seq, 3));
        }

        ASSERT_TRUE(true);
    }

    // Full sector write + verify (all 4096 bytes)
    {
        for (std::size_t i = 0; i < g_full_write.size(); ++i)
            g_full_write[i] = static_cast<std::byte>((i * 7 + 13) & 0xFF);

        constexpr auto cmds = sf::compile(
            sf::erase_sector(2),
            sf::write_sector(2, write_full),
            sf::read_sector(2, read_full)
        );

        ASSERT_EQ(static_cast<int>(cmds.execute()), static_cast<int>(flash::error::success));

        for (std::size_t i = 0; i < flash::sector_size; ++i)
            ASSERT_EQ(g_full_read[i], g_full_write[i]);
    }

    // Runtime placeholder — erase + write + read with arg(0) as sector
    {
        g_marker = std::byte{0xCD};

        constexpr auto cmds = sf::compile(
            sf::erase_sector(sf::arg(0)),
            sf::write_sector(sf::arg(0), write_marker),
            sf::read_sector(sf::arg(0), read_marker)
        );

        // num_args should be 1
        static_assert(cmds.num_args == 1);

        // Execute with sector 4
        ASSERT_EQ(static_cast<int>(cmds.execute(4)), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0xCD});

        // Verify sector 4 persists by reading again with a different invocation
        g_marker = std::byte{0};
        constexpr auto read_cmd = sf::compile(sf::read_sector(4, read_marker));
        read_cmd.execute();
        ASSERT_EQ(g_marker, std::byte{0xCD});
    }

    // Runtime placeholder — two args: bank + sector
    if (chip.chip_size == flash::size::flash_128k) {
        constexpr auto save = sf::compile(
            sf::switch_bank(sf::arg(0)),
            sf::erase_sector(sf::arg(1)),
            sf::write_sector(sf::arg(1), write_marker)
        );

        static_assert(save.num_args == 2);

        // Write 0xAA to bank 0 sector 5
        g_marker = std::byte{0xAA};
        ASSERT_EQ(static_cast<int>(save.execute(0, 5)), static_cast<int>(flash::error::success));

        // Write 0x55 to bank 1 sector 5
        g_marker = std::byte{0x55};
        ASSERT_EQ(static_cast<int>(save.execute(1, 5)), static_cast<int>(flash::error::success));

        // Read back both
        constexpr auto load = sf::compile(
            sf::switch_bank(sf::arg(0)),
            sf::read_sector(sf::arg(1), read_marker)
        );

        static_assert(load.num_args == 2);

        g_marker = std::byte{0};
        ASSERT_EQ(static_cast<int>(load.execute(0, 5)), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0xAA});

        g_marker = std::byte{0};
        ASSERT_EQ(static_cast<int>(load.execute(1, 5)), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0x55});

        // Restore bank 0
        flash::detail::switch_bank(0);
        flash::detail::g_state.current_bank = 0;
    }

    // Runtime placeholder — mixed static and arg commands
    {
        // Erase chip (static), write to runtime sector
        g_marker = std::byte{0x77};

        constexpr auto cmds = sf::compile(
            sf::erase_chip(),
            sf::write_sector(sf::arg(0), write_marker),
            sf::read_sector(sf::arg(0), read_marker)
        );

        static_assert(cmds.num_args == 1);

        ASSERT_EQ(static_cast<int>(cmds.execute(7)), static_cast<int>(flash::error::success));
        ASSERT_EQ(g_marker, std::byte{0x77});
    }

    // Compile-time validation with placeholders
    {
        using sf::detail::validate;

        // Valid: erase arg(0) then write arg(0)
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(sf::arg(0)), sf::write_sector(sf::arg(0), write_marker)};
            static_assert(validate(seq, 2));
        }
        // Valid: erase_chip then write arg(0)
        {
            constexpr sf::cmd seq[] = {sf::erase_chip(), sf::write_sector(sf::arg(0), write_marker)};
            static_assert(validate(seq, 2));
        }
        // Invalid: erase arg(0) then write arg(1) (different placeholder)
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(sf::arg(0)), sf::write_sector(sf::arg(1), write_marker)};
            static_assert(!validate(seq, 2));
        }
        // Invalid: erase static sector 0 then write arg(0) (can't verify)
        {
            constexpr sf::cmd seq[] = {sf::erase_sector(0), sf::write_sector(sf::arg(0), write_marker)};
            static_assert(!validate(seq, 2));
        }
        // Valid: read with arg needs no erase
        {
            constexpr sf::cmd seq[] = {sf::read_sector(sf::arg(0), read_marker)};
            static_assert(validate(seq, 1));
        }
        // Valid: switch_bank arg + erase arg + write arg (same args)
        {
            constexpr sf::cmd seq[] = {sf::switch_bank(sf::arg(0)), sf::erase_sector(sf::arg(1)), sf::write_sector(sf::arg(1), write_marker)};
            static_assert(validate(seq, 3));
        }

        ASSERT_TRUE(true);
    }
}
