/// @file demo_mode3_pixel.cpp
/// @brief Mode 3 bitmap: draw colored pixels. (Video Modes chapter)

#include <gba/bios>
#include <gba/interrupt>
#include <gba/video>

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {.video_mode = 3, .enable_bg2 = true};

    // Draw a red pixel at (120, 80) - center of screen
    gba::mem_vram[120 + 80 * 240] = 0x001F;

    // Draw a green pixel one to the right
    gba::mem_vram[121 + 80 * 240] = 0x03E0;

    // Draw a blue pixel one below
    gba::mem_vram[120 + 81 * 240] = 0x7C00;

    while (true) {
        gba::VBlankIntrWait();
    }
}
