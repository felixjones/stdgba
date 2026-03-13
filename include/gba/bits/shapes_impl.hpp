/// @file bits/shapes_impl.hpp
/// @brief Implementation details for shapes API (internal header).
///
/// This is an internal header. Include <gba/shapes> instead.
#pragma once

#include <gba/sprite>
#include <gba/video>

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>

namespace gba::shapes {

    using u8 = std::uint8_t;

    /// @brief Oval (ellipse) shape command (bounding-box based).
    struct oval_cmd {
        int x, y, w, h;
    };

    /// @brief Rectangle shape command.
    struct rect_cmd {
        int x, y, width, height;
    };

    /// @brief Triangle shape command.
    struct triangle_cmd {
        int x1, y1, x2, y2, x3, y3;
    };

    /// @brief Line shape command.
    struct line_cmd {
        int x1, y1, x2, y2;
        int thickness;
    };

    /// @brief Oval outline command (bounding-box based).
    struct oval_outline_cmd {
        int x, y, w, h, thickness;
    };

    /// @brief Rectangle outline command.
    struct rect_outline_cmd {
        int x, y, width, height, thickness;
    };

    /// @brief Text rendering command (3x5 bitmap font).
    struct text_cmd {
        int x, y;
        const char* str;
    };

    /// @brief Palette index control command.
    struct palette_idx_cmd {
        int index;
    };

    /// @brief Empty type to denote end of shape group.
    struct group_end_marker {};

    /// @brief Group of shapes under a single palette index.
    template<typename... Shapes>
    struct group_cmd {
        std::tuple<Shapes...> shapes;

        /// @brief Create group with shapes.
        template<typename... S>
        constexpr group_cmd(S&&... s) : shapes(std::forward<S>(s)...) {}
    };

    namespace bits {

        /// @brief Floor a double to int (truncation towards negative infinity).
        constexpr int floor_to_int(double v) {
            int i = static_cast<int>(v);
            return (static_cast<double>(i) > v) ? i - 1 : i;
        }

    } // namespace bits

    /// @brief Compiled sprite data in 4bpp format.
    /// @tparam W Sprite width in pixels
    /// @tparam H Sprite height in pixels
    /// @tparam TileCount Number of 8x8 4bpp tiles stored for this sprite
    ///
    /// Example:
    /// @code{.cpp}
    /// constexpr auto sprite = gba::shapes::sprite_16x16(
    ///     gba::shapes::circle(8.0, 8.0, 4.0)
    /// );
    ///
    /// auto* dest = gba::memory_map(gba::mem_vram_obj);
    /// std::memcpy(dest, sprite.data(), sprite.size());
    ///
    /// // Regular object with tile_index from VRAM pointer
    /// gba::obj_mem[0] = sprite.obj(gba::tile_index(dest));
    ///
    /// // Or affine object
    /// gba::obj_aff_mem[0] = sprite.obj_aff(gba::tile_index(dest));
    /// @endcode
    template<int W, int H, std::size_t TileCount = (W / 8) * (H / 8)>
    using sprite_data = gba::sprite4<W, H, TileCount>;

    namespace bits {

        /// @brief Set a pixel at (x, y) to palette_idx in 4bpp tile-ordered buffer.
        ///
        /// GBA sprites store pixels in 8x8 tile order (32 bytes per tile).
        /// Tiles are arranged left-to-right, top-to-bottom within the sprite.
        template<int W, int H>
        constexpr void set_pixel(sprite_data<W, H>& pixels, int x, int y, u8 palette_idx) {
            if (x < 0 || x >= W || y < 0 || y >= H) return;

            // Which 8x8 tile does this pixel fall in?
            int tile_col = x / 8;
            int tile_row = y / 8;
            int tile_idx = tile_row * (W / 8) + tile_col;

            // Position within the tile
            int local_x = x % 8;
            int local_y = y % 8;

            auto& row = pixels.tiles[tile_idx][local_y];
            const auto shift = static_cast<unsigned int>(local_x * 4);
            row = (row & ~(0xFu << shift)) | ((static_cast<unsigned int>(palette_idx) & 0xFu) << shift);
        }

        /// @brief Check if point is inside axis-aligned rectangle.
        constexpr bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
            return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
        }

