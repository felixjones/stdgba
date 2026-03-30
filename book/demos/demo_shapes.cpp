/// @file demo_shapes.cpp
/// @brief Consteval sprite shapes.
///
/// Draws several sprites generated at compile time using the shapes API.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/shapes>
#include <gba/video>

#include <cstring>

using namespace gba::shapes;

// Compile-time sprites
constexpr auto spr_circle = sprite_16x16(circle(8.0, 8.0, 7.0));

constexpr auto spr_donut = sprite_16x16(circle(8.0, 8.0, 7.0), palette_idx(0), circle(8.0, 8.0, 3.0));

constexpr auto spr_rect = sprite_16x16(rect(1, 1, 14, 14));

constexpr auto spr_triangle = sprite_16x16(triangle(8, 1, 15, 14, 1, 14));

constexpr auto spr_face = sprite_32x32(circle(16.0, 16.0, 14.0), // Head (palette 1)
                                       group(                    // Eyes (palette 2)
                                           circle(11.0, 12.0, 2.5), circle(21.0, 12.0, 2.5)),
                                       group( // Mouth (palette 3)
                                           oval(10, 20, 12, 4)),
                                       palette_idx(0),     // Erase
                                       oval(11, 21, 10, 2) // Inner mouth cutout
);

constexpr auto spr_label = sprite_64x32(text(2, 2, "stdgba"),
                                        group(),                      // Reserve palette 2
                                        rect_outline(0, 0, 64, 14, 1) // Border (palette 3)
);

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

    // Background
    gba::pal_bg_mem[0] = {.red = 4, .green = 6, .blue = 10};

    // Sprite palettes
    gba::pal_obj_bank[0][1] = {.red = 28, .green = 8, .blue = 8};  // Red
    gba::pal_obj_bank[1][1] = {.red = 8, .green = 28, .blue = 8};  // Green
    gba::pal_obj_bank[2][1] = {.red = 8, .green = 8, .blue = 28};  // Blue
    gba::pal_obj_bank[3][1] = {.red = 28, .green = 28, .blue = 8}; // Yellow

    // Face palette
    gba::pal_obj_bank[4][1] = {.red = 31, .green = 25, .blue = 12}; // Skin
    gba::pal_obj_bank[4][2] = {.red = 4, .green = 4, .blue = 8};    // Eyes
    gba::pal_obj_bank[4][3] = {.red = 24, .green = 8, .blue = 8};   // Mouth

    // Label palette
    gba::pal_obj_bank[5][1] = {.red = 31, .green = 31, .blue = 31}; // Text
    gba::pal_obj_bank[5][3] = {.red = 16, .green = 20, .blue = 28}; // Border

    // Copy tile data to OBJ VRAM
    auto* dest = gba::memory_map(gba::mem_vram_obj);
    auto* base = dest;

    auto copy_sprite = [&](const auto& spr) {
        auto idx = gba::tile_index(dest);
        std::memcpy(dest, spr.data(), spr.size());
        dest += spr.size() / sizeof(*dest);
        return idx;
    };

    auto idx_circle = copy_sprite(spr_circle);
    auto idx_donut = copy_sprite(spr_donut);
    auto idx_rect = copy_sprite(spr_rect);
    auto idx_triangle = copy_sprite(spr_triangle);
    auto idx_face = copy_sprite(spr_face);
    auto idx_label = copy_sprite(spr_label);

    // Place sprites across the screen
    auto place = [](int slot, auto spr_data, unsigned short tile_idx, unsigned short x, unsigned short y,
                    unsigned short pal) {
        auto obj = spr_data.obj(tile_idx);
        obj.x = x;
        obj.y = y;
        obj.palette_index = pal;
        gba::obj_mem[slot] = obj;
    };

    place(0, spr_circle, idx_circle, 20, 64, 0);
    place(1, spr_donut, idx_donut, 52, 64, 1);
    place(2, spr_rect, idx_rect, 84, 64, 2);
    place(3, spr_triangle, idx_triangle, 116, 64, 3);
    place(4, spr_face, idx_face, 156, 56, 4);
    place(5, spr_label, idx_label, 88, 120, 5);

    // Hide remaining sprites
    for (int i = 6; i < 128; ++i) {
        gba::obj_mem[i] = {.disable = true};
    }

    while (true) {
        gba::VBlankIntrWait();
    }
}
