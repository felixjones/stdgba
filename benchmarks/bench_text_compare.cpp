/// @file benchmarks/bench_text_compare.cpp
/// @brief Benchmark comparing legacy gba::text against gba::text2 with matched inputs.

#include <gba/benchmark>
#include <gba/embed>
#include <gba/format>
#include <gba/interrupt>
#include <gba/text>
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
CHARS 9
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
STARTCHAR 0
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
STARTCHAR 1
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
STARTCHAR 2
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
STARTCHAR 3
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
ENDFONT
)";

    std::array<unsigned char, sizeof(source) - 1> data{};
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(source[i]);
    return data;
}

static constexpr auto font_base = gba::embed::bdf([] { return make_bench_bdf(); });
static constexpr auto font_with_shadow = gba::text::with_shadow<1, 1>(font_base);
static constexpr auto font_with_outline = gba::text::with_outline<1>(font_base);
static constexpr auto fmt_hp = "HP: {hp}/{max}"_fmt;
static constexpr auto fmt_hp_text2 = gba::text2::text2_format<"HP: {hp}/{max}">{ };

volatile unsigned int sink_u32 = 0;

namespace {
    constexpr int iters = 32;

    enum class test_profile {
        two_plane_binary,
        two_plane_three_color,
        three_plane_binary,
        one_plane_full_color,
    };

    constexpr auto text_draw_metrics = gba::text::draw_metrics{};
    constexpr auto text_stream_metrics = gba::text::stream_metrics{};
    constexpr auto text2_stream_metrics = gba::text2::stream_metrics{};

    struct profile_config {
        gba::text::bitplane_config text;
        gba::text2::bitplane_config text2;
        const char* name;
    };

    constexpr profile_config get_profile_config(test_profile prof) noexcept {
        switch (prof) {
            case test_profile::two_plane_binary:
                return {
                    {.profile = gba::text::bitplane_profile::two_plane_binary, .palbank_0 = 1, .palbank_1 = 2, .start_index = 1},
                    {.profile = gba::text2::bitplane_profile::two_plane_binary, .palbank_0 = 1, .palbank_1 = 2, .start_index = 1},
                    "two_plane_binary"
                };
            case test_profile::two_plane_three_color:
                return {
                    {.profile = gba::text::bitplane_profile::two_plane_three_color, .palbank_0 = 1, .palbank_1 = 2, .start_index = 1},
                    {.profile = gba::text2::bitplane_profile::two_plane_three_color, .palbank_0 = 1, .palbank_1 = 2, .start_index = 1},
                    "two_plane_three_color"
                };
            case test_profile::three_plane_binary:
                return {
                    {.profile = gba::text::bitplane_profile::three_plane_binary, .palbank_0 = 1, .palbank_1 = 2, .palbank_2 = 3, .start_index = 1},
                    {.profile = gba::text2::bitplane_profile::three_plane_binary, .palbank_0 = 1, .palbank_1 = 2, .palbank_2 = 3, .start_index = 1},
                    "three_plane_binary"
                };
            case test_profile::one_plane_full_color:
                return {
                    {.profile = gba::text::bitplane_profile::one_plane_full_color, .palbank_0 = 1, .start_index = 0},
                    {.profile = gba::text2::bitplane_profile::one_plane_full_color, .palbank_0 = 1, .start_index = 0},
                    "one_plane_full_color"
                };
        }
        return {{}, {}, "unknown"};
    }

    void clear_tiles() noexcept {
        auto* tiles = gba::memory_map(gba::registral_cast<gba::tile4bpp[2048]>(gba::mem_tile_4bpp));
        for (int i = 0; i < 600; ++i) {
            tiles[i] = {};
        }
    }

    [[nodiscard]]
    gba::text2::bg4bpp_text_layer<32, 32> make_text2_layer_32(const gba::text2::bitplane_config& cfg,
                                                               gba::text2::linear_tile_allocator alloc) noexcept {
        static gba::text2::bg4bpp_text_layer<32, 32>::cell_state_map cell_state{};
        return gba::text2::bg4bpp_text_layer<32, 32>{31, cfg, alloc, cell_state};
    }

