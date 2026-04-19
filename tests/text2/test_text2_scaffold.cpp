/// @file tests/text2/test_text2_scaffold.cpp
/// @brief Verify text2 engine scaffold compiles.

#include <gba/embed>
#include <gba/testing>
#include <gba/text2>

int main() {
    // Test that the basic types exist and are accessible
    using config_t = gba::text2::bitplane_config;
    using profile_t = gba::text2::bitplane_profile;
    using role_t = gba::text2::bitplane_role;
    using theme_t = gba::text2::bitplane_theme;
    using metrics_t = gba::text2::stream_metrics;

    // Test allocator
    gba::text2::linear_tile_allocator alloc;
    auto tile_idx = alloc.allocate();
    gba::test.eq(tile_idx, 0u);

    auto tile_idx2 = alloc.allocate();
    gba::test.eq(tile_idx2, 1u);

    // Test profile accessors
    constexpr auto profile = gba::text2::bitplane_profile::two_plane_three_color;
    constexpr auto plane_count = gba::text2::profile_plane_count(profile);
    gba::test.eq(plane_count, 2u);

    constexpr auto role_count = gba::text2::profile_role_count(profile);
    gba::test.eq(role_count, 3u);

    // Test bitplane_config
    config_t cfg{};
    cfg.profile = profile;
    cfg.palbank_0 = 1;
    cfg.palbank_1 = 2;
    gba::test.eq(cfg.plane_count(), 2u);
    gba::test.eq(cfg.role_count(), 3u);

    // Test that text2_format is accessible
    using fmt_t = gba::text2::text2_format<"Test: {x}">;
    static_assert(true, "text2_format type is accessible");

    // Test: compile-time reveal run metadata
    {
        using literal_runs = gba::text2::compiled_reveal_runs<"Hello">;
        static_assert(literal_runs::run_count == 1);
        static_assert(literal_runs::literal_run_count == 1);
        static_assert(literal_runs::runtime_run_count == 0);
        static_assert(literal_runs::literal_only);
        static_assert(literal_runs::runs[0].kind == gba::text2::glyph_run_kind::literal);
        gba::test.eq(literal_runs::runs[0].lit_length, 5u);

        using mixed_runs = gba::text2::compiled_reveal_runs<"Foo {} Bar">;
        static_assert(mixed_runs::run_count == 3);
        static_assert(mixed_runs::literal_run_count == 2);
        static_assert(mixed_runs::runtime_run_count == 1);
        static_assert(mixed_runs::has_runtime_runs);
        static_assert(mixed_runs::runs[0].kind == gba::text2::glyph_run_kind::literal);
        static_assert(mixed_runs::runs[1].kind == gba::text2::glyph_run_kind::runtime);
        static_assert(mixed_runs::runs[2].kind == gba::text2::glyph_run_kind::literal);
        gba::test.eq(mixed_runs::runs[0].lit_length, 4u);
        gba::test.eq(mixed_runs::runs[2].lit_length, 4u);

        using pal_runs = gba::text2::compiled_reveal_runs<"A{p:pal}B">;
        static_assert(pal_runs::run_count == 3);
        static_assert(pal_runs::runtime_run_count == 1);
        static_assert(pal_runs::runs[1].kind == gba::text2::glyph_run_kind::runtime);
        gba::test.eq(pal_runs::runs[0].lit_length, 1u);
        gba::test.eq(pal_runs::runs[2].lit_length, 1u);

        using escaped_runs = decltype(gba::text2::make_reveal_runs<"{{A}}">());
        static_assert(escaped_runs::run_count == 2);
        static_assert(escaped_runs::literal_run_count == 2);
        static_assert(escaped_runs::runtime_run_count == 0);
        static_assert(escaped_runs::literal_only);
    }

    return gba::test.finish();
}
