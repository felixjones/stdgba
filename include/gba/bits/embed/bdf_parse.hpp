/// @file bits/embed/bdf_parse.hpp
/// @brief Internal BDF parser for gba::embed.
#pragma once

#include <gba/bits/embed/bdf_pack.hpp>
#include <gba/bits/embed/bdf_types.hpp>

#include <cstddef>
#include <tuple>

namespace gba::embed::bits {

    struct bdf_header {
        unsigned int glyph_count{};
        unsigned int font_width{};
        unsigned int font_height{};
        int font_x{};
        int font_y{};
        unsigned int ascent{};
        unsigned int descent{};
        unsigned int default_char{};
    };

    consteval const unsigned char* skip_bdf_ws(const unsigned char* ptr, const unsigned char* end) {
        while (ptr < end && is_bdf_ws(*ptr)) ++ptr;
        return ptr;
    }

    consteval bdf_line next_bdf_line(const unsigned char* data, std::size_t size, std::size_t& pos) {
        if (pos >= size) return {};

        const auto start = pos;
        while (pos < size && data[pos] != '\n') ++pos;
        auto end = pos;

        if (pos < size && data[pos] == '\n') ++pos;
        if (end > start && data[end - 1] == '\r') --end;

        return {data + start, end - start};
    }

    template<std::size_t N>
    consteval bool line_starts_with(bdf_line line, const char (&prefix)[N]) {
        static_assert(N > 0);
        constexpr auto prefix_len = N - 1;
        if (line.size < prefix_len) return false;

        for (std::size_t i = 0; i < prefix_len; ++i) {
            if (line.data[i] != static_cast<unsigned char>(prefix[i])) return false;
        }
        return true;
    }

    consteval bool line_starts_with(bdf_line line, const char* prefix) {
        const auto prefix_len = cstrlen(prefix);
        if (line.size < prefix_len) return false;

        for (std::size_t i = 0; i < prefix_len; ++i) {
            if (line.data[i] != static_cast<unsigned char>(prefix[i])) return false;
        }
        return true;
    }

    template<std::size_t N>
    consteval const unsigned char* line_after_prefix(bdf_line line, const char (&prefix)[N]) {
        static_assert(N > 0);
        if (!line_starts_with(line, prefix)) throw "bdf: expected line prefix";
        return line.data + (N - 1);
    }

    consteval const unsigned char* line_after_prefix(bdf_line line, const char* prefix) {
        if (!line_starts_with(line, prefix)) throw "bdf: expected line prefix";
        return line.data + cstrlen(prefix);
    }

