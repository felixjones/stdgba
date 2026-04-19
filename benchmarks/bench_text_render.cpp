/// @file benchmarks/bench_text_render.cpp
/// @brief Benchmark measuring text rendering cost with a real embedded font.

#include <gba/benchmark>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/bits/text/font_variants.hpp>
#include <gba/text>
#include <gba/video>

#include <array>
#include <cstddef>


using namespace gba::literals;

static constexpr auto make_bench_bdf() {
    constexpr char source[] = R"(STARTFONT 2.1
FONT bench8x8
SIZE 8 75 75
FONTBOUNDINGBOX 5 7 0 0
STARTPROPERTIES 3
FONT_ASCENT 7
FONT_DESCENT 0
DEFAULT_CHAR 48
ENDPROPERTIES
CHARS 15
STARTCHAR space
ENCODING 32
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
00
00
00
00
00
00
00
ENDCHAR
STARTCHAR slash
ENCODING 47
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
08
10
10
20
20
40
80
ENDCHAR
STARTCHAR zero
ENCODING 48
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
70
88
98
A8
C8
88
70
ENDCHAR
STARTCHAR one
ENCODING 49
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
20
60
20
20
20
20
70
ENDCHAR
STARTCHAR two
ENCODING 50
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
70
88
08
10
20
40
F8
ENDCHAR
STARTCHAR three
ENCODING 51
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
F0
08
10
30
08
88
70
ENDCHAR
STARTCHAR four
ENCODING 52
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
10
30
50
90
F8
10
10
ENDCHAR
STARTCHAR five
ENCODING 53
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
F8
80
F0
08
08
88
70
ENDCHAR
STARTCHAR six
ENCODING 54
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
30
40
80
F0
88
88
70
ENDCHAR
STARTCHAR seven
ENCODING 55
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
F8
08
10
20
40
40
40
ENDCHAR
STARTCHAR eight
ENCODING 56
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
70
88
88
70
88
88
70
ENDCHAR
STARTCHAR nine
ENCODING 57
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
70
88
88
78
08
10
60
ENDCHAR
STARTCHAR colon
ENCODING 58
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
00
20
00
00
20
00
00
ENDCHAR
STARTCHAR H
ENCODING 72
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
88
88
88
F8
88
88
88
ENDCHAR
STARTCHAR P
ENCODING 80
SWIDTH 500 0
DWIDTH 8 0
BBX 5 7 0 0
BITMAP
F0
88
88
F0
80
80
80
ENDCHAR
ENDFONT
)";

    std::array<unsigned char, sizeof(source) - 1> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(source[i]);
    return data;
}

static constexpr auto font = gba::embed::bdf([] { return make_bench_bdf(); });
static constexpr auto font_shadowed = gba::text::with_shadow<1, 1>(font);
static constexpr auto fmt_hp = gba::text::text_format<"HP: {hp}/{max}">{ };
static constexpr auto fmt_hp_color = gba::text::text_format<"{pal:pal}HP: {hp}/{max}">{ };
static constexpr auto fmt_cursor_wrap_literal = gba::text::text_format<"AA A{{B}}C AA A{{B}}C">{ };
static constexpr auto fmt_cursor_wrap_mixed = gba::text::text_format<"AA A{value}B AA A{value}B">{ };

volatile unsigned int sink_u32 = 0;
volatile const void* sink_ptr = nullptr;

namespace {
    using layer_type = gba::text::bg4bpp_text_layer<240, 160>;
    using layer_cell_state_type = layer_type::cell_state_map;

    constexpr int iters = 64;

    constexpr auto cursor_metrics = gba::text::stream_metrics{
        .letter_spacing_px = 1,
        .line_spacing_px = 2,
        .tab_width_px = 16,
        .wrap_width_px = 40,
    };

    template<typename Cursor>
    [[gnu::noinline]]
    void keep_cursor_live(const Cursor& cursor) noexcept {
        sink_ptr = &cursor;
    }

    template<typename Layer>
    [[gnu::noinline]]
    void keep_layer_live(const Layer& layer) noexcept {
        sink_ptr = &layer;
    }

    constexpr auto config_1p_full = gba::text::bitplane_config{
        .profile = gba::text::bitplane_profile::one_plane_full_color,
        .palbank_0 = 0,
        .start_index = 1,
    };

