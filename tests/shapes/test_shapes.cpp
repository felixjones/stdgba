/// @file test_shapes.cpp
/// @brief Tests for consteval sprite shape generation.

#include <gba/shapes>
#include <gba/testing>

#include <cstdint>

using namespace gba::shapes;

// Helper: read a 4bpp pixel from tile-ordered data
static constexpr int read_pixel(const std::uint8_t* data, int W, int x, int y) {
    int tile_col = x / 8;
    int tile_row = y / 8;
    int tile_idx = tile_row * (W / 8) + tile_col;
    int local_x = x % 8;
    int local_y = y % 8;
    int byte_idx = tile_idx * 32 + local_y * 4 + local_x / 2;
    if (local_x % 2 == 0) return data[byte_idx] & 0x0F;
    else return (data[byte_idx] >> 4) & 0x0F;
}

int main() {
    // 8x8 rect: fill entire sprite
    {
        constexpr auto s = sprite_8x8(rect(0, 0, 8, 8));
        static_assert(s.size() == 32);
        static_assert(s.tile_count == 1);

        // All pixels should be palette index 1 (first group)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) gba::test.eq(read_pixel(s.data(), 8, x, y), 1);
    }

    // 8x8 empty: all transparent
    {
        constexpr auto s = sprite_8x8();
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) gba::test.eq(read_pixel(s.data(), 8, x, y), 0);
    }

    // 8x8 partial rect: check inside and outside
    {
        constexpr auto s = sprite_8x8(rect(2, 2, 4, 4));
        // Inside rect (2,2)-(5,5) should be palette 1
        gba::test.eq(read_pixel(s.data(), 8, 3, 3), 1);
        // Outside rect should be 0
        gba::test.eq(read_pixel(s.data(), 8, 0, 0), 0);
        gba::test.eq(read_pixel(s.data(), 8, 7, 7), 0);
    }

    // 16x16 sprite: multiple groups get different palette indices
    {
        constexpr auto s = sprite_16x16(rect(0, 0, 8, 8), // group 1, palette_idx = 1
                                        rect(8, 0, 8, 8)  // group 2, palette_idx = 2
        );
        static_assert(s.size() == 128);
        static_assert(s.tile_count == 4);

        gba::test.eq(read_pixel(s.data(), 16, 2, 2), 1);  // in group 1
        gba::test.eq(read_pixel(s.data(), 16, 10, 2), 2); // in group 2
    }

    // palette_idx override
    {
        constexpr auto s = sprite_8x8(palette_idx(5), rect(0, 0, 8, 8));
        gba::test.eq(read_pixel(s.data(), 8, 0, 0), 5);
    }

    // palette_idx(0) erases
    {
        constexpr auto s = sprite_8x8(rect(0, 0, 8, 8),                // palette_idx 1
                                      palette_idx(0), rect(2, 2, 4, 4) // erase inner rect
        );
        // Inside erased area should be 0
        gba::test.eq(read_pixel(s.data(), 8, 3, 3), 0);
        // Outside erased area still palette 1
        gba::test.eq(read_pixel(s.data(), 8, 0, 0), 1);
    }

    // group: empty reserves palette index
    {
        constexpr auto s = sprite_8x8(group(),         // palette_idx 1 (reserved, empty)
                                      rect(0, 0, 8, 8) // palette_idx 2
        );
        gba::test.eq(read_pixel(s.data(), 8, 0, 0), 2);
    }

    // obj() returns correct attributes for 8x8
    {
        constexpr auto s = sprite_8x8(rect(0, 0, 8, 8));
        constexpr auto obj = s.obj(42);
        gba::test.eq(obj.tile_index, 42u);
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_square));
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, static_cast<unsigned>(gba::depth_4bpp));
    }

    // obj() for 16x16
    {
        constexpr auto s = sprite_16x16(rect(0, 0, 16, 16));
        constexpr auto obj = s.obj(10);
        gba::test.eq(obj.tile_index, 10u);
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_square));
    }

    // obj_aff() has affine=true
    {
        constexpr auto s = sprite_8x8(rect(0, 0, 8, 8));
        constexpr auto obj = s.obj_aff(0);
        gba::test.is_true(obj.affine);
        gba::test.eq(static_cast<unsigned>(obj.depth) & 1, static_cast<unsigned>(gba::depth_4bpp));
    }

    // Wide sprite (32x16)
    {
        constexpr auto s = sprite_32x16(rect(0, 0, 32, 16));
        static_assert(s.size() == 256);
        static_assert(s.tile_count == 8);
        constexpr auto obj = s.obj();
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_wide));
    }

    // Tall sprite (16x32)
    {
        constexpr auto s = sprite_16x32(rect(0, 0, 16, 32));
        static_assert(s.size() == 256);
        static_assert(s.tile_count == 8);
        constexpr auto obj = s.obj();
        gba::test.eq(static_cast<unsigned>(obj.shape) & 3, static_cast<unsigned>(gba::shape_tall));
    }

    // circle
    {
        constexpr auto s = sprite_16x16(circle(8.0, 8.0, 4.0));
        // Center should be filled
        gba::test.eq(read_pixel(s.data(), 16, 8, 8), 1);
        // Far corners should be empty
        gba::test.eq(read_pixel(s.data(), 16, 0, 0), 0);
        gba::test.eq(read_pixel(s.data(), 16, 15, 15), 0);
    }

    // triangle
    {
        constexpr auto s = sprite_16x16(triangle(8, 0, 15, 15, 0, 15));
        // Bottom center should be inside
        gba::test.eq(read_pixel(s.data(), 16, 8, 14), 1);
        // Top corners should be outside
        gba::test.eq(read_pixel(s.data(), 16, 0, 0), 0);
        gba::test.eq(read_pixel(s.data(), 16, 15, 0), 0);
    }

    // rect_outline
    {
        constexpr auto s = sprite_16x16(rect_outline(2, 2, 12, 12, 1));
        // Edge should be filled
        gba::test.eq(read_pixel(s.data(), 16, 2, 2), 1);
        gba::test.eq(read_pixel(s.data(), 16, 13, 13), 1);
        // Interior should be empty
        gba::test.eq(read_pixel(s.data(), 16, 7, 7), 0);
        // Outside should be empty
        gba::test.eq(read_pixel(s.data(), 16, 0, 0), 0);
    }

    // line
    {
        constexpr auto s = sprite_8x8(line(0, 0, 7, 7));
        // Diagonal pixels should be filled
        gba::test.eq(read_pixel(s.data(), 8, 0, 0), 1);
        gba::test.eq(read_pixel(s.data(), 8, 3, 3), 1);
        gba::test.eq(read_pixel(s.data(), 8, 7, 7), 1);
    }

    // text
    {
        constexpr auto s = sprite_32x8(text(0, 0, "A"));
        // Letter A should produce some non-zero pixels in the top-left area
        bool found_pixel = false;
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 4; ++x)
                if (read_pixel(s.data(), 32, x, y) != 0) found_pixel = true;
        gba::test.is_true(found_pixel);
    }

    // oval_outline
    {
        constexpr auto s = sprite_16x16(oval_outline(2, 2, 12, 12, 1));
        // Edge should be filled
        gba::test.nz(read_pixel(s.data(), 16, 8, 2));
        // Center should be empty
        gba::test.eq(read_pixel(s.data(), 16, 8, 8), 0);
    }
    return gba::test.finish();
}
