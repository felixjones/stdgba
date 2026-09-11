#include <cstdint>

#include "crash_screen.hpp"

using namespace crash;

namespace {

    struct divzero_state {
        std::uint32_t is_ldiv;
        // Exact register-dump layout produced by divzero_entry.s.
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        std::uint32_t r[13];
        std::uint32_t sp;
        std::uint32_t lr;
        std::uint32_t cpsr;
        std::uint16_t dispcnt;
        std::uint16_t ime;
        std::uint16_t ie;
        std::uint16_t if_reg;
    };

    const char* reg_label(char* buf, int n) {
        buf[0] = 'R';
        if (n >= 10) {
            buf[1] = static_cast<char>('0' + (n / 10));
            buf[2] = static_cast<char>('0' + (n % 10));
            buf[3] = ':';
            buf[4] = '\0';
        } else {
            buf[1] = static_cast<char>('0' + n);
            buf[2] = ':';
            buf[3] = '\0';
        }
        return buf;
    }

} // namespace

// Linker-visible symbol called from divzero_entry.s.
extern "C" [[noreturn, gnu::used]]
// NOLINTNEXTLINE(readability-identifier-naming)
void _stdgba_divzero_render(const divzero_state* state) {
    auto* vramPtr = vram();
    char hexBuf[16];
    char labelBuf[8];
    constexpr int col2 = 120;
    constexpr int val_off = 5 * (font_w + 1);
    draw_rect(vramPtr, 0, 0, width, 126, color_bg);
    int y = 4;
    // Title with idiv0/ldiv0 tag
    y = draw_string(vramPtr, 4, y, "DIVISION BY ZERO", color_red);
    draw_string(vramPtr, 4 + (17 * (font_w + 1)), y, (state->is_ldiv != 0u) ? "(ldiv0)" : "(idiv0)", color_yellow);
    y += font_h + 4;
    // Registers section
    y = draw_string(vramPtr, 4, y, "Registers:", color_cyan);
    y += font_h + 2;
    draw_string(vramPtr, 4, y, "SP:", color_gray);
    draw_string(vramPtr, 4 + (4 * (font_w + 1)), y, hex32(hexBuf, state->sp), color_white);
    draw_string(vramPtr, col2, y, "LR:", color_gray);
    draw_string(vramPtr, col2 + (4 * (font_w + 1)), y, hex32(hexBuf, state->lr), color_white);
    y += font_h + 2;
    draw_string(vramPtr, 4, y, "CPSR:", color_gray);
    draw_string(vramPtr, 4 + (6 * (font_w + 1)), y, hex32(hexBuf, state->cpsr), color_white);
    y += font_h + 4;
    // Hardware section
    y = draw_string(vramPtr, 4, y, "Hardware:", color_cyan);
    y += font_h + 2;
    draw_string(vramPtr, 4, y, "DISPCNT:", color_gray);
    draw_string(vramPtr, 4 + (9 * (font_w + 1)), y, hex16(hexBuf, state->dispcnt), color_white);
    draw_string(vramPtr, col2, y, "IME:", color_gray);
    draw_string(vramPtr, col2 + (5 * (font_w + 1)), y, (state->ime != 0u) ? "1" : "0",
                (state->ime != 0u) ? color_yellow : color_white);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "IE:", color_gray);
    draw_string(vramPtr, 4 + (4 * (font_w + 1)), y, hex16(hexBuf, state->ie), color_white);
    draw_string(vramPtr, col2, y, "IF:", color_gray);
    draw_string(vramPtr, col2 + (4 * (font_w + 1)), y, hex16(hexBuf, state->if_reg), color_white);
    y += font_h;

    // Divider
    y += font_h;
    // R0-R12 in two columns
    for (int i = 0; i < 13; i += 2) {
        draw_string(vramPtr, 4, y, reg_label(labelBuf, i), color_gray);
        draw_string(vramPtr, 4 + val_off, y, hex32(hexBuf, state->r[i]), color_white);
        if (i + 1 < 13) {
            draw_string(vramPtr, col2, y, reg_label(labelBuf, i + 1), color_gray);
            draw_string(vramPtr, col2 + val_off, y, hex32(hexBuf, state->r[i + 1]), color_white);
        }
        y += font_h + 2;
    }
    for (;;) {}
}