        /// @brief Check if point is inside oval inscribed in bounding box.
        constexpr bool point_in_oval(int px, int py, int bx, int by, int bw, int bh) {
            // Map pixel center to doubled coordinate space for symmetry
            // Pixel (px, py) center is at (2*px + 1, 2*py + 1) in doubled space
            // Bounding box center is at (2*bx + bw, 2*by + bh)
            int dx = (2 * px + 1) - (2 * bx + bw);
            int dy = (2 * py + 1) - (2 * by + bh);
            // Semi-axes in doubled space: bw and bh
            // Ellipse test: (dx/bw)^2 + (dy/bh)^2 <= 1
            // Scaled: dx^2 * bh^2 + dy^2 * bw^2 <= bw^2 * bh^2
            return (long long)dx * dx * bh * bh + (long long)dy * dy * bw * bw <= (long long)bw * bw * bh * bh;
        }

        /// @brief Check if point is on oval outline inscribed in bounding box.
        constexpr bool point_on_oval_outline(int px, int py, int bx, int by, int bw, int bh, int thickness) {
            return point_in_oval(px, py, bx, by, bw, bh) &&
                   !point_in_oval(px, py, bx + thickness, by + thickness, bw - 2 * thickness, bh - 2 * thickness);
        }

        /// @brief Signed area of triangle (used for point-in-triangle test).
        constexpr int sign(int x1, int y1, int x2, int y2, int x3, int y3) {
            return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
        }

        /// @brief Check if point is inside triangle using barycentric coordinates.
        constexpr bool point_in_triangle(int x, int y, int x1, int y1, int x2, int y2, int x3, int y3) {
            int d1 = sign(x, y, x1, y1, x2, y2);
            int d2 = sign(x, y, x2, y2, x3, y3);
            int d3 = sign(x, y, x3, y3, x1, y1);

            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

            return !(has_neg && has_pos);
        }

        /// @brief Simple line drawing using DDA algorithm.
        template<int W, int H>
        constexpr void draw_line(sprite_data<W, H>& pixels, int x1, int y1, int x2, int y2, int thickness,
                                 u8 palette_idx) {
            int dx = x2 - x1;
            int dy = y2 - y1;
            int steps = (dx != 0) ? std::abs(dx) : std::abs(dy);
            if (steps == 0) steps = 1;

            for (int step = 0; step <= steps; ++step) {
                int x = x1 + (dx * step) / steps;
                int y = y1 + (dy * step) / steps;

                for (int tx = -thickness / 2; tx <= thickness / 2; ++tx) {
                    for (int ty = -thickness / 2; ty <= thickness / 2; ++ty) {
                        if (tx * tx + ty * ty <= (thickness / 2) * (thickness / 2)) {
                            set_pixel<W, H>(pixels, x + tx, y + ty, palette_idx);
                        }
                    }
                }
            }
        }

        /// @brief Check if point is on rectangle outline (within thickness).
        constexpr bool point_on_rect_outline(int x, int y, int rx, int ry, int rw, int rh, int thickness) {
            if (x < rx || x >= rx + rw || y < ry || y >= ry + rh) return false;

            int dist_from_left = x - rx;
            int dist_from_right = (rx + rw) - x - 1;
            int dist_from_top = y - ry;
            int dist_from_bottom = (ry + rh) - y - 1;

            return dist_from_left < thickness || dist_from_right < thickness || dist_from_top < thickness ||
                   dist_from_bottom < thickness;
        }

#include "font_3x5.hpp"

