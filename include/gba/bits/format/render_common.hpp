/// @file bits/format/render_common.hpp
/// @brief Shared low-level helpers for format rendering.

#pragma once

#include <gba/bits/format/parse.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace gba::format::bits {
    template<typename>
    inline constexpr bool dependent_false_v = false;

    extern "C" std::size_t _stdgba_utoa10_reversed(std::uint32_t value, char* out);
    extern "C" std::size_t _stdgba_grouped3_copy_reversed(const char* reversed, std::size_t count, char* out,
                                                           char separator);
    extern "C" void _stdgba_fixed_frac_digits_u16(std::uint32_t remainder, unsigned int fracBits, char* out,
                                                  std::size_t count);
    extern "C" void _stdgba_frac_digits_u32(std::uint32_t remainder, char* out, std::size_t count);

    constexpr std::size_t cstrlen(const char* str) {
        if (str == nullptr) return 0;
        std::size_t len = 0;
        while (str[len] != '\0') ++len;
        return len;
    }

    constexpr bool is_break_char(char c) {
        return c == ' ' || c == '\n' || c == '\t' || c == '\0';
    }

    constexpr char grouping_char(format_spec::grouping_t grouping) {
        switch (grouping) {
            case format_spec::grouping_t::comma: return ',';
            case format_spec::grouping_t::underscore: return '_';
            default: return '\0';
        }
    }

    constexpr std::size_t group_size_for(format_spec::type_t type) {
        switch (type) {
            case format_spec::type_t::binary:
            case format_spec::type_t::hex_lower:
            case format_spec::type_t::hex_upper: return 4;
            default: return 3;
        }
    }

    constexpr bool is_scientific_type(format_spec::type_t type) {
        return type == format_spec::type_t::scientific_lower || type == format_spec::type_t::scientific_upper;
    }

    constexpr bool is_general_type(format_spec::type_t type) {
        return type == format_spec::type_t::general_lower || type == format_spec::type_t::general_upper;
    }

    constexpr bool is_scientific_uppercase(format_spec::type_t type) {
        return type == format_spec::type_t::scientific_upper || type == format_spec::type_t::general_upper;
    }

    constexpr bool uppercase_digits(format_spec::type_t type) {
        return type == format_spec::type_t::hex_upper;
    }

    constexpr unsigned int integer_base_for(format_spec::type_t type) {
        switch (type) {
            case format_spec::type_t::binary: return 2;
            case format_spec::type_t::octal: return 8;
            case format_spec::type_t::hex_lower:
            case format_spec::type_t::hex_upper: return 16;
            default: return 10;
        }
    }

    constexpr format_spec::align_t effective_alignment(const format_spec& spec, bool numeric) {
        if (spec.alignment != format_spec::align_t::none) return spec.alignment;
        if (numeric && spec.zero_pad) return format_spec::align_t::sign_aware;
        return numeric ? format_spec::align_t::right : format_spec::align_t::left;
    }

    constexpr char effective_fill(const format_spec& spec, bool numeric) {
        if (numeric && spec.zero_pad && spec.alignment == format_spec::align_t::none) return '0';
        return spec.fill;
    }

    constexpr char sign_char_for(bool negative, format_spec::sign_t sign) {
        if (negative) return '-';
        switch (sign) {
            case format_spec::sign_t::plus: return '+';
            case format_spec::sign_t::space: return ' ';
            default: return '\0';
        }
    }

    constexpr std::size_t trim_trailing_zeros(char* digits, std::size_t len) {
        while (len > 0 && digits[len - 1] == '0') --len;
        return len;
    }

    template<typename UInt, bool Runtime>
    constexpr std::size_t to_digits_reversed_impl(UInt value, unsigned int base, bool uppercase, char* out) {
        if constexpr (Runtime) {
            if (base == 10 && !uppercase) {
                if constexpr (sizeof(UInt) <= 4) {
                    return _stdgba_utoa10_reversed(static_cast<std::uint32_t>(value), out);
                } else if (value <= static_cast<UInt>(0xFFFFFFFFu)) {
                    return _stdgba_utoa10_reversed(static_cast<std::uint32_t>(value), out);
                }
            }
        }
        std::size_t count = 0;
        if (value == 0) {
            out[count++] = '0';
            return count;
        }
        while (value > 0) {
            const auto digit = static_cast<unsigned int>(value % base);
            if (digit < 10) out[count++] = static_cast<char>('0' + digit);
            else out[count++] = static_cast<char>((uppercase ? 'A' : 'a') + digit - 10);
            value /= base;
        }
        return count;
    }

    template<typename UInt>
    constexpr std::size_t to_digits_reversed(UInt value, unsigned int base, bool uppercase, char* out) {
        if (std::is_constant_evaluated()) {
            return to_digits_reversed_impl<UInt, false>(value, base, uppercase, out);
        } else {
            return to_digits_reversed_impl<UInt, true>(value, base, uppercase, out);
        }
    }

    template<typename UInt, bool Runtime>
    constexpr std::size_t write_grouped_digits_impl(UInt value, unsigned int base, bool uppercase,
                                                    format_spec::grouping_t grouping, format_spec::type_t type,
                                                    char* out) {
        char reversed[65]{};
        const auto count = to_digits_reversed(value, base, uppercase, reversed);
        if (grouping == format_spec::grouping_t::none && type != format_spec::type_t::grouped) {
            for (std::size_t i = 0; i < count; ++i) out[i] = reversed[count - 1 - i];
            return count;
        }

        const auto groupSize = group_size_for(type);
        const auto separator = grouping == format_spec::grouping_t::none ? ',' : grouping_char(grouping);
        if constexpr (Runtime) {
            if (base == 10 && !uppercase && groupSize == 3 && separator != '\0') {
                return _stdgba_grouped3_copy_reversed(reversed, count, out, separator);
            }
        }
        std::size_t outPos = 0;
        for (std::size_t i = count; i > 0; --i) {
            out[outPos++] = reversed[i - 1];
            if (i > 1 && ((i - 1) % groupSize) == 0) out[outPos++] = separator;
        }
        return outPos;
    }

    template<typename UInt>
    constexpr std::size_t write_grouped_digits(UInt value, unsigned int base, bool uppercase,
                                               format_spec::grouping_t grouping, format_spec::type_t type, char* out) {
        if (std::is_constant_evaluated()) {
            return write_grouped_digits_impl<UInt, false>(value, base, uppercase, grouping, type, out);
        } else {
            return write_grouped_digits_impl<UInt, true>(value, base, uppercase, grouping, type, out);
        }
    }

    constexpr std::size_t append_repeated(char* out, std::size_t pos, std::size_t cap, char ch, std::size_t count) {
        for (std::size_t i = 0; i < count && pos < cap; ++i) out[pos++] = ch;
        return pos;
    }

    constexpr std::size_t append_span(char* out, std::size_t pos, std::size_t cap, const char* src, std::size_t len) {
        for (std::size_t i = 0; i < len && pos < cap; ++i) out[pos++] = src[i];
        return pos;
    }
} // namespace gba::format::bits
