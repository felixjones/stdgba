/// @file demo_hello_vblank.cpp
/// @brief Minimal VBlank loop with a visible result. (Hello VBlank chapter)
///
/// Sets up Mode 3 and draws a white rectangle in the center to prove
/// the program ran.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/video>

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {.video_mode = 3, .enable_bg2 = true};

    // Draw a white 40x20 rectangle centered on the 240x160 screen
    for (int y = 70; y < 90; ++y) {
        for (int x = 100; x < 140; ++x) {
            gba::mem_vram[x + y * 240] = 0x7FFF;
        }
    }

    while (true) {
        gba::VBlankIntrWait();
    }
}