        /// @brief Render a single character glyph to sprite buffer.
        template<int W, int H>
        constexpr void draw_char(sprite_data<W, H>& pixels, int px, int py, char c, u8 palette_idx) {
            if (c < 32 || c > 127) c = '?';
            const std::uint16_t glyph = gba::bits::font_3x5_data[c - 32];

            for (int row = 0; row < 5; ++row) {
                const int y = py + row;
                if (y < 0 || y >= H) continue;

                const int glyph_pixels = gba::bits::get_font_line(glyph, row);

                if ((glyph_pixels & 0x08) && px >= 0 && px < W) {
                    set_pixel<W, H>(pixels, px, y, palette_idx);
                }
                if ((glyph_pixels & 0x04) && px + 1 >= 0 && px + 1 < W) {
                    set_pixel<W, H>(pixels, px + 1, y, palette_idx);
                }
                if ((glyph_pixels & 0x02) && px + 2 >= 0 && px + 2 < W) {
                    set_pixel<W, H>(pixels, px + 2, y, palette_idx);
                }
            }
        }

    } // namespace bits

    template<int W, int H, typename... Items>
    constexpr sprite_data<W, H> compile_sprite(Items&&... items) {
        sprite_data<W, H> result{};

        int current_palette_idx = 1;

        auto process_item = [&](const auto& item) {
            using ItemType = std::decay_t<decltype(item)>;

            if constexpr (std::is_same_v<ItemType, palette_idx_cmd>) {
                current_palette_idx = item.index;
            } else {
                auto render_shape = [&](const auto& shape) {
                    using ShapeType = std::decay_t<decltype(shape)>;

                    if constexpr (!std::is_same_v<ShapeType, group_end_marker>) {
                        if constexpr (std::is_same_v<ShapeType, oval_cmd>) {
                            for (int y = 0; y < H; ++y) {
                                for (int x = 0; x < W; ++x) {
                                    if (bits::point_in_oval(x, y, shape.x, shape.y, shape.w, shape.h)) {
                                        bits::set_pixel<W, H>(result, x, y, current_palette_idx);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<ShapeType, rect_cmd>) {
                            for (int y = 0; y < H; ++y) {
                                for (int x = 0; x < W; ++x) {
                                    if (bits::point_in_rect(x, y, shape.x, shape.y, shape.width, shape.height)) {
                                        bits::set_pixel<W, H>(result, x, y, current_palette_idx);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<ShapeType, triangle_cmd>) {
                            for (int y = 0; y < H; ++y) {
                                for (int x = 0; x < W; ++x) {
                                    if (bits::point_in_triangle(x, y, shape.x1, shape.y1, shape.x2, shape.y2, shape.x3,
                                                                shape.y3)) {
                                        bits::set_pixel<W, H>(result, x, y, current_palette_idx);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<ShapeType, line_cmd>) {
                            bits::draw_line<W, H>(result, shape.x1, shape.y1, shape.x2, shape.y2, shape.thickness,
                                                  current_palette_idx);
                        } else if constexpr (std::is_same_v<ShapeType, oval_outline_cmd>) {
                            for (int y = 0; y < H; ++y) {
                                for (int x = 0; x < W; ++x) {
                                    if (bits::point_on_oval_outline(x, y, shape.x, shape.y, shape.w, shape.h,
                                                                    shape.thickness)) {
                                        bits::set_pixel<W, H>(result, x, y, current_palette_idx);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<ShapeType, rect_outline_cmd>) {
                            for (int y = 0; y < H; ++y) {
                                for (int x = 0; x < W; ++x) {
                                    if (bits::point_on_rect_outline(x, y, shape.x, shape.y, shape.width, shape.height,
                                                                    shape.thickness)) {
                                        bits::set_pixel<W, H>(result, x, y, current_palette_idx);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<ShapeType, text_cmd>) {
                            int char_x = shape.x;
                            const char* str = shape.str;
                            while (*str && char_x < W) {
                                bits::draw_char<W, H>(result, char_x, shape.y, *str, current_palette_idx);
                                char_x += 4;
                                ++str;
                            }
                        }
                    }
                };

                if constexpr (std::is_same_v<ItemType, oval_cmd> || std::is_same_v<ItemType, rect_cmd> ||
                              std::is_same_v<ItemType, triangle_cmd> || std::is_same_v<ItemType, line_cmd> ||
                              std::is_same_v<ItemType, oval_outline_cmd> ||
                              std::is_same_v<ItemType, rect_outline_cmd> || std::is_same_v<ItemType, text_cmd>) {
                    render_shape(item);
                } else {
                    std::apply([&](const auto&... shapes) { (..., render_shape(shapes)); }, item.shapes);
                }

                ++current_palette_idx;
            }
        };

        (..., process_item(items));

        return result;
    }

} // namespace gba::shapes
