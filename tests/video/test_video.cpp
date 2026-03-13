/// @file test_video.cpp
/// @brief Tests for video types: color, _clr literal, object, screen_entry.

#include <gba/testing>
#include <gba/video>

#include <bit>
#include <cstdint>

int main() {
    // color struct layout
    {
        static_assert(sizeof(gba::color) == 2);
        static_assert(alignof(gba::color) == 2);
    }

    // color: default zero-initialized
    {
        constexpr gba::color black = {};
        auto raw = __builtin_bit_cast(unsigned short, black);
        gba::test.eq(raw, 0);
    }

    // color: red channel only
    {
        constexpr gba::color red = {.red = 31};
        auto raw = __builtin_bit_cast(unsigned short, red);
        gba::test.eq(raw, 31); // bits 0-4
    }

    // color: green channel only
    {
        constexpr gba::color green = {.green = 31};
        auto raw = __builtin_bit_cast(unsigned short, green);
        gba::test.eq(raw, 31 << 5); // bits 5-9
    }

    // color: blue channel only
    {
        constexpr gba::color blue = {.blue = 31};
        auto raw = __builtin_bit_cast(unsigned short, blue);
        gba::test.eq(raw, 31 << 10); // bits 10-14
    }

    // color: white (all channels max, no grn_lo)
    {
        constexpr gba::color white = {.red = 31, .green = 31, .blue = 31};
        auto raw = __builtin_bit_cast(unsigned short, white);
        gba::test.eq(raw, 0x7FFF);
    }

    // color: grn_lo bit at position 15
    {
        constexpr gba::color g_lo = {.grn_lo = 1};
        auto raw = __builtin_bit_cast(unsigned short, g_lo);
        gba::test.eq(raw, 0x8000u);
    }

    // color: full green (5-bit + grn_lo)
    {
        constexpr gba::color bright_green = {.green = 31, .grn_lo = 1};
        auto raw = __builtin_bit_cast(unsigned short, bright_green);
        gba::test.eq(raw, static_cast<unsigned short>((31 << 5) | 0x8000));
    }

    // _clr literal: pure red
    {
        using namespace gba;
        constexpr auto red = "#FF0000"_clr;
        gba::test.eq(red.red, 31u);
        gba::test.eq(red.green, 0u);
        gba::test.eq(red.blue, 0u);
    }

    // _clr literal: pure green (0xFF >> 2 & 1 = 1, so grn_lo is set)
    {
        using namespace gba;
        constexpr auto green = "#00FF00"_clr;
        gba::test.eq(green.red, 0u);
        gba::test.eq(green.green, 31u);
        gba::test.eq(green.blue, 0u);
        gba::test.eq(green.grn_lo, 1u);
    }

    // _clr literal: pure blue
    {
        using namespace gba;
        constexpr auto blue = "#0000FF"_clr;
        gba::test.eq(blue.red, 0u);
        gba::test.eq(blue.green, 0u);
        gba::test.eq(blue.blue, 31u);
        gba::test.eq(blue.grn_lo, 0u);
    }

    // _clr literal: white (0xFF >> 2 & 1 = 1, so grn_lo is set)
    {
        using namespace gba;
        constexpr auto white = "#FFFFFF"_clr;
        gba::test.eq(white.red, 31u);
        gba::test.eq(white.green, 31u);
        gba::test.eq(white.blue, 31u);
        gba::test.eq(white.grn_lo, 1u);
    }

    // _clr literal: black
    {
        using namespace gba;
        constexpr auto black = "#000000"_clr;
        gba::test.eq(black.red, 0u);
        gba::test.eq(black.green, 0u);
        gba::test.eq(black.blue, 0u);
        gba::test.eq(black.grn_lo, 0u);
    }

    // _clr literal: mid-gray (0x80 >> 3 = 16, 0x80 >> 2 & 1 = 0)
    {
        using namespace gba;
        constexpr auto gray = "#808080"_clr;
        gba::test.eq(gray.red, 16u);
        gba::test.eq(gray.green, 16u);
        gba::test.eq(gray.blue, 16u);
        gba::test.eq(gray.grn_lo, 0u);
    }

    // _clr literal: lowercase hex
    {
        using namespace gba;
        constexpr auto c = "#ff8040"_clr;
        gba::test.eq(c.red, 31u);   // 0xFF >> 3 = 31
        gba::test.eq(c.green, 16u); // 0x80 >> 3 = 16
        gba::test.eq(c.blue, 8u);   // 0x40 >> 3 = 8
    }

    // object struct layout
    { static_assert(sizeof(gba::object) == 6); }

    // object: default fields
    {
        constexpr gba::object obj = {};
        gba::test.eq(obj.y, 0u);
        gba::test.eq(obj.x, 0u);
        gba::test.eq(obj.tile_index, 0u);
        gba::test.is_false(obj.disable);
        gba::test.is_false(obj.mosaic);
        gba::test.is_false(obj.flip_x);
        gba::test.is_false(obj.flip_y);
    }

    // object: set position and tile
    {
        constexpr gba::object obj = {.y = 80, .x = 120, .tile_index = 42};
        gba::test.eq(obj.y, 80u);
        gba::test.eq(obj.x, 120u);
        gba::test.eq(obj.tile_index, 42u);
    }

    // object: modes and depth
    // Note: enum bitfields with values that set the sign bit of the bitfield
    // width undergo sign-extension on readback. Cast to unsigned for comparison.
    {
        constexpr gba::object obj = {
            .mode = gba::mode_blend, .depth = gba::depth_8bpp, .shape = gba::shape_wide, .size = 2};
        gba::test.eq(static_cast<unsigned>(obj.mode), static_cast<unsigned>(gba::mode_blend));
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, static_cast<unsigned>(gba::depth_8bpp));
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_wide));
        gba::test.eq(obj.size, 2u);
    }

    // object_affine struct layout
    { static_assert(sizeof(gba::object_affine) == 6); }

    // object_affine: default has affine=true
    {
        constexpr gba::object_affine obj = {};
        gba::test.is_true(obj.affine);
    }

    // object_affine: set affine index
    {
        constexpr gba::object_affine obj = {.y = 50, .x = 100, .affine_index = 3, .tile_index = 10};
        gba::test.eq(obj.y, 50u);
        gba::test.eq(obj.x, 100u);
        gba::test.eq(obj.affine_index, 3u);
        gba::test.eq(obj.tile_index, 10u);
        gba::test.is_true(obj.affine);
    }

    // screen_entry struct layout
    { static_assert(sizeof(gba::screen_entry) == 2); }

    // screen_entry fields
    {
        constexpr gba::screen_entry se = {
            .tile_index = 100, .flip_horizontal = true, .flip_vertical = false, .palette_index = 5};
        gba::test.eq(se.tile_index, 100u);
        gba::test.is_true(se.flip_horizontal);
        gba::test.is_false(se.flip_vertical);
        gba::test.eq(se.palette_index, 5u);
    }

    // tile types
    {
        static_assert(sizeof(gba::tile4bpp) == 32);
        static_assert(sizeof(gba::tile8bpp) == 64);
    }

    // mode/depth/shape enums
    {
        static_assert(gba::mode_normal == 0);
        static_assert(gba::mode_blend == 1);
        static_assert(gba::mode_window == 2);
        static_assert(gba::depth_4bpp == 0);
        static_assert(gba::depth_8bpp == 1);
        static_assert(gba::shape_square == 0);
        static_assert(gba::shape_wide == 1);
        static_assert(gba::shape_tall == 2);
    }
    return gba::test.finish();
}
