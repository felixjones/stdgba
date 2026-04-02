/// @file benchmarks/bench_text_render.cpp
/// @brief Benchmark measuring BG4 text rendering pipeline cost.
///
/// Measures the full text rendering pipeline on GBA hardware:
///   1. draw_stream with a plain C-string stream (short text).
///   2. draw_stream with a plain C-string stream (long text with wrapping).
///   3. draw_stream with a format_generator stream (typewriter mode).
///   4. draw_stream with shadow enabled vs disabled.
///   5. draw_char for a single glyph.
///   6. Layer clear cost.
///
/// Uses an embedded BDF font compiled at consteval time.
/// Run in mgba-headless to see output.

#include <gba/bios>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/text>
#include <gba/video>

#include <array>
#include <cstddef>

#include <gba/benchmark>

using namespace gba::literals;

// -- Embedded BDF font (consteval) --------------------------------------------

static constexpr auto make_bench_bdf() {
    constexpr char source[] = R"(STARTFONT 2.1
FONT bench5x7
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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
DWIDTH 6 0
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

// -- Shared format strings ----------------------------------------------------

static constexpr auto fmt_hp = "HP: {hp}/{max}"_fmt;
static constexpr auto hp_arg = "hp"_arg;
static constexpr auto max_arg = "max"_arg;

// -- Draw metrics presets -----------------------------------------------------

static constexpr gba::text::draw_metrics default_metrics{
    .letter_spacing_px = 1,
    .line_spacing_px = 2,
    .wrap_width_px = 220,
    .break_chars = gba::text::break_policy::whitespace,
};

static constexpr gba::text::glyph_style style_no_shadow{
    .draw_shadow = false,
};

static constexpr gba::text::glyph_style style_shadow{
    .draw_shadow = true,
    .shadow_dx = 1,
    .shadow_dy = 1,
};

static constexpr gba::text::stream_metrics stream_cfg{
    .letter_spacing_px = 1,
};

// -- Volatile sinks -----------------------------------------------------------

volatile int sink_int = 0;

namespace {

    constexpr int iters = 64;

    // -- Benchmark helpers --------------------------------------------------------

    // Re-create the layer each call to measure the full allocation + draw cost.

    // 1. Short C-string, no shadow
    [[gnu::noinline]]
    void draw_cstr_short() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto s = gba::text::stream("HP: 42/99", font, stream_cfg);
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_no_shadow);
        sink_int = static_cast<int>(n);
    }

    // 2. Longer C-string with word wrapping
    [[gnu::noinline]]
    void draw_cstr_long() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto s = gba::text::stream("HP: 42/99 HP: 42/99 HP: 42/99", font, stream_cfg);
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_no_shadow);
        sink_int = static_cast<int>(n);
    }

    // 3. Format generator stream (typewriter path)
    [[gnu::noinline]]
    void draw_format_stream() {
        volatile int hp = 42;
        volatile int mx = 99;

        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto gen = fmt_hp.generator(hp_arg = hp, max_arg = mx);
        auto s = gba::text::stream(gen, font, stream_cfg);
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_no_shadow);
        sink_int = static_cast<int>(n);
    }

    // 4a. Short text WITHOUT shadow
    [[gnu::noinline]]
    void draw_no_shadow() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto s = gba::text::stream("HP: 42/99", font, stream_cfg);
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_no_shadow);
        sink_int = static_cast<int>(n);
    }

    // 4b. Short text WITH drop shadow
    [[gnu::noinline]]
    void draw_with_shadow() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto s = gba::text::stream("HP: 42/99", font, stream_cfg);
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_shadow);
        sink_int = static_cast<int>(n);
    }

    // 5. Single glyph via draw_char
    [[gnu::noinline]]
    void draw_single_char() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto advance = layer.draw_char(font, static_cast<unsigned int>('H'), 8, 24, style_no_shadow);
        sink_int = advance;
    }

    // 6. Layer clear only
    [[gnu::noinline]]
    void layer_clear_only() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};
        // Constructor already clears; measure via a second clear to isolate cost.
        layer.clear();
        sink_int = 0;
    }

    // 7. Typewriter reveal (max_chars limited)
    [[gnu::noinline]]
    void draw_typewriter_reveal() {
        gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
        constexpr auto config = gba::text::bitplane_config{
            .profile = gba::text::bitplane_profile::two_plane_three_color,
            .palbank_0 = 0,
            .palbank_1 = 1,
            .start_index = 1,
        };
        gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, config, alloc};

        auto s = gba::text::stream("HP: 42/99", font, stream_cfg);
        // Reveal only first 5 characters (simulates typewriter mid-animation)
        auto n = layer.draw_stream(font, s, 8, 16, default_metrics, style_shadow, 5);
        sink_int = static_cast<int>(n);
    }

} // namespace

