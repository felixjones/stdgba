/// @file test_save_record_eeprom.cpp
/// @brief Tests for gba::bits::backup::record over the 512B EEPROM backend, including
/// values whose encoded size straddles an 8-byte block boundary.

#include <gba/backup>
#include <gba/testing>

#include <cstddef>
#include <cstdint>
#include <vector>

// mgba uses this string to detect EEPROM save type and size.
[[gnu::used, gnu::section(".rodata")]]
static const char eeprom_marker[] = "EEPROM_V126";

namespace {

    struct player_save {
        std::uint32_t score;
        std::uint16_t level;
    };

    constexpr auto save_codec =
        gba::codec::make_tuple_codec(gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
                                      gba::codec::member<&player_save::level>(gba::codec::uint16_codec))
            .apply<player_save>();

    // Capacity 16 is exactly two 8-byte EEPROM blocks -- exercises eeprom_backend's
    // block-aligned placement path.
    using record_type = gba::bits::backup::record<save_codec, gba::bits::backup::eeprom_backend_512b, 16>;

    constexpr auto bytes_codec = gba::codec::vector_of(gba::codec::uint8_codec);
    using dynamic_record_type = gba::bits::backup::record<bytes_codec, gba::bits::backup::eeprom_backend_512b, 16>;

} // namespace

int main() {
    // Establish EEPROM bus width for emulator auto-detection before anything else.
    {
        auto fresh = gba::eeprom::eeprom_512b::read_block(0);
        for (int i = 0; i < 8; ++i) gba::test.eq(fresh[i], std::byte{0xFF});
    }

    gba::test("store then load round trips across block boundaries", [] {
        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 0xCAFEBABEu, .level = 42}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 0xCAFEBABEu);
        gba::test.expect.eq(loaded->level, static_cast<std::uint16_t>(42));
    });

    gba::test("a second record placed by table does not disturb the first", [] {
        gba::bits::backup::table<record_type, record_type> pair;

        gba::test.expect.is_true(pair.field<0>().store(player_save{.score = 1, .level = 1}));
        gba::test.expect.is_true(pair.field<1>().store(player_save{.score = 2, .level = 2}));

        gba::test.expect.eq(pair.field<0>().load()->score, 1u);
        gba::test.expect.eq(pair.field<1>().load()->score, 2u);

        // record_type's 16-byte wire_size is already block-aligned; table places the
        // second record immediately after.
        gba::test.expect.eq(pair.field<0>().offset(), std::size_t{0});
        gba::test.expect.eq(pair.field<1>().offset(), std::size_t{16});
    });

    gba::test("dynamic fields reserve a block run for the directory, then block-align each field",
              [] {
                  gba::bits::backup::table<dynamic_record_type, dynamic_record_type> saves;

                  const std::vector<std::uint8_t> a = {1, 2, 3};
                  const std::vector<std::uint8_t> b = {4, 5};
                  gba::test.expect.is_true(saves.emplace<0>(a));
                  gba::test.expect.is_true(saves.emplace<1>(b));

                  // Each of the two directory generations has 2 fields * 8 bytes/entry,
                  // plus a 2-byte generation and 2-byte CRC. Each 20-byte copy gets its
                  // own 8-byte write block, so the reservation is 48 bytes. a's encoded size
                  // (4-byte length prefix + 3 data bytes = 7 bytes) fits in one block (8
                  // bytes); b's (4 + 2 = 6 bytes) does too.
                  gba::test.expect.eq(saves.field<0>().offset(), std::size_t{48});
                  gba::test.expect.eq(saves.field<1>().offset(), std::size_t{56});

                  gba::test.expect.is_false(saves.is_fragmented());
                  gba::test.expect.eq(*saves.get<0>(), a);
                  gba::test.expect.eq(*saves.get<1>(), b);
              });

    return gba::test.finish();
}
