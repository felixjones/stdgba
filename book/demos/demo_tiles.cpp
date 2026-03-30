/// @file demo_tiles.cpp
/// @brief Tile mode 0 with auto-scrolling checkerboard.
///
/// Creates two tiles (light and dark), fills the screen with a checkerboard, and auto-scrolls diagonally.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/video>

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {.video_mode = 0, .enable_bg0 = true};
    gba::reg_bgcnt[0] = {.screenblock = 31};

    // Palette
    gba::pal_bg_mem[0] = {.red = 2, .green = 2, .blue = 6};
    gba::pal_bg_bank[0][1] = {.red = 10, .green = 14, .blue = 20};
    gba::pal_bg_bank[0][2] = {.red = 4, .green = 6, .blue = 12};

    // Tile 1: solid light (palette index 1)
    gba::mem_tile_4bpp[0][1] = {
        0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111,
    };

    // Tile 2: solid dark (palette index 2)
    gba::mem_tile_4bpp[0][2] = {
        0x22222222, 0x22222222, 0x22222222, 0x22222222, 0x22222222, 0x22222222, 0x22222222, 0x22222222,
    };

    // Fill the 32x32 tilemap with a checkerboard
    for (int ty = 0; ty < 32; ++ty)
        for (int tx = 0; tx < 32; ++tx)
            gba::mem_se[31][tx + ty * 32] = {
                .tile_index = static_cast<unsigned short>(((tx ^ ty) & 1) ? 2 : 1),
            };

    int scroll_x = 0, scroll_y = 0;

    while (true) {
        gba::VBlankIntrWait();

        ++scroll_x;
        ++scroll_y;

        gba::reg_bgofs[0][0] = static_cast<short>(scroll_x);
        gba::reg_bgofs[0][1] = static_cast<short>(scroll_y);
    }
}
