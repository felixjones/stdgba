/// @file bits/format/parse.hpp
/// @brief Python-style format string parser.
///
/// Canonical grammar:
///   [[fill]align][sign][#][0][width][grouping][.precision][type]
///
/// Placeholder forms:
///   {} - positional (auto), {0} - explicit positional, {name} - named
///   {:spec} - with format spec
#pragma once

#include <gba/bits/format/fixed_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::format {

    /// @brief Parsed format specification following Python's mini-language.
    struct format_spec {
        enum class align_t : std::uint8_t {
            none,
            left,      // <
            right,     // >
            center,    // ^
            sign_aware // =
        };

        enum class sign_t : std::uint8_t {
            none,  // default: only show '-'
            plus,  // '+'
            minus, // '-'
            space  // ' '
        };

        enum class type_t : std::uint8_t {
            default_fmt,
            // Integer / shared numeric
            decimal,   // 'd'
            hex_lower, // 'x'
            hex_upper, // 'X'
            binary,    // 'b'
            octal,     // 'o'
            grouped,   // 'n'
            character, // 'c'
            // String
            string, // 's'
            // Fixed-point
            fixed_lower,      // 'f'
            fixed_upper,      // 'F'
            percent,          // '%'
            scientific_lower, // 'e'
            scientific_upper, // 'E'
            general_lower,    // 'g'
            general_upper,    // 'G'
            // Angle
            radians, // 'r'
            turns,   // 't'
            raw_int  // 'i'
        };

        enum class grouping_t : std::uint8_t {
            none,
            comma,     // ','
            underscore // '_'
        };

        align_t alignment = align_t::none;
        sign_t sign = sign_t::none;
        type_t fmt_type = type_t::default_fmt;
        grouping_t grouping = grouping_t::none;
        char fill = ' ';
        std::uint8_t width = 0;
        std::uint8_t precision = 0;
        bool has_precision = false;
        bool alternate_form = false;
        bool zero_pad = false;

        constexpr format_spec() = default;
    };

    // Segment types

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

        static constexpr format_segment make_named(unsigned int hash, std::uint16_t start, std::uint8_t len,
                                                   format_spec s = {}) {
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

        consteval void add_literal(std::uint16_t start, std::uint16_t len) {
            if (len > 0 && segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_literal(start, len);
            }
        }

        consteval void add_named(unsigned int hash, std::uint16_t name_start, std::uint8_t name_len,
                                 format_spec spec = {}) {
            if (segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_named(hash, name_start, name_len, spec);
                ++named_count;
            }
        }

        consteval void add_positional(std::uint8_t index, format_spec spec = {}) {
            if (segment_count < MAX_SEGMENTS) {
                segments[segment_count++] = format_segment::make_positional(index, spec);
                ++positional_count;
            }
        }

        consteval void set_error(std::size_t pos) {
            valid = false;
            error_pos = pos;
        }
    };

    // Parser helpers

    namespace bits {

        consteval bool is_digit(char c) {
            return c >= '0' && c <= '9';
        }
        consteval bool is_alpha(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }
        consteval bool is_alnum(char c) {
            return is_digit(c) || is_alpha(c);
        }
        consteval bool is_name_char(char c) {
            return is_alnum(c) || c == '_';
        }

        consteval std::uint32_t parse_uint(const char* str, std::size_t& pos, std::size_t end) {
            std::uint32_t result = 0;
            while (pos < end && is_digit(str[pos])) {
                result = result * 10 + (str[pos] - '0');
                ++pos;
            }
            return result;
        }

        consteval bool is_align_char(char c) {
            return c == '<' || c == '>' || c == '^' || c == '=';
        }

        consteval format_spec::align_t align_from_char(char c) {
            switch (c) {
                case '<': return format_spec::align_t::left;
                case '>': return format_spec::align_t::right;
                case '^': return format_spec::align_t::center;
                case '=': return format_spec::align_t::sign_aware;
                default: return format_spec::align_t::none;
            }
        }

        consteval format_spec::type_t type_from_char(char c) {
            switch (c) {
                case 'd': return format_spec::type_t::decimal;
                case 'b': return format_spec::type_t::binary;
                case 'o': return format_spec::type_t::octal;
                case 'x': return format_spec::type_t::hex_lower;
                case 'X': return format_spec::type_t::hex_upper;
                case 's': return format_spec::type_t::string;
                case 'n': return format_spec::type_t::grouped;
                case 'c': return format_spec::type_t::character;
                case 'f': return format_spec::type_t::fixed_lower;
                case 'F': return format_spec::type_t::fixed_upper;
                case '%': return format_spec::type_t::percent;
                case 'e': return format_spec::type_t::scientific_lower;
                case 'E': return format_spec::type_t::scientific_upper;
                case 'g': return format_spec::type_t::general_lower;
                case 'G': return format_spec::type_t::general_upper;
                case 'r': return format_spec::type_t::radians;
                case 't': return format_spec::type_t::turns;
                case 'i': return format_spec::type_t::raw_int;
                default: return format_spec::type_t::default_fmt;
            }
        }

        consteval bool is_type_char(char c) {
            return type_from_char(c) != format_spec::type_t::default_fmt;
        }

        consteval bool validate_format_spec(const format_spec& spec) {
            if (spec.fmt_type == format_spec::type_t::string) {
                if (spec.sign != format_spec::sign_t::none) return false;
                if (spec.alternate_form) return false;
                if (spec.grouping != format_spec::grouping_t::none) return false;
                if (spec.alignment == format_spec::align_t::sign_aware) return false;
            }

            if (spec.fmt_type == format_spec::type_t::character) {
                if (spec.has_precision) return false;
                if (spec.grouping != format_spec::grouping_t::none) return false;
                if (spec.alternate_form) return false;
                if (spec.sign != format_spec::sign_t::none) return false;
            }

            if (spec.fmt_type == format_spec::type_t::raw_int) {
                if (spec.has_precision) return false;
            }

            return true;
        }

        /// @brief Parse format spec following Python canonical order.
        ///
        /// Grammar: [[fill]align][sign][#][0][width][grouping][.precision][type]
        consteval format_spec parse_format_spec(const char* str, std::size_t& pos, std::size_t end) {
            format_spec spec{};
            if (pos >= end) return spec;

            // 1. [[fill]align]
            if (pos + 1 < end && is_align_char(str[pos + 1])) {
                spec.fill = str[pos];
                spec.alignment = align_from_char(str[pos + 1]);
                pos += 2;
            } else if (pos < end && is_align_char(str[pos])) {
                spec.alignment = align_from_char(str[pos]);
                ++pos;
            }

            // 2. [sign]
            if (pos < end) {
                switch (str[pos]) {
                    case '+':
                        spec.sign = format_spec::sign_t::plus;
                        ++pos;
                        break;
                    case '-':
                        spec.sign = format_spec::sign_t::minus;
                        ++pos;
                        break;
                    case ' ':
                        spec.sign = format_spec::sign_t::space;
                        ++pos;
                        break;
                    default: break;
                }
            }

            // 3. [#]
            if (pos < end && str[pos] == '#') {
                spec.alternate_form = true;
                ++pos;
            }

            // 4. [0]
            if (pos < end && str[pos] == '0') {
                spec.zero_pad = true;
                ++pos;
            }

            // 5. [width]
            if (pos < end && is_digit(str[pos])) {
                spec.width = static_cast<std::uint8_t>(parse_uint(str, pos, end));
            }

            // 6. [grouping]
            if (pos < end && (str[pos] == ',' || str[pos] == '_')) {
                spec.grouping = (str[pos] == ',') ? format_spec::grouping_t::comma
                                                  : format_spec::grouping_t::underscore;
                ++pos;
            }

            // 7. [.precision]
            if (pos < end && str[pos] == '.') {
                ++pos;
                spec.precision = static_cast<std::uint8_t>(parse_uint(str, pos, end));
                spec.has_precision = true;
            }

            // 8. [type]
            if (pos < end && is_type_char(str[pos])) {
                spec.fmt_type = type_from_char(str[pos]);
                ++pos;
            }

            return spec;
        }

    } // namespace bits

    // Main parser

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

                ++pos; // Skip '{'

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
                    spec = bits::parse_format_spec(str, spec_pos, close);
                    if (spec_pos != close || !bits::validate_format_spec(spec)) {
                        ast.set_error(colon + 1);
                        return ast;
                    }
                }

                if (name_len == 0) {
                    ast.add_positional(next_positional++, spec);
                } else if (bits::is_digit(str[name_start])) {
                    std::size_t idx_pos = name_start;
                    auto idx = bits::parse_uint(str, idx_pos, name_end);
                    ast.add_positional(static_cast<std::uint8_t>(idx), spec);
                } else {
                    unsigned int hash = fnv1a_hash(str + name_start, name_len);
                    ast.add_named(hash, static_cast<std::uint16_t>(name_start), static_cast<std::uint8_t>(name_len),
                                  spec);
                }

                pos = close + 1;
                literal_start = pos;

            } else if (str[pos] == '}') {
                if (pos + 1 < len && str[pos + 1] == '}') {
                    ast.add_literal(static_cast<std::uint16_t>(literal_start),
                                    static_cast<std::uint16_t>(pos - literal_start + 1));
                    pos += 2;
                    literal_start = pos;
                    continue;
                }
                ast.set_error(pos);
                return ast;
            } else {
                ++pos;
            }
        }

        // Trailing literal
        if (pos > literal_start) {
            ast.add_literal(static_cast<std::uint16_t>(literal_start), static_cast<std::uint16_t>(pos - literal_start));
        }

        return ast;
    }

} // namespace gba::format