    consteval unsigned int parse_next_uint(const unsigned char*& ptr, const unsigned char* end) {
        ptr = skip_bdf_ws(ptr, end);
        if (ptr >= end || *ptr < '0' || *ptr > '9') throw "bdf: expected unsigned integer";

        unsigned int value = 0;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            value = value * 10u + static_cast<unsigned int>(*ptr - '0');
            ++ptr;
        }
        return value;
    }

    consteval int parse_next_int(const unsigned char*& ptr, const unsigned char* end) {
        ptr = skip_bdf_ws(ptr, end);
        if (ptr >= end) throw "bdf: expected signed integer";

        bool negative = false;
        if (*ptr == '-') {
            negative = true;
            ++ptr;
        }

        if (ptr >= end || *ptr < '0' || *ptr > '9') throw "bdf: expected signed integer";

        int value = 0;
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            value = value * 10 + static_cast<int>(*ptr - '0');
            ++ptr;
        }

        return negative ? -value : value;
    }

    template<std::size_t N>
    consteval unsigned int parse_prefixed_uint(bdf_line line, const char (&prefix)[N]) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;
        return parse_next_uint(ptr, end);
    }

    consteval unsigned int parse_prefixed_uint(bdf_line line, const char* prefix) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;
        return parse_next_uint(ptr, end);
    }

    template<std::size_t N>
    consteval int parse_prefixed_int(bdf_line line, const char (&prefix)[N]) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;
        return parse_next_int(ptr, end);
    }

    consteval int parse_prefixed_int(bdf_line line, const char* prefix) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;
        return parse_next_int(ptr, end);
    }

    template<std::size_t N>
    consteval void parse_prefixed_bbox(bdf_line line, const char (&prefix)[N], unsigned int& width,
                                       unsigned int& height, int& x, int& y) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;

        width = parse_next_uint(ptr, end);
        height = parse_next_uint(ptr, end);
        x = parse_next_int(ptr, end);
        y = parse_next_int(ptr, end);
    }

    template<std::size_t N>
    consteval void parse_prefixed_bbox_wh(bdf_line line, const char (&prefix)[N], unsigned int& width,
                                          unsigned int& height) {
        auto ptr = line_after_prefix(line, prefix);
        const auto end = line.data + line.size;

        width = parse_next_uint(ptr, end);
        height = parse_next_uint(ptr, end);
    }

    struct bdf_layout {
        bdf_header header{};
        std::size_t bitmap_bytes{};
    };

    template<typename Data>
    consteval bdf_header parse_bdf_header(const Data& data) {
        bdf_header header{};

        std::size_t pos = 0;
        bool in_properties = false;
        constexpr std::size_t data_size = std::tuple_size_v<Data>;

        while (pos < data_size) {
            const auto line = next_bdf_line(data.data(), data_size, pos);
            if (line.size == 0) continue;

            if (line_starts_with(line, "STARTPROPERTIES ")) {
                in_properties = true;
                continue;
            }

            if (line_starts_with(line, "ENDPROPERTIES")) {
                in_properties = false;
                continue;
            }

            if (line_starts_with(line, "FONTBOUNDINGBOX ")) {
                parse_prefixed_bbox(line, "FONTBOUNDINGBOX ", header.font_width, header.font_height, header.font_x,
                                    header.font_y);
                continue;
            }

            if (in_properties && line_starts_with(line, "FONT_ASCENT ")) {
                header.ascent = parse_prefixed_uint(line, "FONT_ASCENT ");
                continue;
            }

            if (in_properties && line_starts_with(line, "FONT_DESCENT ")) {
                header.descent = parse_prefixed_uint(line, "FONT_DESCENT ");
                continue;
            }

            if (in_properties && line_starts_with(line, "DEFAULT_CHAR ")) {
                header.default_char = parse_prefixed_uint(line, "DEFAULT_CHAR ");
                continue;
            }

            if (line_starts_with(line, "CHARS ")) {
                header.glyph_count = parse_prefixed_uint(line, "CHARS ");
                break;
            }
        }

        if (header.glyph_count == 0) throw "bdf: CHARS missing or zero";

        if (header.ascent == 0 && header.descent == 0 && header.font_height != 0) {
            header.ascent = static_cast<unsigned int>(header.font_height + header.font_y);
            header.descent = header.font_y < 0 ? static_cast<unsigned int>(-header.font_y) : 0u;
        }

        return header;
    }

    template<typename Data>
    consteval bdf_layout parse_bdf_layout(const Data& data) {
        bdf_layout layout{};
        auto& header = layout.header;

        std::size_t pos = 0;
        bool in_properties = false;
        bool seen_chars = false;
        unsigned int seen_bbx = 0;
        constexpr std::size_t data_size = std::tuple_size_v<Data>;

        while (pos < data_size) {
            const auto line = next_bdf_line(data.data(), data_size, pos);
            if (line.size == 0) continue;

            if (!seen_chars) {
                if (line_starts_with(line, "STARTPROPERTIES ")) {
                    in_properties = true;
                    continue;
                }

                if (line_starts_with(line, "ENDPROPERTIES")) {
                    in_properties = false;
                    continue;
                }

                if (line_starts_with(line, "FONTBOUNDINGBOX ")) {
                    parse_prefixed_bbox(line, "FONTBOUNDINGBOX ", header.font_width, header.font_height, header.font_x,
                                        header.font_y);
                    continue;
                }

                if (in_properties && line_starts_with(line, "FONT_ASCENT ")) {
                    header.ascent = parse_prefixed_uint(line, "FONT_ASCENT ");
                    continue;
                }

                if (in_properties && line_starts_with(line, "FONT_DESCENT ")) {
                    header.descent = parse_prefixed_uint(line, "FONT_DESCENT ");
                    continue;
                }

                if (in_properties && line_starts_with(line, "DEFAULT_CHAR ")) {
                    header.default_char = parse_prefixed_uint(line, "DEFAULT_CHAR ");
                    continue;
                }

                if (line_starts_with(line, "CHARS ")) {
                    header.glyph_count = parse_prefixed_uint(line, "CHARS ");
                    if (header.glyph_count == 0) throw "bdf: CHARS missing or zero";
                    seen_chars = true;
                    continue;
                }

                continue;
            }

            if (line_starts_with(line, "BBX ")) {
                unsigned int width = 0;
                unsigned int height = 0;
                parse_prefixed_bbox_wh(line, "BBX ", width, height);
                layout.bitmap_bytes += static_cast<std::size_t>((width + 7u) / 8u) * height;
                ++seen_bbx;
                if (seen_bbx == header.glyph_count) break;
            }
        }

        if (!seen_chars) throw "bdf: CHARS missing or zero";
        if (seen_bbx != header.glyph_count) throw "bdf: CHARS count does not match parsed BBX blocks";

        if (header.ascent == 0 && header.descent == 0 && header.font_height != 0) {
            header.ascent = static_cast<unsigned int>(header.font_height + header.font_y);
            header.descent = header.font_y < 0 ? static_cast<unsigned int>(-header.font_y) : 0u;
        }

        return layout;
    }

    template<typename Data>
    consteval std::size_t bdf_bitmap_bytes(const Data& data, unsigned int glyph_count) {
        std::size_t pos = 0;
        std::size_t total = 0;
        unsigned int seen = 0;
        constexpr std::size_t data_size = std::tuple_size_v<Data>;

        while (pos < data_size) {
            const auto line = next_bdf_line(data.data(), data_size, pos);
            if (line_starts_with(line, "BBX ")) {
                unsigned int width = 0;
                unsigned int height = 0;
                parse_prefixed_bbox_wh(line, "BBX ", width, height);
                total += static_cast<std::size_t>((width + 7u) / 8u) * height;
                ++seen;
                if (seen == glyph_count) break;
            }
        }

        return total;
    }

    template<std::size_t GlyphCount, std::size_t BitmapBytes, typename Data>
    consteval auto build_bdf_font(const Data& data, const bdf_header& header) {
        bdf_font_result<GlyphCount, BitmapBytes> result{};
        result.font_width = static_cast<unsigned short>(header.font_width);
        result.font_height = static_cast<unsigned short>(header.font_height);
        result.font_x = static_cast<short>(header.font_x);
        result.font_y = static_cast<short>(header.font_y);
        result.ascent = static_cast<unsigned short>(header.ascent);
        result.descent = static_cast<unsigned short>(header.descent);
        result.default_char = header.default_char;

        using glyph_type = typename bdf_font_result<GlyphCount, BitmapBytes>::glyph;

        std::size_t pos = 0;
        std::size_t glyph_index = 0;
        glyph_type current{};
        bool in_glyph = false;
        bool in_bitmap = false;
        unsigned int bitmap_row = 0;
        unsigned int bitmap_offset = 0;
        constexpr std::size_t data_size = std::tuple_size_v<Data>;

        while (pos < data_size) {
            const auto line = next_bdf_line(data.data(), data_size, pos);
            if (line.size == 0) continue;

            if (line_starts_with(line, "STARTCHAR ")) {
                in_glyph = true;
                in_bitmap = false;
                bitmap_row = 0;
                current = {};
                continue;
            }

            if (!in_glyph) continue;

            if (line_starts_with(line, "ENCODING ")) {
                const int encoding = parse_prefixed_int(line, "ENCODING ");
                current.encoding = encoding < 0 ? bdf_font_result<GlyphCount, BitmapBytes>::encoding_none
                                                : static_cast<unsigned int>(encoding);
                continue;
            }

            if (line_starts_with(line, "DWIDTH ")) {
                auto ptr = line_after_prefix(line, "DWIDTH ");
                const auto end = line.data + line.size;
                current.dwidth = static_cast<unsigned short>(parse_next_uint(ptr, end));
                continue;
            }

            if (line_starts_with(line, "BBX ")) {
                unsigned int width = 0;
                unsigned int height = 0;
                int x = 0;
                int y = 0;
                parse_prefixed_bbox(line, "BBX ", width, height, x, y);
                current.width = static_cast<unsigned short>(width);
                current.height = static_cast<unsigned short>(height);
                current.x_offset = static_cast<short>(x);
                current.y_offset = static_cast<short>(y);
                current.bitmap_offset = bitmap_offset;
                current.bitmap_byte_width = static_cast<unsigned short>((width + 7u) / 8u);
                continue;
            }

            if (line_starts_with(line, "BITMAP")) {
                in_bitmap = true;
                bitmap_row = 0;
                continue;
            }

            if (line_starts_with(line, "ENDCHAR")) {
                if (glyph_index >= GlyphCount) throw "bdf: glyph count mismatch";
                if (bitmap_row != current.height) throw "bdf: bitmap row count does not match BBX height";

                result.glyphs[glyph_index++] = current;
                bitmap_offset += current.bitmap_bytes();
                in_glyph = false;
                in_bitmap = false;
                continue;
            }

            if (in_bitmap) {
                if (bitmap_row >= current.height) throw "bdf: bitmap row count exceeds BBX height";

                pack_bdf_row(line, current.width,
                             result.bitmap.data() + current.bitmap_offset + bitmap_row * current.bitmap_byte_width,
                             current.bitmap_byte_width);

                ++bitmap_row;
            }
        }

        if (glyph_index != GlyphCount) throw "bdf: CHARS count does not match parsed glyphs";
        if (bitmap_offset != BitmapBytes) throw "bdf: bitmap size mismatch";

        return result;
    }

} // namespace gba::embed::bits
