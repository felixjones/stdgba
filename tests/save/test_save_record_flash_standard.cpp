/// @file test_save_record_flash_standard.cpp
/// @brief Tests for gba::bits::backup::record over the standard Flash backend, including a
/// partial-sector update that exercises the read-modify-erase-write path.

#include <gba/backup>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

    struct player_save {
        std::uint32_t score;
        std::uint16_t level;
    };

    constexpr auto save_codec =
        gba::codec::make_tuple_codec(gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
                                      gba::codec::member<&player_save::level>(gba::codec::uint16_codec))
            .apply<player_save>();

    using record_type = gba::bits::backup::record<save_codec, gba::bits::backup::flash_backend_standard_64k, 16>;

    constexpr auto bytes_codec = gba::codec::vector_of(gba::codec::uint8_codec);
    using dynamic_record_type = gba::bits::backup::record<bytes_codec, gba::bits::backup::flash_backend_standard_64k, 16>;

} // namespace

int main() {
    const auto chip = gba::flash::detect();
    gba::test.is_true(chip.chip_size != gba::flash::size::detect);

    // Atmel chips get their own backend/test; this file covers erase-then-program chips.
    if (chip.mfr == gba::flash::manufacturer::atmel) return gba::test.finish();

    gba::bits::backup::flash_backend_standard_64k backend;
    // Clean slate for every test below.
    gba::flash::bits::erase_sector(0);

    gba::test("store then load round trips through a sector erase", [] {
        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 0xCAFEBABEu, .level = 42}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 0xCAFEBABEu);
        gba::test.expect.eq(loaded->level, static_cast<std::uint16_t>(42));
    });

    gba::test("a second store preserves the rest of the sector across the erase", [] {
        // Poison the byte immediately after this record's wire footprint so a
        // backend that fails to preserve untouched sector bytes on erase is caught.
        std::array<std::byte, 1> sentinel = {std::byte{0x7A}};
        gba::bits::backup::flash_backend_standard_64k raw;
        raw.write(record_type::wire_size, sentinel);

        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 7, .level = 1}));
        gba::test.expect.is_true(record.store(player_save{.score = 99, .level = 3}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 99u);

        std::array<std::byte, 1> readback{};
        raw.read(record_type::wire_size, readback);
        gba::test.expect.eq(readback[0], std::byte{0x7A});
    });

    gba::test("a second record placed by table lands on the next sector", [] {
        gba::bits::backup::table<record_type, record_type> pair;

        gba::test.expect.is_true(pair.field<0>().store(player_save{.score = 1, .level = 1}));
        gba::test.expect.is_true(pair.field<1>().store(player_save{.score = 2, .level = 2}));

        gba::test.expect.eq(pair.field<0>().load()->score, 1u);
        gba::test.expect.eq(pair.field<1>().load()->score, 2u);

        gba::test.expect.eq(pair.field<0>().offset(), std::size_t{0});
        gba::test.expect.eq(pair.field<1>().offset(), gba::flash::sector_size);
    });

    gba::test("dynamic fields reserve a whole sector for the directory, then one sector each",
              [] {
                  gba::flash::bits::erase_sector(0);
                  gba::flash::bits::erase_sector(1);
                  gba::flash::bits::erase_sector(2);
                  gba::flash::bits::erase_sector(3);

                  gba::bits::backup::table<dynamic_record_type, dynamic_record_type> saves;

                  const std::vector<std::uint8_t> a = {1, 2, 3};
                  const std::vector<std::uint8_t> b = {4, 5};
                  gba::test.expect.is_true(saves.emplace<0>(a));
                  gba::test.expect.is_true(saves.emplace<1>(b));

                  // The two directory generations occupy sectors 0 and 1; the two
                  // dynamic fields land on the following sectors, one each.
                  gba::test.expect.eq(saves.field<0>().offset(), gba::flash::sector_size * 2);
                  gba::test.expect.eq(saves.field<1>().offset(), gba::flash::sector_size * 3);

                  gba::test.expect.is_false(saves.is_fragmented());
                  gba::test.expect.eq(*saves.get<0>(), a);
                  gba::test.expect.eq(*saves.get<1>(), b);
              });

    // Bank switching (128KB chips only)
    if (chip.chip_size == gba::flash::size::flash_128k) {
        gba::test("flash_backend_standard_128k reaches the second bank", [] {
            using wide_record_type = gba::bits::backup::record<save_codec, gba::bits::backup::flash_backend_standard_128k, 16>;

            wide_record_type record{gba::flash::bank_size + gba::flash::sector_size * 2};
            gba::test.expect.is_true(record.store(player_save{.score = 0x1234u, .level = 9}));

            const auto loaded = record.load();
            gba::test.expect.is_true(loaded.has_value());
            gba::test.expect.eq(loaded->score, 0x1234u);

            gba::flash::bits::switch_bank(0);
            gba::flash::bits::g_state.current_bank = 0;
        });
    }

    return gba::test.finish();
}