    constexpr auto config_2p3c = gba::text::bitplane_config{
        .profile   = gba::text::bitplane_profile::two_plane_three_color,
        .palbank_0 = 0,
        .palbank_1 = 1,
        .start_index = 1,
    };

    constexpr auto config_2pbin = gba::text::bitplane_config{
        .profile   = gba::text::bitplane_profile::two_plane_binary,
        .palbank_0 = 0,
        .palbank_1 = 1,
        .start_index = 1,
    };

    constexpr auto config_3pbin = gba::text::bitplane_config{
        .profile   = gba::text::bitplane_profile::three_plane_binary,
        .palbank_0 = 0,
        .palbank_1 = 1,
        .palbank_2 = 2,
        .start_index = 1,
    };

    // Keep old alias for the existing one_plane_full_color benchmarks.
    constexpr auto& config = config_1p_full;

    layer_cell_state_type g_layer_cell_state{};

    [[nodiscard]]
    layer_type make_layer_for(const gba::text::bitplane_config& cfg) noexcept {
        return layer_type{31, cfg, {.next_tile = 1, .end_tile = 512}, g_layer_cell_state};
    }

    [[nodiscard]]
    layer_type make_layer() noexcept {
        return make_layer_for(config);
    }

    void clear_tiles() noexcept {
        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        for (int i = 0; i < 600; ++i) {
            tiles[i] = {};
        }
    }

    [[gnu::noinline]]
    void draw_single_char() noexcept {
        clear_tiles();
        auto layer = make_layer();
        layer.draw_char(font, static_cast<unsigned int>('H'), 0, font.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_single_char_shadow() noexcept {
        clear_tiles();
        auto layer = make_layer();
        layer.draw_char(font_shadowed, static_cast<unsigned int>('H'), 0, font_shadowed.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_cstr_short() noexcept {
        clear_tiles();
        auto layer = make_layer();
        layer.draw_stream(font, "HP: 42/99", 0, 0);
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_cstr_color_escape() noexcept {
        clear_tiles();
        auto layer = make_layer();
        layer.draw_stream(font, "\x02HP: \x03" "42/99", 0, 0);
        sink_u32 = layer.palette();
    }

    [[gnu::noinline]]
    void draw_format_string() noexcept {
        volatile int hp = 42;
        volatile int mx = 99;
        char buffer[32]{};
        clear_tiles();
        fmt_hp.to(buffer, "hp"_arg = hp, "max"_arg = mx);
        auto layer = make_layer();
        layer.draw_stream(font, buffer, 0, 0);
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_format_with_palette() noexcept {
        volatile int hp = 42;
        volatile int mx = 99;
        volatile int pal = 3;
        char buffer[32]{};
        clear_tiles();
        fmt_hp_color.to(buffer, "pal"_arg = pal, "hp"_arg = hp, "max"_arg = mx);
        auto layer = make_layer();
        layer.draw_stream(font, buffer, 0, 0);
        sink_u32 = layer.palette();
    }

    [[gnu::noinline]]
    void draw_full_screen_fill() noexcept {
        clear_tiles();
        auto layer = make_layer();
        for (int row = 0; row < 20; ++row) {
            const int y = row * static_cast<int>(font.line_height());
            for (int col = 0; col < 30; ++col) {
                const int x = col * static_cast<int>(font.font_width);
                layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)),
                                x, y + font.ascent);
            }
        }
        layer.flush_cache();
        sink_u32 = layer.cursor_row();
    }

    [[gnu::noinline]]
    void draw_full_screen_fill_shadow() noexcept {
        clear_tiles();
        auto layer = make_layer();
        for (int row = 0; row < 20; ++row) {
            const int y = row * static_cast<int>(font_shadowed.line_height());
            for (int col = 0; col < 30; ++col) {
                const int x = col * static_cast<int>(font_shadowed.font_width);
                layer.draw_char(font_shadowed, static_cast<unsigned int>('0' + (col % 10)),
                                x, y + font_shadowed.ascent);
            }
        }
        layer.flush_cache();
        sink_u32 = layer.cursor_row();
    }

    [[gnu::noinline]]
    void draw_cursor_literal_wrap() noexcept {
        clear_tiles();
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
            gba::text::stream(fmt_cursor_wrap_literal),
            0, 0, cursor_metrics);
        while (cursor.next_visible()) {}
        sink_u32 = static_cast<unsigned int>(cursor.emitted());
    }

    [[gnu::noinline]]
    void draw_cursor_mixed_wrap() noexcept {
        volatile int value = 7;
        clear_tiles();
        auto layer = make_layer();
        auto gen = fmt_cursor_wrap_mixed.generator("value"_arg = value);
        auto cursor = layer.make_cursor(font, gba::text::stream(gen, font, cursor_metrics),
                                        0, 0, cursor_metrics);
        while (cursor.next_visible()) {}
        sink_u32 = static_cast<unsigned int>(cursor.emitted());
    }

    [[gnu::noinline]]
    void init_cursor_cstr() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::cstr_stream("AA A{B}C AA A{B}C"),
                                        0, 0, cursor_metrics);
        keep_cursor_live(cursor);
    }