    template<test_profile Profile, typename Font>
    class bench_text_single_char {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            sink_u32 = layer.draw_char(font, 'H', 0, font.ascent);
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_single_char {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = layer.draw_char(font, 'H', 0, font.ascent);
            layer.flush_cache();
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_cstr_short {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto s = gba::text::stream("HP: 42/99", font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_cstr_short {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, "HP: 42/99", 0, 0, text2_stream_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_cstr_long {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto s = gba::text::stream("123 456 789 00 1", font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_cstr_long {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, "123 456 789 00 1", 0, 0, text2_stream_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_typewriter {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto s = gba::text::stream("HP: 42/99", font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics, 5));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_typewriter {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, "HP: 42/99", 0, 0, text2_stream_metrics, 5));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_format {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            volatile int hp = 42;
            volatile int mx = 99;
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto gen = fmt_hp.generator("hp"_arg = hp, "max"_arg = mx);
            auto s = gba::text::stream(gen, font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_format {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            volatile int hp = 42;
            volatile int mx = 99;
            char buffer[32]{};
            fmt_hp_text2.to(buffer, "hp"_arg = hp, "max"_arg = mx);
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, buffer, 0, 0, text2_stream_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_lazy_format {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            volatile int hp = 42;
            volatile int mx = 99;
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto gen = fmt_hp.generator("hp"_arg = [&] { return hp; }, "max"_arg = [&] { return mx; });
            auto s = gba::text::stream(gen, font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_lazy_format {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            volatile int hp = 42;
            volatile int mx = 99;
            char buffer[32]{};
            fmt_hp_text2.to(buffer, "hp"_arg = [&] { return hp; }, "max"_arg = [&] { return mx; });
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, buffer, 0, 0, text2_stream_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_color_escape {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            auto s = gba::text::stream("\x1B" "2HP: " "\x1B" "3" "42/99", font, text_stream_metrics);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, s, 0, 0, text_draw_metrics));
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text_fullscreen {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            gba::text::bg4_text_layer<32, 32, gba::text::linear_tile_allocator> layer{31, cfg.text, alloc};
            for (int row = 0; row < 20; ++row) {
                const int y = row * 8;
                for (int col = 0; col < 30; ++col) {
                    const int x = col * 6;
                    layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)), x, y + font.ascent);
                }
            }
            sink_u32 = 42;
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_fullscreen {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            for (int row = 0; row < 20; ++row) {
                const int y = row * 8;
                for (int col = 0; col < 30; ++col) {
                    const int x = col * 8;
                    layer.draw_char(font, static_cast<unsigned int>('0' + (col % 10)), x, y + font.ascent);
                }
            }
            layer.flush_cache();
            sink_u32 = 42;
        }
        const Font& font;
    };

    template<test_profile Profile, typename Font>
    class bench_text2_color_escape {
    public:
        [[gnu::noinline]]
        void operator()() const noexcept {
            clear_tiles();
            const auto cfg = get_profile_config(Profile);
            gba::text2::linear_tile_allocator alloc{.next_tile = 1, .end_tile = 512};
            auto layer = make_text2_layer_32(cfg.text2, alloc);
            sink_u32 = static_cast<unsigned int>(layer.draw_stream(font, "\x1B" "2HP: " "\x1B" "3" "42/99", 0, 0, text2_stream_metrics));
        }
        const Font& font;
    };

    template<template<test_profile, typename> class TextFn, template<test_profile, typename> class Text2Fn,
             typename Font>
    struct benchmark_dispatcher {
        test_profile prof;
        const Font& font;

        unsigned int measure_text() const {
            switch (prof) {
                case test_profile::two_plane_binary:
                    return gba::benchmark::measure_avg(iters, TextFn<test_profile::two_plane_binary, Font>{font});
                case test_profile::two_plane_three_color:
                    return gba::benchmark::measure_avg(iters, TextFn<test_profile::two_plane_three_color, Font>{font});
                case test_profile::three_plane_binary:
                    return gba::benchmark::measure_avg(iters, TextFn<test_profile::three_plane_binary, Font>{font});
                case test_profile::one_plane_full_color:
                    return gba::benchmark::measure_avg(iters, TextFn<test_profile::one_plane_full_color, Font>{font});
            }
            return 0u;
        }

        unsigned int measure_text2() const {
            switch (prof) {
                case test_profile::two_plane_binary:
                    return gba::benchmark::measure_avg(iters, Text2Fn<test_profile::two_plane_binary, Font>{font});
                case test_profile::two_plane_three_color:
                    return gba::benchmark::measure_avg(iters, Text2Fn<test_profile::two_plane_three_color, Font>{font});
                case test_profile::three_plane_binary:
                    return gba::benchmark::measure_avg(iters, Text2Fn<test_profile::three_plane_binary, Font>{font});
                case test_profile::one_plane_full_color:
                    return gba::benchmark::measure_avg(iters, Text2Fn<test_profile::one_plane_full_color, Font>{font});
            }
            return 0u;
        }
    };

    template<template<test_profile, typename> class TextFn,
             template<test_profile, typename> class Text2Fn,
             typename Font>
    void benchmark_case(test_profile profile, const char* label, const Font& font) {
        const auto disp = benchmark_dispatcher<TextFn, Text2Fn, Font>{profile, font};
        const auto text_time = disp.measure_text();
        const auto text2_time = disp.measure_text2();
        const int delta = static_cast<int>(text2_time) - static_cast<int>(text_time);
        const int bps = (text_time > 0)
                            ? static_cast<int>((10000LL * static_cast<long long>(delta)) /
                                               static_cast<long long>(text_time))
                            : 0;
        gba::benchmark::with_logger([label, text_time, text2_time, delta, bps] {
            gba::benchmark::log(
                gba::log::level::info,
                "    {label:<50}  {text:>7}  {text2:>7}  {delta:>7}  {bps:>7}"_fmt,
                "label"_arg = label,
                "text"_arg = text_time,
                "text2"_arg = text2_time,
                "delta"_arg = delta,
                "bps"_arg = bps);
        });
    }
}

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;
    gba::reg_dispcnt = {.video_mode = 0, .enable_bg0 = true};
    gba::reg_bgcnt[0] = {.screenblock = 31};

    constexpr auto theme = gba::text::bitplane_theme{
        .background = gba::color{},
        .foreground = "white"_clr,
        .shadow = "#102040"_clr,
    };

    for (int prof_idx = 0; prof_idx < 4; ++prof_idx) {
        const auto profile = static_cast<test_profile>(prof_idx);
        const auto cfg = get_profile_config(profile);
        gba::text::set_theme(cfg.text, theme);
        gba::text2::set_theme(cfg.text2, gba::text2::bitplane_theme{
                                             .background = theme.background,
                                             .foreground = theme.foreground,
                                             .shadow = theme.shadow,
                                         });
    }
    gba::pal_bg_mem[0] = theme.background;

    const auto log_row = [](const char* case_label, unsigned int text_cycles, unsigned int text2_cycles) {
        const int delta = static_cast<int>(text2_cycles) - static_cast<int>(text_cycles);
        const int bps = (text_cycles > 0)
                            ? static_cast<int>((10000LL * static_cast<long long>(delta)) /
                                               static_cast<long long>(text_cycles))
                            : 0;
        gba::benchmark::log(
            gba::log::level::info,
            "    {label:<50}  {text:>7}  {text2:>7}  {delta:>7}  {bps:>7}"_fmt,
            "label"_arg = case_label,
            "text"_arg = text_cycles,
            "text2"_arg = text2_cycles,
            "delta"_arg = delta,
            "bps"_arg = bps);
    };

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== text vs text2 comprehensive benchmark ===");
        gba::benchmark::log(gba::log::level::info, "");
    });

    static constexpr test_profile profiles[] = {
        test_profile::two_plane_binary,
        test_profile::two_plane_three_color,
        test_profile::three_plane_binary,
        test_profile::one_plane_full_color,
    };

    for (const auto profile : profiles) {
        const auto cfg = get_profile_config(profile);

        gba::benchmark::with_logger([cfg] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "  [{profile}]"_fmt, "profile"_arg = cfg.name);
            gba::benchmark::log(
                gba::log::level::info,
                "    {label:<50}  {text:>7}  {text2:>7}  {delta:>7}  {bps:>7}"_fmt,
                "label"_arg = "Case",
                "text"_arg = "text",
                "text2"_arg = "text2",
                "delta"_arg = "delta",
                "bps"_arg = "bp");
        });

        benchmark_case<bench_text_single_char, bench_text2_single_char>(profile, "single char ('H', plain)", font_base);
        benchmark_case<bench_text_cstr_short, bench_text2_cstr_short>(profile, "cstr short (\"HP: 42/99\")", font_base);
        benchmark_case<bench_text_cstr_long, bench_text2_cstr_long>(profile, "cstr long (\"123 456 789 00 1\", wrap)", font_base);
        benchmark_case<bench_text_fullscreen, bench_text2_fullscreen>(profile, "fullscreen fill (600 glyphs)", font_base);
        benchmark_case<bench_text_single_char, bench_text2_single_char>(profile, "single char ('H', shadow)", font_with_shadow);
        benchmark_case<bench_text_cstr_short, bench_text2_cstr_short>(profile, "cstr short (\"HP: 42/99\", shadow)", font_with_shadow);
        benchmark_case<bench_text_cstr_long, bench_text2_cstr_long>(profile, "cstr long (\"123 456 789 00 1\", shadow)", font_with_shadow);
        benchmark_case<bench_text_fullscreen, bench_text2_fullscreen>(profile, "fullscreen fill (600 glyphs, shadow)", font_with_shadow);
        benchmark_case<bench_text_single_char, bench_text2_single_char>(profile, "single char ('H', outline)", font_with_outline);
        benchmark_case<bench_text_cstr_short, bench_text2_cstr_short>(profile, "cstr short (\"HP: 42/99\", outline)", font_with_outline);
        benchmark_case<bench_text_cstr_long, bench_text2_cstr_long>(profile, "cstr long (\"123 456 789 00 1\", outline)", font_with_outline);
        benchmark_case<bench_text_fullscreen, bench_text2_fullscreen>(profile, "fullscreen fill (600 glyphs, outline)", font_with_outline);
        benchmark_case<bench_text_typewriter, bench_text2_typewriter>(profile, "typewriter reveal (5/9)", font_base);
        benchmark_case<bench_text_format, bench_text2_format>(profile, "format stream (HP template)", font_base);
        benchmark_case<bench_text_lazy_format, bench_text2_lazy_format>(profile, "lazy format suppliers", font_base);
        benchmark_case<bench_text_color_escape, bench_text2_color_escape>(profile, "inline color escapes", font_base);
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
