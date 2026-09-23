/// @file test_save_table.cpp
/// @brief Tests for gba::bits::backup::table's automatic, granularity-aligned field placement.

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
    };

    constexpr auto player_codec =
        gba::codec::make_tuple_codec(gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
                                      gba::codec::member<&player_save::level>(gba::codec::uint16_codec))
            .apply<player_save>();

    struct settings_save {
        bool sound;
    };

    constexpr auto settings_codec =
        gba::codec::make_tuple_codec(gba::codec::member<&settings_save::sound>(gba::codec::bool_codec))
            .apply<settings_save>();

    // player_codec/settings_codec are both FixedSizeCodec, so Capacity is omitted here
    // and deduced from each codec's encoded_size.
    using player_record = gba::bits::backup::record<player_codec, gba::bits::backup::sram_backend>;
    using settings_record = gba::bits::backup::record<settings_codec, gba::bits::backup::sram_backend>;

    static_assert(player_record::wire_size == sizeof(std::uint32_t) + sizeof(std::uint16_t));

} // namespace

int main() {
    gba::test("table places fields back to back on SRAM (granularity 1)", [] {
        gba::bits::backup::table<player_record, settings_record> saves;

        gba::test.expect.eq(saves.offset<0>(), std::size_t{0});
        gba::test.expect.eq(saves.offset<1>(), player_record::wire_size);
    });

    gba::test("each field in the table stores and loads independently", [] {
        gba::bits::backup::table<player_record, settings_record> saves;

        gba::test.expect.is_true(saves.emplace<0>(player_save{.score = 42, .level = 3}));
        gba::test.expect.is_true(saves.emplace<1>(settings_save{.sound = true}));

        const auto player = saves.get<0>();
        const auto settings = saves.get<1>();
        gba::test.expect.is_true(player.has_value());
        gba::test.expect.is_true(settings.has_value());
        gba::test.expect.eq(player->score, 42u);
        gba::test.expect.is_true(settings->sound);
    });

    gba::test("emplace forwards constructor arguments, like std::tuple's emplace", [] {
        gba::bits::backup::table<player_record, settings_record> saves;

        gba::test.expect.is_true(saves.emplace<0>(7u, 1));
        gba::test.expect.is_true(saves.emplace<settings_save>(true));

        const auto player = saves.get<player_save>();
        const auto settings = saves.get<1>();
        gba::test.expect.is_true(player.has_value());
        gba::test.expect.eq(player->score, 7u);
        gba::test.expect.eq(player->level, std::uint16_t{1});
        gba::test.expect.is_true(settings.has_value());
        gba::test.expect.is_true(settings->sound);
    });

    gba::test("erasing one field does not disturb its neighbor", [] {
        gba::bits::backup::table<player_record, settings_record> saves;

        gba::test.expect.is_true(saves.emplace<0>(player_save{.score = 7, .level = 1}));
        gba::test.expect.is_true(saves.emplace<1>(settings_save{.sound = true}));

        saves.erase<0>();
        gba::test.expect.is_false(saves.get<0>().has_value());
        gba::test.expect.is_true(saves.get<1>().has_value());
    });

    return gba::test.finish();
}
