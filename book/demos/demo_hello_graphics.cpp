/// @file demo_hello_graphics.cpp
/// @brief First visible graphics demo using a consteval sprite.
///
/// Uses gba::shapes to build a 16x16 sprite at compile time, uploads it to OBJ
/// VRAM, and places it in the center of the screen.

#include <gba/bios>
#include <gba/color>
#include <gba/interrupt>
#include <gba/shapes>
#include <gba/video>

#include <cstring>

using namespace gba::shapes;
using gba::operator""_clr;

namespace {

    constexpr auto spr_ball = sprite_16x16(circle(8.0, 8.0, 7.0));

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
    obj.x = (240 - 16) / 2;
    obj.y = (160 - 16) / 2;
    obj.palette_index = 0;
    gba::obj_mem[0] = obj;

    for (int i = 1; i < 128; ++i) {
        gba::obj_mem[i] = {.disable = true};
    }

    while (true) {
        gba::VBlankIntrWait();
    }
}
