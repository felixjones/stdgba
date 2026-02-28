/// @file test_video.cpp
/// @brief Tests for video types: color, _clr literal, object, screen_entry.

#include <gba/video>

#include <mgba_test.hpp>

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
        ASSERT_EQ(raw, 0);
    }

    // color: red channel only
    {
        constexpr gba::color red = {.red = 31};
        auto raw = __builtin_bit_cast(unsigned short, red);
        ASSERT_EQ(raw, 31); // bits 0-4
    }

    // color: green channel only
    {
        constexpr gba::color green = {.green = 31};
        auto raw = __builtin_bit_cast(unsigned short, green);
        ASSERT_EQ(raw, 31 << 5); // bits 5-9
    }

    // color: blue channel only
    {
        constexpr gba::color blue = {.blue = 31};
        auto raw = __builtin_bit_cast(unsigned short, blue);
        ASSERT_EQ(raw, 31 << 10); // bits 10-14
    }

    // color: white (all channels max, no green_lo)
    {
        constexpr gba::color white = {.red = 31, .green = 31, .blue = 31};
        auto raw = __builtin_bit_cast(unsigned short, white);
        ASSERT_EQ(raw, 0x7FFF);
    }

    // color: green_lo bit at position 15
    {
        constexpr gba::color g_lo = {.green_lo = 1};
        auto raw = __builtin_bit_cast(unsigned short, g_lo);
        ASSERT_EQ(raw, 0x8000u);
    }

    // color: full green (5-bit high + low bit)
    {
        constexpr gba::color bright_green = {.green = 31, .green_lo = 1};
        auto raw = __builtin_bit_cast(unsigned short, bright_green);
        ASSERT_EQ(raw, static_cast<unsigned short>((31 << 5) | 0x8000));
    }

    // _clr literal: pure red
    {
        using namespace gba;
        constexpr auto red = "#FF0000"_clr;
        ASSERT_EQ(red.red, 31u);
        ASSERT_EQ(red.green, 0u);
        ASSERT_EQ(red.blue, 0u);
    }

    // _clr literal: pure green
    {
        using namespace gba;
        constexpr auto green = "#00FF00"_clr;
        ASSERT_EQ(green.red, 0u);
        ASSERT_EQ(green.green, 31u);
        ASSERT_EQ(green.blue, 0u);
    }

    // _clr literal: pure blue
    {
        using namespace gba;
        constexpr auto blue = "#0000FF"_clr;
        ASSERT_EQ(blue.red, 0u);
        ASSERT_EQ(blue.green, 0u);
        ASSERT_EQ(blue.blue, 31u);
    }

    // _clr literal: white
    {
        using namespace gba;
        constexpr auto white = "#FFFFFF"_clr;
        ASSERT_EQ(white.red, 31u);
        ASSERT_EQ(white.green, 31u);
        ASSERT_EQ(white.blue, 31u);
    }

    // _clr literal: black
    {
        using namespace gba;
        constexpr auto black = "#000000"_clr;
        ASSERT_EQ(black.red, 0u);
        ASSERT_EQ(black.green, 0u);
        ASSERT_EQ(black.blue, 0u);
    }

    // _clr literal: mid-gray (0x80 >> 3 = 16)
    {
        using namespace gba;
        constexpr auto gray = "#808080"_clr;
        ASSERT_EQ(gray.red, 16u);
        ASSERT_EQ(gray.green, 16u);
        ASSERT_EQ(gray.blue, 16u);
    }

    // _clr literal: lowercase hex
    {
        using namespace gba;
        constexpr auto c = "#ff8040"_clr;
        ASSERT_EQ(c.red, 31u);        // 0xFF >> 3 = 31
        ASSERT_EQ(c.green, 16u);      // 0x80 >> 3 = 16
        ASSERT_EQ(c.blue, 8u);        // 0x40 >> 3 = 8
    }

    // object struct layout
    {
        static_assert(sizeof(gba::object) == 6);
    }

    // object: default fields
    {
        constexpr gba::object obj = {};
        ASSERT_EQ(obj.y, 0u);
        ASSERT_EQ(obj.x, 0u);
        ASSERT_EQ(obj.tile_index, 0u);
        ASSERT_FALSE(obj.disable);
        ASSERT_FALSE(obj.mosaic);
        ASSERT_FALSE(obj.flip_x);
        ASSERT_FALSE(obj.flip_y);
    }

    // object: set position and tile
    {
        constexpr gba::object obj = {.y = 80, .x = 120, .tile_index = 42};
        ASSERT_EQ(obj.y, 80u);
        ASSERT_EQ(obj.x, 120u);
        ASSERT_EQ(obj.tile_index, 42u);
    }

    // object: modes and depth
    // Note: enum bitfields with values that set the sign bit of the bitfield
    // width undergo sign-extension on readback. Cast to unsigned for comparison.
    {
        constexpr gba::object obj = {
            .mode = gba::mode_blend,
            .depth = gba::depth_8bpp,
            .shape = gba::shape_wide,
            .size = 2
        };
        ASSERT_EQ(static_cast<unsigned>(obj.mode), static_cast<unsigned>(gba::mode_blend));
        ASSERT_EQ(static_cast<unsigned>(obj.depth) & 1, static_cast<unsigned>(gba::depth_8bpp));
        ASSERT_EQ(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_wide));
        ASSERT_EQ(obj.size, 2u);
    }

    // object_affine struct layout
    {
        static_assert(sizeof(gba::object_affine) == 6);
    }

    // object_affine: default has affine=true
    {
        constexpr gba::object_affine obj = {};
        ASSERT_TRUE(obj.affine);
    }

    // object_affine: set affine index
    {
        constexpr gba::object_affine obj = {.y = 50, .x = 100, .affine_index = 3, .tile_index = 10};
        ASSERT_EQ(obj.y, 50u);
        ASSERT_EQ(obj.x, 100u);
        ASSERT_EQ(obj.affine_index, 3u);
        ASSERT_EQ(obj.tile_index, 10u);
        ASSERT_TRUE(obj.affine);
    }

    // screen_entry struct layout
    {
        static_assert(sizeof(gba::screen_entry) == 2);
    }

    // screen_entry fields
    {
        constexpr gba::screen_entry se = {
            .tile_index = 100,
            .flip_horizontal = true,
            .flip_vertical = false,
            .palette_index = 5
        };
        ASSERT_EQ(se.tile_index, 100u);
        ASSERT_TRUE(se.flip_horizontal);
        ASSERT_FALSE(se.flip_vertical);
        ASSERT_EQ(se.palette_index, 5u);
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
}
