/**
 * @file bits/format/parse.hpp
 * @brief Enhanced format string parser with max value hints and positional args.
 *
 * Extended syntax:
 * - `{}` - Positional argument (index 0, 1, 2...)
 * - `{0}` - Explicit positional index
 * - `{name}` - Named argument
 * - `{name:d<999}` - Named with decimal format, max value 999
 * - `{:x}` - Positional with hex format
 */
#pragma once

#include <gba/bits/format/fixed_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::format {

    // Format Spec (duplicated here to avoid circular deps, or could be shared)

    struct format_spec {
        enum class align_t : std::uint8_t { none, left, right, center };
        enum class sign_t : std::uint8_t { none, plus, minus, space };
        enum class type_t : std::uint8_t {
            default_fmt, decimal, hex_lower, hex_upper, binary, string
        };

        align_t alignment;
        sign_t sign;
        type_t fmt_type;
        char fill;
        std::uint8_t width;
        std::uint8_t precision;
        std::uint32_t max_value;
        bool zero_pad;

        constexpr format_spec()
            : alignment{align_t::none}
            , sign{sign_t::none}
            , fmt_type{type_t::default_fmt}
            , fill{' '}
            , width{0}
            , precision{255}
            , max_value{0}
            , zero_pad{false}
        {}

        [[nodiscard]] constexpr std::uint8_t max_digits() const {
            if (max_value == 0) return 10;
            std::uint8_t digits = 0;
            auto v = max_value;
            while (v > 0) { ++digits; v /= 10; }
            return digits > 0 ? digits : 1;
        }
    };

    // Segment Types

    enum class segment_type : std::uint8_t {
        literal,
        named_placeholder,
        positional_placeholder,
    };

    struct format_segment {
        segment_type type = segment_type::literal;

        // Literal data
        std::uint16_t lit_start = 0;
        std::uint16_t lit_length = 0;

        // Named placeholder data
        unsigned int name_hash = 0;
        std::uint16_t name_start = 0;
        std::uint8_t name_len = 0;

        // Positional placeholder data
        std::uint8_t pos_index = 0;

        // Format spec (shared by named and positional)
        format_spec spec{};

        static constexpr format_segment make_literal(std::uint16_t start, std::uint16_t len) {
            format_segment seg{};
            seg.type = segment_type::literal;
            seg.lit_start = start;
            seg.lit_length = len;
            return seg;
        }

        static constexpr format_segment make_named(
            unsigned int hash,
            std::uint16_t start,
            std::uint8_t len,
            format_spec s = {}
        ) {
            format_segment seg{};
            seg.type = segment_type::named_placeholder;
            seg.name_hash = hash;
            seg.name_start = start;
            seg.name_len = len;
            seg.spec = s;
            return seg;
        }

        static constexpr format_segment make_positional(std::uint8_t index, format_spec s = {}) {
            format_segment seg{};
            seg.type = segment_type::positional_placeholder;
            seg.pos_index = index;
            seg.spec = s;
            return seg;
        }
    };

    // Format AST

    inline constexpr std::size_t MAX_SEGMENTS = 32;

    template<std::size_t N>
    struct format_ast {
        fixed_string<N> format_str;
        std::array<format_segment, MAX_SEGMENTS> segments{};
        std::size_t segment_count = 0;
        std::size_t named_count = 0;
        std::size_t positional_count = 0;
        bool valid = true;
        std::size_t error_pos = 0;

        // Workspace size calculation based on max value hints
        std::size_t required_workspace = 0;

        constexpr void add_literal(std::uint16_t start, std::uint16_t len) {
            if (len > 0 && segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_literal(start, len);
            }
        }

        constexpr void add_named(
            unsigned int hash,
            std::uint16_t name_start,
            std::uint8_t name_len,
            format_spec spec = {}
        ) {
            if (segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_named(hash, name_start, name_len, spec);
                ++named_count;
                required_workspace += spec.max_digits() + 2;  // +2 for sign and padding
            }
        }

        constexpr void add_positional(std::uint8_t index, format_spec spec = {}) {
            if (segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_positional(index, spec);
                ++positional_count;
                required_workspace += spec.max_digits() + 2;
            }
        }

        constexpr void set_error(std::size_t pos) {
            valid = false;
            error_pos = pos;
        }
    };

    // Parser Helpers

    namespace detail {

        constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
        constexpr bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
        constexpr bool is_alnum(char c) { return is_digit(c) || is_alpha(c); }
        constexpr bool is_name_char(char c) { return is_alnum(c) || c == '_'; }

        constexpr std::uint32_t parse_uint(const char* str, std::size_t& pos, std::size_t end) {
            std::uint32_t result = 0;
            while (pos < end && is_digit(str[pos])) {
                result = result * 10 + (str[pos] - '0');
                ++pos;
            }
            return result;
        }

        /**
         * @brief Parse extended format spec with max value hints.
         *
         * Syntax: [[fill]align][sign][#][0][width][.precision][<max_value][type]
         *
         * Extended: `<N` after width means "max value is N" for workspace sizing
         */
        consteval format_spec parse_format_spec(const char* str, std::size_t& pos, std::size_t end) {
            format_spec spec{};
            if (pos >= end) return spec;

            // Fill and align
            if (pos + 1 < end) {
                char next = str[pos + 1];
                if (next == '<' || next == '>' || next == '^') {
                    spec.fill = str[pos];
                    ++pos;
                    if (str[pos] == '<') spec.alignment = format_spec::align_t::left;
                    else if (str[pos] == '>') spec.alignment = format_spec::align_t::right;
                    else if (str[pos] == '^') spec.alignment = format_spec::align_t::center;
                    ++pos;
                }
            }

            // Align without fill
            if (spec.alignment == format_spec::align_t::none && pos < end) {
                char c = str[pos];
                if (c == '<') { spec.alignment = format_spec::align_t::left; ++pos; }
                else if (c == '>') { spec.alignment = format_spec::align_t::right; ++pos; }
                else if (c == '^') { spec.alignment = format_spec::align_t::center; ++pos; }
            }

            // Sign
            if (pos < end) {
                char c = str[pos];
                if (c == '+') { spec.sign = format_spec::sign_t::plus; ++pos; }
                else if (c == '-') { spec.sign = format_spec::sign_t::minus; ++pos; }
                else if (c == ' ') { spec.sign = format_spec::sign_t::space; ++pos; }
            }

            // Zero padding
            if (pos < end && str[pos] == '0') {
                spec.zero_pad = true;
                ++pos;
            }

            // Width
            if (pos < end && is_digit(str[pos])) {
                spec.width = static_cast<std::uint8_t>(parse_uint(str, pos, end));
            }

            // Precision
            if (pos < end && str[pos] == '.') {
                ++pos;
                spec.precision = static_cast<std::uint8_t>(parse_uint(str, pos, end));
            }

            // Max value hint: `<N` where N is the maximum expected value
            if (pos < end && str[pos] == '<') {
                ++pos;
                spec.max_value = parse_uint(str, pos, end);
            }

            // Type
            if (pos < end) {
                char c = str[pos];
                switch (c) {
                    case 'd': spec.fmt_type = format_spec::type_t::decimal; ++pos; break;
                    case 'x': spec.fmt_type = format_spec::type_t::hex_lower; ++pos; break;
                    case 'X': spec.fmt_type = format_spec::type_t::hex_upper; ++pos; break;
                    case 'b': spec.fmt_type = format_spec::type_t::binary; ++pos; break;
                    case 's': spec.fmt_type = format_spec::type_t::string; ++pos; break;
                    default: break;
                }
            }

            return spec;
        }

    } // namespace detail

    // Main Parser

    template<fixed_string Fmt>
    consteval auto parse_format() {
        format_ast<Fmt.size + 1> ast{Fmt};
        const char* str = Fmt.data;
        const std::size_t len = Fmt.size;

        std::size_t pos = 0;
        std::size_t literal_start = 0;
        std::uint8_t next_positional = 0;

        while (pos < len) {
            if (str[pos] == '{') {
                // Escaped brace?
                if (pos + 1 < len && str[pos + 1] == '{') {
                    // Add literal up to and including first brace
                    ast.add_literal(static_cast<std::uint16_t>(literal_start),
                                   static_cast<std::uint16_t>(pos - literal_start + 1));
                    pos += 2;
                    literal_start = pos;
                    continue;
                }

                // Add preceding literal
                if (pos > literal_start) {
                    ast.add_literal(static_cast<std::uint16_t>(literal_start),
                                   static_cast<std::uint16_t>(pos - literal_start));
                }

                ++pos;  // Skip '{'
                std::size_t placeholder_start = pos;

                // Find closing brace
                std::size_t close = pos;
                while (close < len && str[close] != '}') ++close;

                if (close >= len) {
                    ast.set_error(pos);
                    return ast;
                }

                // Parse placeholder content
                std::size_t colon = pos;
                while (colon < close && str[colon] != ':') ++colon;

                std::size_t name_end = colon;
                std::size_t name_start = pos;
                std::size_t name_len = name_end - name_start;

                format_spec spec{};
                if (colon < close) {
                    std::size_t spec_pos = colon + 1;
                    spec = detail::parse_format_spec(str, spec_pos, close);
                }

                if (name_len == 0) {
                    // Positional: {} or {:spec}
                    ast.add_positional(next_positional++, spec);
                } else if (detail::is_digit(str[name_start])) {
                    // Explicit positional: {0} or {1:spec}
                    std::size_t idx_pos = name_start;
                    auto idx = detail::parse_uint(str, idx_pos, name_end);
                    ast.add_positional(static_cast<std::uint8_t>(idx), spec);
                } else {
                    // Named: {foo} or {foo:spec}
                    unsigned int hash = fnv1a_hash(str + name_start, name_len);
                    ast.add_named(hash,
                                 static_cast<std::uint16_t>(name_start),
                                 static_cast<std::uint8_t>(name_len),
                                 spec);
                }

                pos = close + 1;
                literal_start = pos;

            } else if (str[pos] == '}') {
                // Escaped brace?
                if (pos + 1 < len && str[pos + 1] == '}') {
                    ast.add_literal(static_cast<std::uint16_t>(literal_start),
                                   static_cast<std::uint16_t>(pos - literal_start + 1));
                    pos += 2;
                    literal_start = pos;
                    continue;
                }
                // Unmatched }
                ast.set_error(pos);
                return ast;
            } else {
                ++pos;
            }
        }

        // Trailing literal
        if (pos > literal_start) {
            ast.add_literal(static_cast<std::uint16_t>(literal_start),
                           static_cast<std::uint16_t>(pos - literal_start));
        }

        return ast;
    }

} // namespace gba::format