int main() {
    const auto log_row = [](const char* caseName, unsigned int single, unsigned int avg) {
        gba::benchmark::log(gba::log::level::info, "  {case:<36}  {single:>6}  {avg:>6}"_fmt, "case"_arg = caseName,
                   "single"_arg = single, "avg"_arg = avg);
    };

    // Minimal HW setup so VRAM writes succeed
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;
    gba::reg_dispcnt = {.video_mode = 0, .enable_bg0 = true};
    gba::reg_bgcnt[0] = {.screenblock = 31};

    // Setup bitplane configuration
    constexpr auto config = gba::text::bitplane_config{
        .profile = gba::text::bitplane_profile::two_plane_three_color,
        .palbank_0 = 0,
        .palbank_1 = 1,
        .start_index = 1,
    };

    gba::text::set_theme(config, {
                                     .background = "#102040"_clr,
                                     .foreground = "white"_clr,
                                     .shadow = "#304060"_clr,
                                 });

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== text render benchmark (cycles) ===");
        gba::benchmark::log(gba::log::level::info, "  BDF font: {glyphs} glyphs"_fmt, "glyphs"_arg = font.glyph_count);
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "  {case:<36}  {single:>6}  {avg:>6}"_fmt, "case"_arg = "Case",
                   "single"_arg = "single", "avg"_arg = "avg");
    });

    // -- Short C-string, no shadow --------------------------------------------
    {
        auto single = gba::benchmark::measure(draw_cstr_short);
        auto avg = gba::benchmark::measure_avg(iters, draw_cstr_short);
        gba::benchmark::with_logger([&] { log_row("cstr short (9ch, no shadow)", single, avg); });
    }

    // -- Longer C-string with wrapping ----------------------------------------
    {
        auto single = gba::benchmark::measure(draw_cstr_long);
        auto avg = gba::benchmark::measure_avg(iters, draw_cstr_long);
        gba::benchmark::with_logger([&] { log_row("cstr long (30ch, wrap)", single, avg); });
    }

    // -- Format generator stream ----------------------------------------------
    {
        auto single = gba::benchmark::measure(draw_format_stream);
        auto avg = gba::benchmark::measure_avg(iters, draw_format_stream);
        gba::benchmark::with_logger([&] { log_row("format stream \"HP: {hp}/{max}\"", single, avg); });
    }

    // -- Shadow comparison ----------------------------------------------------
    {
        auto avg_no = gba::benchmark::measure_avg(iters, draw_no_shadow);
        auto avg_yes = gba::benchmark::measure_avg(iters, draw_with_shadow);
        gba::benchmark::with_logger([&] {
            log_row("shadow off (9ch)", 0u, avg_no);
            log_row("shadow on  (9ch)", 0u, avg_yes);
        });
    }

    // -- Single glyph ---------------------------------------------------------
    {
        auto single = gba::benchmark::measure(draw_single_char);
        auto avg = gba::benchmark::measure_avg(iters, draw_single_char);
        gba::benchmark::with_logger([&] { log_row("single draw_char ('H')", single, avg); });
    }

    // -- Layer clear ----------------------------------------------------------
    {
        auto single = gba::benchmark::measure(layer_clear_only);
        auto avg = gba::benchmark::measure_avg(iters, layer_clear_only);
        gba::benchmark::with_logger([&] { log_row("layer clear (32x32)", single, avg); });
    }

    // -- Typewriter reveal (partial) ------------------------------------------
    {
        auto single = gba::benchmark::measure(draw_typewriter_reveal);
        auto avg = gba::benchmark::measure_avg(iters, draw_typewriter_reveal);
        gba::benchmark::with_logger([&] { log_row("typewriter reveal (5/9ch, shadow)", single, avg); });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
