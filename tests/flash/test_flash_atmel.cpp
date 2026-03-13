/// @file test_flash_atmel.cpp
/// @brief Tests for Layer 1 Atmel Flash compiled command sequences.
///
/// Exercises the consteval command compiler for Atmel Flash chips.
/// mgba emulates a standard (non-Atmel) chip, so hardware execution
/// tests only run when an Atmel chip is detected. Compile-time
/// validation and API surface tests always run.

#include <gba/save>
#include <gba/testing>

#include <array>
#include <cstddef>

namespace flash = gba::flash;
namespace af = gba::flash::atmel;

// Global test data

static std::array<std::byte, flash::page_size_atmel> g_write_page;
static std::array<std::byte, flash::page_size_atmel> g_read_page;
static std::byte g_marker;
static std::byte g_p0_val, g_p1_val;
static std::byte g_b0_val, g_b1_val;

// Callback functions

void write_page_data(af::page_span buf) {
    for (std::size_t i = 0; i < buf.size(); ++i) buf[i] = g_write_page[i];
}
void read_page_data(af::const_page_span buf) {
    for (std::size_t i = 0; i < buf.size(); ++i) g_read_page[i] = buf[i];
}
void write_marker(af::page_span buf) {
    buf[0] = g_marker;
}
void read_marker(af::const_page_span buf) {
    g_marker = buf[0];
}
void write_p0(af::page_span buf) {
    buf[0] = std::byte{0xAA};
}
void write_p1(af::page_span buf) {
    buf[0] = std::byte{0x55};
}
void read_p0(af::const_page_span buf) {
    g_p0_val = buf[0];
}
void read_p1(af::const_page_span buf) {
    g_p1_val = buf[0];
}
void write_aa(af::page_span buf) {
    buf[0] = std::byte{0xAA};
}
void write_55(af::page_span buf) {
    buf[0] = std::byte{0x55};
}
void read_b0(af::const_page_span buf) {
    g_b0_val = buf[0];
}
void read_b1(af::const_page_span buf) {
    g_b1_val = buf[0];
}

