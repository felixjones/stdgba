#include "crash_screen.hpp"

using namespace crash;

struct divzero_state {
    std::uint32_t is_ldiv;
    std::uint32_t r[13];
    std::uint32_t sp;
    std::uint32_t lr;
    std::uint32_t cpsr;
    std::uint16_t dispcnt;
    std::uint16_t ime;
    std::uint16_t ie;
    std::uint16_t if_;
};

static const char* reg_label(char* buf, int n) {
    buf[0] = 'R';
    if (n >= 10) {
        buf[1] = '0' + n / 10;
        buf[2] = '0' + n % 10;
        buf[3] = ':';
        buf[4] = '\0';
    } else {
        buf[1] = '0' + n;
        buf[2] = ':';
        buf[3] = '\0';
    }
    return buf;
}

extern "C" [[noreturn, gnu::used]]
void _stdgba_divzero_render(const divzero_state* state) {
    const auto VRAM = vram();
    char hex_buf[16];
    char label_buf[8];
    constexpr int COL2 = 120;
    constexpr int VAL_OFF = 5 * (FONT_W + 1);
    draw_rect(VRAM, 0, 0, WIDTH, 126, BG);
    int y = 4;
    // Title with idiv0/ldiv0 tag
    y = draw_string(VRAM, 4, y, "DIVISION BY ZERO", RED);
    draw_string(VRAM, 4 + 17 * (FONT_W + 1), y, state->is_ldiv ? "(ldiv0)" : "(idiv0)", YELLOW);
    y += FONT_H + 4;
    // Registers section
    y = draw_string(VRAM, 4, y, "Registers:", CYAN);
    y += FONT_H + 2;
    draw_string(VRAM, 4, y, "SP:", GRAY);
    draw_string(VRAM, 4 + 4 * (FONT_W + 1), y, hex32(hex_buf, state->sp), WHITE);
    draw_string(VRAM, COL2, y, "LR:", GRAY);
    draw_string(VRAM, COL2 + 4 * (FONT_W + 1), y, hex32(hex_buf, state->lr), WHITE);
    y += FONT_H + 2;
    draw_string(VRAM, 4, y, "CPSR:", GRAY);
    draw_string(VRAM, 4 + 6 * (FONT_W + 1), y, hex32(hex_buf, state->cpsr), WHITE);
    y += FONT_H + 4;
    // Hardware section
    y = draw_string(VRAM, 4, y, "Hardware:", CYAN);
    y += FONT_H + 2;
    draw_string(VRAM, 4, y, "DISPCNT:", GRAY);
    draw_string(VRAM, 4 + 9 * (FONT_W + 1), y, hex16(hex_buf, state->dispcnt), WHITE);
    draw_string(VRAM, COL2, y, "IME:", GRAY);
    draw_string(VRAM, COL2 + 5 * (FONT_W + 1), y, state->ime ? "1" : "0", state->ime ? YELLOW : WHITE);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "IE:", GRAY);
    draw_string(VRAM, 4 + 4 * (FONT_W + 1), y, hex16(hex_buf, state->ie), WHITE);
    draw_string(VRAM, COL2, y, "IF:", GRAY);
    draw_string(VRAM, COL2 + 4 * (FONT_W + 1), y, hex16(hex_buf, state->if_), WHITE);
    y += FONT_H;

    // Divider
    y += FONT_H;
    // R0-R12 in two columns
    for (int i = 0; i < 13; i += 2) {
        draw_string(VRAM, 4, y, reg_label(label_buf, i), GRAY);
        draw_string(VRAM, 4 + VAL_OFF, y, hex32(hex_buf, state->r[i]), WHITE);
        if (i + 1 < 13) {
            draw_string(VRAM, COL2, y, reg_label(label_buf, i + 1), GRAY);
            draw_string(VRAM, COL2 + VAL_OFF, y, hex32(hex_buf, state->r[i + 1]), WHITE);
        }
        y += FONT_H + 2;
    }
    for (;;) {}
}