    [[gnu::noinline]]
    void init_cursor_literal_stream() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(fmt_cursor_wrap_literal),
                                        0, 0, cursor_metrics);
        keep_cursor_live(cursor);
    }

    [[gnu::noinline]]
    void init_cursor_mixed_stream() noexcept {
        volatile int value = 7;
        auto layer = make_layer();
        auto gen = fmt_cursor_wrap_mixed.generator("value"_arg = value);
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(gen, font, cursor_metrics),
                                        0, 0, cursor_metrics);
        keep_cursor_live(cursor);
    }

    [[gnu::noinline]]
    void init_layer_only() noexcept {
        auto layer = make_layer();
        keep_layer_live(layer);
    }

    [[gnu::noinline]]
    void init_layer_first_draw() noexcept {
        auto layer = make_layer();
        layer.draw_char(font, static_cast<unsigned int>('H'), 0, font.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void init_cursor_cstr_first_visible() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::cstr_stream("AA A{B}C AA A{B}C"),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next_visible());
    }

    [[gnu::noinline]]
    void init_cursor_literal_stream_first_visible() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(fmt_cursor_wrap_literal),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next_visible());
    }

    [[gnu::noinline]]
    void init_cursor_mixed_stream_first_visible() noexcept {
        volatile int value = 7;
        auto layer = make_layer();
        auto gen = fmt_cursor_wrap_mixed.generator("value"_arg = value);
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(gen, font, cursor_metrics),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next_visible());
    }

    [[gnu::noinline]]
    void init_cursor_cstr_first_step() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::cstr_stream("AA A{B}C AA A{B}C"),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next());
    }

    [[gnu::noinline]]
    void init_cursor_literal_stream_first_step() noexcept {
        auto layer = make_layer();
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(fmt_cursor_wrap_literal),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next());
    }

    [[gnu::noinline]]
    void init_cursor_mixed_stream_first_step() noexcept {
        volatile int value = 7;
        auto layer = make_layer();
        auto gen = fmt_cursor_wrap_mixed.generator("value"_arg = value);
        auto cursor = layer.make_cursor(font,
                                        gba::text::stream(gen, font, cursor_metrics),
                                        0, 0, cursor_metrics);
        sink_u32 = static_cast<unsigned int>(cursor.next());
    }

    [[gnu::noinline]]
    void draw_single_char_2p3c() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_2p3c);
        layer.draw_char(font, static_cast<unsigned int>('H'), 0, font.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_full_screen_fill_2p3c() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_2p3c);
        for (int row = 0; row < 20; ++row) {
            const int y = row * static_cast<int>(font.line_height());
            for (int col = 0; col < 30; ++col) {
                const int x = col * static_cast<int>(font.font_width);
                layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)),
                                x, y + font.ascent);
            }
        }
        layer.flush_cache();
        sink_u32 = layer.cursor_row();
    }

    [[gnu::noinline]]
    void draw_single_char_2pbin() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_2pbin);
        layer.draw_char(font, static_cast<unsigned int>('H'), 0, font.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_full_screen_fill_2pbin() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_2pbin);
        for (int row = 0; row < 20; ++row) {
            const int y = row * static_cast<int>(font.line_height());
            for (int col = 0; col < 30; ++col) {
                const int x = col * static_cast<int>(font.font_width);
                layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)),
                                x, y + font.ascent);
            }
        }
        layer.flush_cache();
        sink_u32 = layer.cursor_row();
    }

    [[gnu::noinline]]
    void draw_single_char_3pbin() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_3pbin);
        layer.draw_char(font, static_cast<unsigned int>('H'), 0, font.ascent);
        layer.flush_cache();
        sink_u32 = layer.cursor_column();
    }

    [[gnu::noinline]]
    void draw_full_screen_fill_3pbin() noexcept {
        clear_tiles();
        auto layer = make_layer_for(config_3pbin);
        for (int row = 0; row < 20; ++row) {
            const int y = row * static_cast<int>(font.line_height());
            for (int col = 0; col < 30; ++col) {
                const int x = col * static_cast<int>(font.font_width);
                layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)),
                                x, y + font.ascent);
            }
        }
        layer.flush_cache();
        sink_u32 = layer.cursor_row();
    }

} // namespace

