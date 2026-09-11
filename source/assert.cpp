#include <cstdint>
#ifndef NDEBUG

#include "crash_screen.hpp"

using namespace crash;

namespace {

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
        std::uint16_t if_reg;
    };

} // namespace

// Linker-visible symbol called from assert_entry.S.
extern "C" [[noreturn, gnu::used]]
// NOLINTNEXTLINE(readability-identifier-naming)
void _stdgba_assert_render(const assert_state* state) {
    auto* vramPtr = vram();

    constexpr int label_offset = 6 * (font_w + 1);
    char hexBuf[16];

    draw_rect(vramPtr, 0, 0, width, height, color_bg);

    int y = 4;

    y = draw_string(vramPtr, 4, y, "ASSERT FAILED", color_red);
    y += font_h + 4;

    draw_string(vramPtr, 4, y, "Expr:", color_gray);
    y = draw_string(vramPtr, 4 + label_offset, y, state->expr, color_yellow);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "File:", color_gray);
    y = draw_string(vramPtr, 4 + label_offset, y, state->file, color_white);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "Line:", color_gray);
    y = draw_string(vramPtr, 4 + label_offset, y, itoa(hexBuf, state->line), color_white);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "Func:", color_gray);
    y = draw_string(vramPtr, 4 + label_offset, y, state->func, color_white);
    y += font_h + 4;

    // Register info section
    y = draw_string(vramPtr, 4, y, "Registers:", color_cyan);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "SP:", color_gray);
    draw_string(vramPtr, 4 + (4 * (font_w + 1)), y, hex32(hexBuf, state->sp), color_white);
    draw_string(vramPtr, 120, y, "LR:", color_gray);
    draw_string(vramPtr, 120 + (4 * (font_w + 1)), y, hex32(hexBuf, state->lr), color_white);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "CPSR:", color_gray);
    draw_string(vramPtr, 4 + (6 * (font_w + 1)), y, hex32(hexBuf, state->cpsr), color_white);
    y += font_h + 4;

    // Hardware state
    y = draw_string(vramPtr, 4, y, "Hardware:", color_cyan);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "DISPCNT:", color_gray);
    draw_string(vramPtr, 4 + (9 * (font_w + 1)), y, hex16(hexBuf, state->dispcnt), color_white);
    draw_string(vramPtr, 120, y, "IME:", color_gray);
    draw_string(vramPtr, 120 + (5 * (font_w + 1)), y, (state->ime != 0u) ? "1" : "0",
                (state->ime != 0u) ? color_yellow : color_white);
    y += font_h + 2;

    draw_string(vramPtr, 4, y, "IE:", color_gray);
    draw_string(vramPtr, 4 + (4 * (font_w + 1)), y, hex16(hexBuf, state->ie), color_white);
    draw_string(vramPtr, 120, y, "IF:", color_gray);
    draw_string(vramPtr, 120 + (4 * (font_w + 1)), y, hex16(hexBuf, state->if_reg), color_white);

    for (;;) {}
}

#endif // NDEBUG
