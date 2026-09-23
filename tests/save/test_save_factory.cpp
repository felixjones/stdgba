/// @file test_save_factory.cpp
/// @brief Tests for the backend-named save table factories (gba::backup_*).

#include <gba/backup>
#include <gba/testing>

#include <concepts>
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

    struct high_score_save {
        std::uint32_t score;
    };

    constexpr auto high_score_codec = gba::backup_redundant(
        gba::codec::make_tuple_codec(gba::codec::member<&high_score_save::score>(gba::codec::uint32_codec))
            .apply<high_score_save>());

    using sram_factory_table = decltype(gba::bits::backup::backup_sram<player_codec, settings_codec>())::field_type<0>;
    static_assert(std::same_as<sram_factory_table::backend_type, gba::bits::backup::sram_backend>,
                  "backup_sram should place records directly on sram_backend, with no runtime indirection");

} // namespace

int main() {
    gba::test("backup_sram<Codecs...> builds a table with one record per codec", [] {
        auto saves = gba::bits::backup::backup_sram<player_codec, settings_codec>();

        gba::test.expect.is_true(saves.emplace<0>(player_save{.score = 42, .level = 3}));
        gba::test.expect.is_true(saves.emplace<settings_save>(true));

        const auto player = saves.get<player_save>();
        const auto settings = saves.get<1>();
        gba::test.expect.is_true(player.has_value());
        gba::test.expect.is_true(settings.has_value());
        gba::test.expect.eq(player->score, 42u);
        gba::test.expect.is_true(settings->sound);
    });

    gba::test("gba::backup can hold a factory-built table, named explicitly like gba::link", [] {
        using save_table = decltype(gba::backup_sram<player_codec, settings_codec>)::table_type;

        auto& saves = gba::backup.emplace(gba::backup_sram<player_codec, settings_codec>);

        gba::test.expect.is_true(gba::backup.holds<save_table>());
        gba::test.expect.eq(gba::backup.get_if<save_table>(), &saves);
        gba::test.expect.is_true(saves.emplace<player_save>(7u, 1));
        gba::test.expect.is_true(saves.emplace<settings_save>());

        gba::test.expect.is_true(saves.get<player_save>().has_value());
        gba::test.expect.is_false(saves.get<settings_save>()->sound);

        // Reached again from elsewhere, without the reference emplace() returned.
        gba::test.expect.eq(gba::backup.get<save_table>().get<player_save>()->score, 7u);
    });

    gba::test("a redundant fixed-size field's alternate copy is reachable through the held table", [] {
        using save_table = decltype(gba::backup_sram<player_codec, high_score_codec>)::table_type;

        auto& saves = gba::backup.emplace(gba::backup_sram<player_codec, high_score_codec>);

        gba::test.expect.is_false(saves.get_alternate<high_score_save>().has_value());

        gba::test.expect.is_true(saves.emplace<high_score_save>(100u));
        gba::test.expect.is_true(saves.emplace<high_score_save>(200u));
        gba::test.expect.eq(saves.get<high_score_save>()->score, 200u);
        gba::test.expect.eq(saves.get_alternate<high_score_save>()->score, 100u);

        // A non-redundant field alongside it is unaffected, reached the same way.
        gba::test.expect.is_true(saves.emplace<player_save>(1u, 1));
        gba::test.expect.eq(gba::backup.get<save_table>().get<player_save>()->score, 1u);
    });

    return gba::test.finish();
}
