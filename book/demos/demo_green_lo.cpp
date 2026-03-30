/// @file demo_green_lo.cpp
/// @brief Hidden word revealed by 6-bit green channel.

#include <gba/video>

static constexpr unsigned char glyphs[][5] = {
    {0b101, 0b101, 0b111, 0b101, 0b101}, // H
    {0b111, 0b100, 0b111, 0b100, 0b111}, // E
    {0b100, 0b100, 0b100, 0b100, 0b111}, // L
    {0b100, 0b100, 0b100, 0b100, 0b111}, // L
    {0b111, 0b101, 0b101, 0b101, 0b111}, // O
};

static void draw_glyph(int g, int px, int py, int scale, unsigned short color) {
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (!(glyphs[g][row] & (4 >> col))) continue;
            for (int sy = 0; sy < scale; ++sy)
                for (int sx = 0; sx < scale; ++sx)
                    gba::mem_vram[(px + col * scale + sx) + (py + row * scale + sy) * 240] = color;
        }
    }
}

int main() {
    gba::reg_dispcnt = {.video_mode = 3, .enable_bg2 = true};

    constexpr short base = 12 << 5;                     // green=12
    constexpr unsigned short hidden = base | (1 << 15); // green=12, grn_lo=1

    for (int i = 0; i < 240 * 160; ++i) gba::mem_vram[i] = base;

    constexpr int scale = 6, ox = (240 - 19 * scale) / 2, oy = (160 - 5 * scale) / 2;
    for (int i = 0; i < 5; ++i) draw_glyph(i, ox + i * 4 * scale, oy, scale, hidden);

    // Brightness increase on BG2 - hardware processes the full 6-bit
    // green channel, revealing the hidden text on real hardware
    gba::reg_bldcnt = {.dest_bg2 = true, .blend_op = gba::blend_op_brighten};
    using namespace gba::literals;
    gba::reg_bldy = 0.25_fx;

    for (;;) {}
}