int main() {
    const auto chip = flash::detect();
    gba::test.is_true(chip.chip_size != flash::size::detect);

    // Compile-time validation -- always runs regardless of chip type

    // compile() accepts write_page, read_page, switch_bank
    {
        constexpr auto cmds = af::compile(af::write_page(0, write_marker), af::read_page(0, read_marker));
        gba::test.eq(cmds.count, 2u);
    }

    // compile() with switch_bank
    {
        constexpr auto cmds = af::compile(af::switch_bank(0), af::write_page(100, write_marker), af::switch_bank(1),
                                          af::read_page(200, read_marker));
        gba::test.eq(cmds.count, 4u);
    }

    // Page index range: 0 to pages_per_bank-1 (0-511)
    {
        constexpr auto cmds = af::compile(af::write_page(0, write_marker), af::write_page(511, write_marker));
        gba::test.eq(cmds.count, 2u);
    }

    // Bank index range: 0 or 1
    {
        constexpr auto cmds = af::compile(af::switch_bank(0), af::switch_bank(1));
        gba::test.eq(cmds.count, 2u);
    }

    // Empty compile
    {
        constexpr auto cmds = af::compile();
        gba::test.eq(cmds.count, 0u);
    }

    // No erase-before-write validation needed for Atmel
    // (page writes implicitly erase)
    {
        constexpr auto cmds = af::compile(af::write_page(0, write_marker), af::write_page(0, write_p0),
                                          af::write_page(0, write_p1));
        gba::test.eq(cmds.count, 3u);
    }

    // Compiled struct stores correct command kinds
    {
        constexpr auto cmds = af::compile(af::write_page(5, write_marker), af::read_page(10, read_marker),
                                          af::switch_bank(1));

        static_assert(cmds.commands[0].kind == af::cmd::write_page_k);
        static_assert(cmds.commands[1].kind == af::cmd::read_page_k);
        static_assert(cmds.commands[2].kind == af::cmd::switch_bank_k);

        static_assert(cmds.commands[0].index == 5);
        static_assert(cmds.commands[1].index == 10);
        static_assert(cmds.commands[2].index == 1);

        gba::test.is_true(true);
    }

    // Hardware tests -- only run on Atmel chips

    if (chip.mfr == flash::manufacturer::atmel) {
        // Basic write + read cycle
        {
            for (std::size_t i = 0; i < g_write_page.size(); ++i) g_write_page[i] = static_cast<std::byte>(i ^ 0x5A);
            g_read_page = {};

            constexpr auto cmds = af::compile(af::write_page(0, write_page_data), af::read_page(0, read_page_data));

            const auto err = cmds.execute();
            gba::test.eq(static_cast<int>(err), static_cast<int>(flash::error::success));

            for (std::size_t i = 0; i < g_write_page.size(); ++i) gba::test.eq(g_read_page[i], g_write_page[i]);
        }

        // Multiple pages in one sequence
        {
            constexpr auto cmds = af::compile(af::write_page(0, write_p0), af::write_page(1, write_p1),
                                              af::read_page(0, read_p0), af::read_page(1, read_p1));

            gba::test.eq(static_cast<int>(cmds.execute()), static_cast<int>(flash::error::success));
            gba::test.eq(g_p0_val, std::byte{0xAA});
            gba::test.eq(g_p1_val, std::byte{0x55});
        }

        // operator() as alias for execute()
        {
            g_marker = std::byte{0xEF};

            constexpr auto cmds = af::compile(af::write_page(2, write_marker), af::read_page(2, read_marker));

            gba::test.eq(static_cast<int>(cmds()), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0xEF});
        }

        // Bank switching (128KB chips only)
        if (chip.chip_size == flash::size::flash_128k) {
            constexpr auto write_cmds = af::compile(af::switch_bank(0), af::write_page(511, write_aa),
                                                    af::switch_bank(1), af::write_page(511, write_55));

            auto err = write_cmds.execute();
            gba::test.eq(static_cast<int>(err), static_cast<int>(flash::error::success));

            constexpr auto read_cmds = af::compile(af::switch_bank(0), af::read_page(511, read_b0), af::switch_bank(1),
                                                   af::read_page(511, read_b1));

            err = read_cmds.execute();
            gba::test.eq(static_cast<int>(err), static_cast<int>(flash::error::success));

            gba::test.eq(g_b0_val, std::byte{0xAA});
            gba::test.eq(g_b1_val, std::byte{0x55});

            flash::bits::switch_bank(0);
            flash::bits::g_state.current_bank = 0;
        }

        // Read-only sequence
        {
            // Page 0 still has the data from the multi-page test
            g_marker = std::byte{0};

            constexpr auto cmds = af::compile(af::read_page(0, read_marker));

            gba::test.eq(static_cast<int>(cmds()), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0xAA});
        }
    }

    // operator() on non-Atmel -- test it compiles and invokes execute()
    // (uses standard chip, but we can at least verify the call works
    //  on the compiled struct even if the hardware write would differ)

    if (chip.mfr != flash::manufacturer::atmel) {
        // Just verify operator() compiles and returns -- read from page 0
        // which is safe regardless of chip type (read_bytes is universal)
        g_marker = std::byte{0};

        constexpr auto cmds = af::compile(af::read_page(0, read_marker));

        // operator() should work as alias
        const auto err = cmds();
        gba::test.eq(static_cast<int>(err), static_cast<int>(flash::error::success));
    }

    // Placeholder arg compile-time validation -- always runs

    // Single arg: write + read with arg(0) as page
    {
        constexpr auto cmds = af::compile(af::write_page(af::arg(0), write_marker),
                                          af::read_page(af::arg(0), read_marker));
        static_assert(cmds.num_args == 1);
        gba::test.eq(cmds.count, 2u);
    }

    // Two args: bank + page
    {
        constexpr auto cmds = af::compile(af::switch_bank(af::arg(0)), af::write_page(af::arg(1), write_marker));
        static_assert(cmds.num_args == 2);
        gba::test.eq(cmds.count, 2u);
    }

    // Mixed static and arg commands
    {
        constexpr auto cmds = af::compile(af::switch_bank(0), af::write_page(af::arg(0), write_marker),
                                          af::read_page(42, read_marker));
        static_assert(cmds.num_args == 1);
        gba::test.eq(cmds.count, 3u);
    }

    // No args -- num_args should be 0
    {
        constexpr auto cmds = af::compile(af::write_page(0, write_marker));
        static_assert(cmds.num_args == 0);
    }

    // Arg stores correct arg_index in cmd
    {
        constexpr auto cmds = af::compile(af::switch_bank(af::arg(0)), af::write_page(af::arg(1), write_marker),
                                          af::read_page(af::arg(1), read_marker));
        static_assert(cmds.commands[0].arg_index == 0);
        static_assert(cmds.commands[1].arg_index == 1);
        static_assert(cmds.commands[2].arg_index == 1);
        // Static commands have arg_index == -1
        constexpr auto static_cmds = af::compile(af::write_page(5, write_marker));
        static_assert(static_cmds.commands[0].arg_index == -1);
        gba::test.is_true(true);
    }

    // Placeholder arg hardware tests -- only on Atmel chips

    if (chip.mfr == flash::manufacturer::atmel) {
        // Write + read with runtime page via arg(0)
        {
            g_marker = std::byte{0xCD};

            constexpr auto cmds = af::compile(af::write_page(af::arg(0), write_marker),
                                              af::read_page(af::arg(0), read_marker));

            gba::test.eq(static_cast<int>(cmds.execute(4)), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0xCD});

            // Verify persistence via static read
            g_marker = std::byte{0};
            constexpr auto verify = af::compile(af::read_page(4, read_marker));
            verify();
            gba::test.eq(g_marker, std::byte{0xCD});
        }

        // Two args: bank + page (128KB chips only)
        if (chip.chip_size == flash::size::flash_128k) {
            constexpr auto save = af::compile(af::switch_bank(af::arg(0)), af::write_page(af::arg(1), write_marker));

            g_marker = std::byte{0xAA};
            gba::test.eq(static_cast<int>(save(0, 100)), static_cast<int>(flash::error::success));

            g_marker = std::byte{0x55};
            gba::test.eq(static_cast<int>(save(1, 100)), static_cast<int>(flash::error::success));

            constexpr auto load = af::compile(af::switch_bank(af::arg(0)), af::read_page(af::arg(1), read_marker));

            g_marker = std::byte{0};
            gba::test.eq(static_cast<int>(load(0, 100)), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0xAA});

            g_marker = std::byte{0};
            gba::test.eq(static_cast<int>(load(1, 100)), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0x55});

            flash::bits::switch_bank(0);
            flash::bits::g_state.current_bank = 0;
        }

        // operator() with args
        {
            g_marker = std::byte{0x77};

            constexpr auto cmds = af::compile(af::write_page(af::arg(0), write_marker),
                                              af::read_page(af::arg(0), read_marker));

            gba::test.eq(static_cast<int>(cmds(10)), static_cast<int>(flash::error::success));
            gba::test.eq(g_marker, std::byte{0x77});
        }
    }
    return gba::test.finish();
}
