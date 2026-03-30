/// @file demo_colors.cpp
/// @brief Palette color swatches.
///
/// Draws a row of color blocks using Mode 0 tiles. Each block shows a different palette color constructed via the
/// stdgba color API.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/video>

static void fill_tile_solid(int tile_idx) {
    // Fill every nibble with palette index 1 (0x11111111 per row)
    gba::mem_tile_4bpp[0][tile_idx] = {
        0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111, 0x11111111,
    };
}

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {
        .video_mode = 0,
        .enable_bg0 = true,
    };

    // Use charblock 0 for tiles, screenblock 31 for map
    gba::reg_bgcnt[0] = {.screenblock = 31};

    // Create a solid tile (palette index 1 everywhere)
    fill_tile_solid(1);

    // Set up 8 color swatches across the top row
    using namespace gba;
    using namespace gba::literals;
    pal_bg_bank[0][1] = "red"_clr;            // CSS: red
    pal_bg_bank[1][1] = "lime"_clr;           // CSS: lime (pure green)
    pal_bg_bank[2][1] = "blue"_clr;           // CSS: blue
    pal_bg_bank[3][1] = "gold"_clr;           // CSS: gold
    pal_bg_bank[4][1] = "cyan"_clr;           // CSS: cyan
    pal_bg_bank[5][1] = "magenta"_clr;        // CSS: magenta
    pal_bg_bank[6][1] = "white"_clr;          // CSS: white
    pal_bg_bank[7][1] = "cornflowerblue"_clr; // CSS: cornflowerblue

    // Background color (palette 0, index 0)
    pal_bg_mem[0] = {.red = 2, .green = 2, .blue = 4};

    // Place 3x3 blocks of the solid tile across screen row 8-10
    for (int swatch = 0; swatch < 8; ++swatch) {
        for (int dy = 0; dy < 3; ++dy) {
            for (int dx = 0; dx < 3; ++dx) {
                int map_x = 1 + swatch * 4 + dx;
                int map_y = 8 + dy;
                mem_se[31][map_x + map_y * 32] = {
                    .tile_index = 1,
                    .palette_index = static_cast<unsigned short>(swatch),
                };
            }
        }
    }

    while (true) {
        gba::VBlankIntrWait();
    }
}
