/// @file test_save_record_flash_atmel.cpp
/// @brief Tests for gba::bits::backup::record over the Atmel Flash backend.
///
/// mgba emulates a standard (non-Atmel) chip, so the hardware-backed
/// assertions here only run when an Atmel chip is actually detected;
/// see test_flash_atmel.cpp for the same convention.

#include <gba/backup>
#include <gba/testing>

#include <array>
#include <cstdint>

namespace {

    struct player_save {
        std::uint32_t score;
        std::uint16_t level;
    };

    constexpr auto save_codec =
        gba::codec::make_tuple_codec(gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
                                      gba::codec::member<&player_save::level>(gba::codec::uint16_codec))
            .apply<player_save>();

    using record_type = gba::bits::backup::record<save_codec, gba::bits::backup::flash_backend_atmel, 16>;

} // namespace

int main() {
    const auto chip = gba::flash::detect();
    gba::test.is_true(chip.chip_size != gba::flash::size::detect);

    if (chip.mfr != gba::flash::manufacturer::atmel) return gba::test.finish();

    gba::flash::bits::write_atmel_page(0, std::array<std::uint8_t, gba::flash::page_size_atmel>{}.data());

    gba::test("fresh page has no valid record", [] {
        record_type record;
        gba::test.expect.is_false(record.load().has_value());
    });

    gba::test("store then load round trips through a page write", [] {
        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 0xCAFEBABEu, .level = 42}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 0xCAFEBABEu);
        gba::test.expect.eq(loaded->level, static_cast<std::uint16_t>(42));
    });

    gba::test("a second store preserves the rest of the page", [] {
        std::array<std::byte, 1> sentinel = {std::byte{0x7A}};
        gba::bits::backup::flash_backend_atmel raw;
        raw.write(record_type::wire_size, sentinel);

        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 7, .level = 1}));
        gba::test.expect.is_true(record.store(player_save{.score = 99, .level = 3}));
        gba::test.expect.eq(record.load()->score, 99u);

        std::array<std::byte, 1> readback{};
        raw.read(record_type::wire_size, readback);
        gba::test.expect.eq(readback[0], std::byte{0x7A});
    });

    return gba::test.finish();
}