int main() {
    const auto log_row = [](const char* case_name, unsigned int single, unsigned int avg) {
        gba::benchmark::log(gba::log::level::info, "  {case:<36}  {single:>6}  {avg:>6}"_fmt,
                            "case"_arg = case_name, "single"_arg = single, "avg"_arg = avg);
    };

    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;
    gba::reg_dispcnt = {.video_mode = 0, .enable_bg0 = true};
    gba::reg_bgcnt[0] = {.screenblock = 31};

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== text render benchmark (cycles) ===");
        gba::benchmark::log(gba::log::level::info, "  BDF font: {glyphs} glyphs"_fmt,
                            "glyphs"_arg = font.glyph_count);
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info,
                            "  {case:<36}  {single:>6}  {avg:>6}"_fmt,
                            "case"_arg = "Case", "single"_arg = "single", "avg"_arg = "avg");
    });

    {
        auto single = gba::benchmark::measure(draw_single_char);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char);
        gba::benchmark::with_logger([&] { log_row("single put_char ('H')", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_single_char_shadow);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char_shadow);
        gba::benchmark::with_logger([&] { log_row("single put_char shadow ('H')", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_cstr_short);
        auto avg = gba::benchmark::measure_avg(iters, draw_cstr_short);
        gba::benchmark::with_logger([&] { log_row("cstr short (\"HP: 42/99\")", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_cstr_color_escape);
        auto avg = gba::benchmark::measure_avg(iters, draw_cstr_color_escape);
        gba::benchmark::with_logger([&] { log_row("cstr palette escapes", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_format_string);
        auto avg = gba::benchmark::measure_avg(iters, draw_format_string);
        gba::benchmark::with_logger([&] { log_row("format -> put_string", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_format_with_palette);
        auto avg = gba::benchmark::measure_avg(iters, draw_format_with_palette);
        gba::benchmark::with_logger([&] { log_row("format + {pal:pal}", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_full_screen_fill);
        auto avg = gba::benchmark::measure_avg(iters, draw_full_screen_fill);
        gba::benchmark::with_logger([&] { log_row("full-screen fill (600 glyphs)", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_full_screen_fill_shadow);
        auto avg = gba::benchmark::measure_avg(iters, draw_full_screen_fill_shadow);
        gba::benchmark::with_logger([&] { log_row("full-screen fill shadow (600 glyphs)", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_cursor_literal_wrap);
        auto avg = gba::benchmark::measure_avg(iters, draw_cursor_literal_wrap);
        gba::benchmark::with_logger([&] { log_row("cursor wrap literal runs", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_cursor_mixed_wrap);
        auto avg = gba::benchmark::measure_avg(iters, draw_cursor_mixed_wrap);
        gba::benchmark::with_logger([&] { log_row("cursor wrap mixed runs", single, avg); });
    }

    unsigned int layer_init_avg = 0;
    {
        auto single = gba::benchmark::measure(init_layer_only);
        auto avg = gba::benchmark::measure_avg(iters, init_layer_only);
        layer_init_avg = avg;
        gba::benchmark::with_logger([&] { log_row("layer init only", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(init_layer_first_draw);
        auto avg = gba::benchmark::measure_avg(iters, init_layer_first_draw);
        gba::benchmark::with_logger([&] { log_row("layer init+1st draw", single, avg); });
        const auto delta = (avg >= layer_init_avg) ? (avg - layer_init_avg) : 0u;
        gba::benchmark::with_logger([&] { log_row("layer 1st-draw delta", 0u, delta); });
    }

    unsigned int cursor_init_cstr_avg = 0;
    {
        auto single = gba::benchmark::measure(init_cursor_cstr);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_cstr);
        cursor_init_cstr_avg = avg;
        gba::benchmark::with_logger([&] { log_row("cursor init cstr", single, avg); });
    }

    unsigned int cursor_init_literal_avg = 0;
    {
        auto single = gba::benchmark::measure(init_cursor_literal_stream);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_literal_stream);
        cursor_init_literal_avg = avg;
        gba::benchmark::with_logger([&] { log_row("cursor init literal stream", single, avg); });
    }

    unsigned int cursor_init_mixed_avg = 0;
    {
        auto single = gba::benchmark::measure(init_cursor_mixed_stream);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_mixed_stream);
        cursor_init_mixed_avg = avg;
        gba::benchmark::with_logger([&] { log_row("cursor init mixed stream", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_cstr_first_step);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_cstr_first_step);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st step cstr", single, avg); });
        const auto delta = (avg >= cursor_init_cstr_avg) ? (avg - cursor_init_cstr_avg) : 0u;
        gba::benchmark::with_logger([&] { log_row("cursor 1st-step delta cstr", 0u, delta); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_literal_stream_first_step);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_literal_stream_first_step);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st step literal", single, avg); });
        const auto delta = (avg >= cursor_init_literal_avg) ? (avg - cursor_init_literal_avg) : 0u;
        gba::benchmark::with_logger([&] { log_row("cursor 1st-step delta literal", 0u, delta); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_mixed_stream_first_step);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_mixed_stream_first_step);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st step mixed", single, avg); });
        const auto delta = (avg >= cursor_init_mixed_avg) ? (avg - cursor_init_mixed_avg) : 0u;
        gba::benchmark::with_logger([&] { log_row("cursor 1st-step delta mixed", 0u, delta); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_cstr_first_visible);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_cstr_first_visible);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st visible cstr", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_literal_stream_first_visible);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_literal_stream_first_visible);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st visible literal", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(init_cursor_mixed_stream_first_visible);
        auto avg = gba::benchmark::measure_avg(iters, init_cursor_mixed_stream_first_visible);
        gba::benchmark::with_logger([&] { log_row("cursor init+1st visible mixed", single, avg); });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "--- two_plane_three_color profile ---");
    });

    {
        auto single = gba::benchmark::measure(draw_single_char_2p3c);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char_2p3c);
        gba::benchmark::with_logger([&] { log_row("2p3c single put_char ('H')", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_full_screen_fill_2p3c);
        auto avg = gba::benchmark::measure_avg(iters, draw_full_screen_fill_2p3c);
        gba::benchmark::with_logger([&] { log_row("2p3c full-screen fill (600 glyphs)", single, avg); });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "--- two_plane_binary profile ---");
    });

    {
        auto single = gba::benchmark::measure(draw_single_char_2pbin);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char_2pbin);
        gba::benchmark::with_logger([&] { log_row("2pbin single put_char ('H')", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_full_screen_fill_2pbin);
        auto avg = gba::benchmark::measure_avg(iters, draw_full_screen_fill_2pbin);
        gba::benchmark::with_logger([&] { log_row("2pbin full-screen fill (600 glyphs)", single, avg); });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "--- three_plane_binary profile ---");
    });

    {
        auto single = gba::benchmark::measure(draw_single_char_3pbin);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char_3pbin);
        gba::benchmark::with_logger([&] { log_row("3pbin single put_char ('H')", single, avg); });
    }

    {
        auto single = gba::benchmark::measure(draw_full_screen_fill_3pbin);
        auto avg = gba::benchmark::measure_avg(iters, draw_full_screen_fill_3pbin);
        gba::benchmark::with_logger([&] { log_row("3pbin full-screen fill (600 glyphs)", single, avg); });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
