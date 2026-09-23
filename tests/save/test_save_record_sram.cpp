/// @file test_save_record_sram.cpp
/// @brief Tests for gba::bits::backup::record over the SRAM backend, plus gba::backup.

#include <gba/backup>
#include <gba/testing>

#include <cstddef>
#include <cstdint>

// mgba uses this string to detect SRAM save type.
[[gnu::used, gnu::section(".rodata")]]
static const char sram_marker[] = "SRAM_V113";

namespace {

    struct player_save {
        std::uint32_t score;
        std::uint16_t level;
        bool hardcore;
    };

    constexpr auto save_codec = gba::codec::make_tuple_codec(
                                     gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
                                     gba::codec::member<&player_save::level>(gba::codec::uint16_codec),
                                     gba::codec::member<&player_save::hardcore>(gba::codec::bool_codec))
                                     .apply<player_save>();

    using record_type = gba::bits::backup::record<save_codec, gba::bits::backup::sram_backend, 32>;

} // namespace

int main() {
    gba::test("store then load round trips the value", [] {
        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 1234, .level = 7, .hardcore = true}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 1234u);
        gba::test.expect.eq(loaded->level, static_cast<std::uint16_t>(7));
        gba::test.expect.is_true(loaded->hardcore);
    });

    gba::test("store overwrites a previous value", [] {
        record_type record;
        gba::test.expect.is_true(record.store(player_save{.score = 1, .level = 1, .hardcore = false}));
        gba::test.expect.is_true(record.store(player_save{.score = 99, .level = 3, .hardcore = true}));

        const auto loaded = record.load();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 99u);
        gba::test.expect.eq(loaded->level, static_cast<std::uint16_t>(3));
    });

    gba::test("gba::backup slot holds a single-record table and round trips through it", [] {
        gba::backup.reset();

        // A freshly assigned table's directory hasn't been formatted yet, so every
        // field reports absent regardless of whatever bytes the backend happens to hold.
        auto& saves = gba::backup.emplace<gba::bits::backup::table<record_type>>();
        gba::test.expect.is_false(saves.get<player_save>().has_value());

        gba::test.expect.is_true(saves.emplace<player_save>(777u, 9, true));
        const auto loaded = saves.get<player_save>();
        gba::test.expect.is_true(loaded.has_value());
        gba::test.expect.eq(loaded->score, 777u);

        gba::test.expect.is_true(static_cast<bool>(gba::backup));
        saves.erase<player_save>();
        gba::test.expect.is_false(saves.get<player_save>().has_value());

        gba::backup.reset();
        gba::test.expect.is_false(static_cast<bool>(gba::backup));
    });

    return gba::test.finish();
}
