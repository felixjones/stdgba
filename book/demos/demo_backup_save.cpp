/// @file demo_backup_save.cpp
/// @brief Persist a sprite's position and a background colour setting across resets.
///
/// Two independently placed `gba::backup` fields share one SRAM chip: `player_save` (the
/// ball's position, saved with A) and `settings_save` (an inverted-colours toggle, saved
/// with SELECT). Both are loaded once at boot, falling back to defaults when absent.

#include <gba/backup>
#include <gba/bios>
#include <gba/codec>
#include <gba/color>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/shapes>
#include <gba/video>

#include <algorithm>
#include <cstring>

using namespace gba::shapes;
using gba::operator""_clr;

namespace {

    constexpr int screen_width = 240;
    constexpr int screen_height = 160;
    constexpr int sprite_size = 16;

    constexpr auto spr_ball = sprite_16x16(circle(8.0, 8.0, 7.0));

    struct player {
        std::uint8_t x = (screen_width - sprite_size) / 2;
        std::uint8_t y = (screen_height - sprite_size) / 2;

        static constexpr auto codec =
            gba::codec::make_tuple_codec(gba::codec::member<&player::x>(gba::codec::uint8_codec),
                                         gba::codec::member<&player::y>(gba::codec::uint8_codec))
                .apply<player>();
    };

    struct settings {
        bool invert = false;

        static constexpr auto codec =
            gba::codec::make_tuple_codec(gba::codec::member<&settings::invert>(gba::codec::bool_codec))
                .apply<settings>();
    };

    int clamp(const int value, const int lo, const int hi) {
        if (value > hi) {
            return value < lo ? lo : hi;
        }
        return value < lo ? lo : value;
    }

    void apply_theme(const bool invert) {
        gba::pal_bg_mem[0] = invert ? "white"_clr : "#102040"_clr;
        gba::pal_obj_bank[0][1] = invert ? "#102040"_clr : "white"_clr;
    }

} // namespace

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {
        .linear_obj_tilemap = true,
        .enable_obj = true,
    };

    // One SRAM-backed table for both fields; each is auto-placed on its own aligned offset.
    auto& backup = gba::backup.emplace(gba::backup_sram<player::codec, settings::codec>);

    // Load the ball's last saved position, or fall back to player_save's default centre.
    player player = backup.get<::player>().value_or(::player{});

    // Load the settings, default-constructing and storing them if this is the first boot.
    if (!backup.get<settings>().has_value()) {
        backup.emplace<settings>();
    }
    settings settings = *backup.get<::settings>();

    apply_theme(settings.invert);

    auto* objDst = gba::memory_map(gba::mem_vram_obj);
    std::memcpy(objDst, spr_ball.data(), spr_ball.size());

    auto obj = spr_ball.obj(gba::tile_index(objDst));
    obj.x = player.x;
    obj.y = player.y;
    gba::obj_mem[0] = obj;

    std::fill(std::begin(gba::obj_mem) + 1, std::end(gba::obj_mem), gba::object{.disable = true});

    gba::keypad keys;

    while (true) {
        gba::VBlankIntrWait();
        keys = gba::reg_keyinput;

        const int x = clamp(player.x + keys.xaxis(), 0, screen_width - sprite_size);
        const int y = clamp(player.y + keys.i_yaxis(), 0, screen_height - sprite_size);
        player.x = static_cast<std::uint8_t>(x);
        player.y = static_cast<std::uint8_t>(y);

        // Save on the rising edge of A, so a held button doesn't wear out Flash/EEPROM
        // with repeated writes (SRAM has no such limit, but the pattern still applies).
        if (keys.pressed(gba::key_a)) {
            backup.emplace<::player>(player);
        }

        if (keys.pressed(gba::key_select)) {
            settings.invert = !settings.invert;
            apply_theme(settings.invert);
            backup.emplace<::settings>(settings);
        }

        obj.x = player.x;
        obj.y = player.y;
        gba::obj_mem[0] = obj;
    }
}
