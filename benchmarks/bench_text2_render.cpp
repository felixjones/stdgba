/// @file benchmarks/bench_text2_render.cpp
/// @brief Benchmark measuring text2 rendering cost with a real embedded font.

#include <gba/benchmark>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/text2>
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
static constexpr auto fmt_hp = gba::text2::text2_format<"HP: {hp}/{max}">{ };
static constexpr auto fmt_hp_color = gba::text2::text2_format<"{pal:pal}HP: {hp}/{max}">{ };

volatile unsigned int sink_u32 = 0;

namespace {
    using layer_type = gba::text2::bg4bpp_text_layer<decltype(font), 240, 160>;

    constexpr int iters = 64;
    constexpr auto config = gba::text2::bitplane_config{
        .profile = gba::text2::bitplane_profile::one_plane_full_color,
        .palbank_0 = 0,
        .start_index = 1,
    };

    [[nodiscard]]
    layer_type make_layer() noexcept {
        return layer_type{&font, config};
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
        gba::benchmark::log(gba::log::level::info, "=== text2 render benchmark (cycles) ===");
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

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
