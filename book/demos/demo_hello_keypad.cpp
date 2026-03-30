/// @file demo_hello_keypad.cpp
/// @brief Move a consteval sprite with the D-pad.
///
/// Polls keypad input once per frame and moves a gba::shapes sprite via OAM.

#include <gba/bios>
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

    int clamp(int value, int lo, int hi) {
        if (value < lo) {
            return lo;
        }
        if (value > hi) {
            return hi;
        }
        return value;
    }

} // namespace

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {
        .video_mode = 0,
        .linear_obj_tilemap = true,
        .enable_obj = true,
    };

    gba::pal_bg_mem[0] = "#102040"_clr;
    gba::pal_obj_bank[0][1] = "white"_clr;

    auto* objDst = gba::memory_map(gba::mem_vram_obj);
    std::memcpy(objDst, spr_ball.data(), spr_ball.size());
    const auto tileIdx = gba::tile_index(objDst);

    auto obj = spr_ball.obj(tileIdx);
    obj.palette_index = 0;

    int spriteX = (screen_width - sprite_size) / 2;
    int spriteY = (screen_height - sprite_size) / 2;
    obj.x = static_cast<unsigned short>(spriteX);
    obj.y = static_cast<unsigned short>(spriteY);
    gba::obj_mem[0] = obj;

    gba::object disabled{.disable = true};
    std::fill(std::begin(gba::obj_mem) + 1, std::end(gba::obj_mem), disabled);

    gba::keypad keys;

    while (true) {
        gba::VBlankIntrWait();
        keys = gba::reg_keyinput;

        spriteX += keys.xaxis();
        spriteY += keys.i_yaxis();

        spriteX = clamp(spriteX, 0, screen_width - sprite_size);
        spriteY = clamp(spriteY, 0, screen_height - sprite_size);

        obj.x = static_cast<unsigned short>(spriteX);
        obj.y = static_cast<unsigned short>(spriteY);
        gba::obj_mem[0] = obj;
    }
}
