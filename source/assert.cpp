#ifndef NDEBUG

#include "crash_screen.hpp"

using namespace crash;

struct assert_state {
    const char* file;
    int line;
    const char* func;
    const char* expr;
    std::uint32_t sp;
    std::uint32_t lr;
    std::uint32_t cpsr;
    std::uint16_t dispcnt;
    std::uint16_t ime;
    std::uint16_t ie;
    std::uint16_t if_;
};

extern "C" [[noreturn, gnu::used]]
void _stdgba_assert_render(const assert_state* state) {
    const auto VRAM = vram();

    constexpr int LABEL_OFFSET = 6 * (FONT_W + 1);
    char hex_buf[16];

    draw_rect(VRAM, 0, 0, WIDTH, HEIGHT, BG);

    int y = 4;

    y = draw_string(VRAM, 4, y, "ASSERT FAILED", RED);
    y += FONT_H + 4;

    draw_string(VRAM, 4, y, "Expr:", GRAY);
    y = draw_string(VRAM, 4 + LABEL_OFFSET, y, state->expr, YELLOW);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "File:", GRAY);
    y = draw_string(VRAM, 4 + LABEL_OFFSET, y, state->file, WHITE);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "Line:", GRAY);
    y = draw_string(VRAM, 4 + LABEL_OFFSET, y, itoa(hex_buf, state->line), WHITE);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "Func:", GRAY);
    y = draw_string(VRAM, 4 + LABEL_OFFSET, y, state->func, WHITE);
    y += FONT_H + 4;

    // Register info section
    y = draw_string(VRAM, 4, y, "Registers:", CYAN);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "SP:", GRAY);
    draw_string(VRAM, 4 + 4 * (FONT_W + 1), y, hex32(hex_buf, state->sp), WHITE);
    draw_string(VRAM, 120, y, "LR:", GRAY);
    draw_string(VRAM, 120 + 4 * (FONT_W + 1), y, hex32(hex_buf, state->lr), WHITE);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "CPSR:", GRAY);
    draw_string(VRAM, 4 + 6 * (FONT_W + 1), y, hex32(hex_buf, state->cpsr), WHITE);
    y += FONT_H + 4;

    // Hardware state
    y = draw_string(VRAM, 4, y, "Hardware:", CYAN);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "DISPCNT:", GRAY);
    draw_string(VRAM, 4 + 9 * (FONT_W + 1), y, hex16(hex_buf, state->dispcnt), WHITE);
    draw_string(VRAM, 120, y, "IME:", GRAY);
    draw_string(VRAM, 120 + 5 * (FONT_W + 1), y, state->ime ? "1" : "0", state->ime ? YELLOW : WHITE);
    y += FONT_H + 2;

    draw_string(VRAM, 4, y, "IE:", GRAY);
    draw_string(VRAM, 4 + 4 * (FONT_W + 1), y, hex16(hex_buf, state->ie), WHITE);
    draw_string(VRAM, 120, y, "IF:", GRAY);
    draw_string(VRAM, 120 + 4 * (FONT_W + 1), y, hex16(hex_buf, state->if_), WHITE);

    for (;;) {}
}

#endif // NDEBUG
