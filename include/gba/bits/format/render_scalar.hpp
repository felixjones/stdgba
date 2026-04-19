/// @file bits/format/render_scalar.hpp
/// @brief Internal string/integer renderers for gba::format.

#pragma once

#include <gba/bits/format/render_common.hpp>

#include <type_traits>

namespace gba::format::bits {
    template<typename T>
    constexpr std::size_t render_string_value(char* out, std::size_t cap, const T& value, const format_spec& spec) {
        const char* ptr = value == nullptr ? "" : value;
        std::size_t contentLen = cstrlen(ptr);
        if (spec.has_precision && spec.precision < contentLen) contentLen = spec.precision;

        const auto align = effective_alignment(spec, false);
        const auto fill = effective_fill(spec, false);
        const std::size_t totalLen = contentLen < spec.width ? spec.width : contentLen;
        const std::size_t pad = totalLen - contentLen;
        std::size_t leftPad = 0;
        std::size_t rightPad = 0;

        switch (align) {
            case format_spec::align_kind::center:
                leftPad = pad / 2;
                rightPad = pad - leftPad;
                break;
            case format_spec::align_kind::right: leftPad = pad; break;
            default: rightPad = pad; break;
        }

        std::size_t pos = 0;
        pos = append_repeated(out, pos, cap, fill, leftPad);
        pos = append_span(out, pos, cap, ptr, contentLen);
        pos = append_repeated(out, pos, cap, fill, rightPad);
        return pos;
    }

    template<typename T>
    constexpr std::size_t render_integer_value(char* out, std::size_t cap, const T& value, const format_spec& spec) {
        using UT = std::make_unsigned_t<T>;

        if (spec.fmt_type == format_spec::format_kind::character) {
            const char ch = static_cast<char>(value);
            char tmp[2] = {ch, '\0'};
            return render_string_value(out, cap, tmp, spec);
        }

        const auto effectiveType = spec.fmt_type == format_spec::format_kind::default_fmt ? format_spec::format_kind::decimal
                                                                                           : spec.fmt_type;
        const auto base = integer_base_for(effectiveType);
        const auto uppercase = uppercase_digits(effectiveType);

        bool negative = false;
        UT absValue{};
        if constexpr (std::is_signed_v<T>) {
            if (value < 0) {
                negative = true;
                absValue = static_cast<UT>(0) - static_cast<UT>(value);
            } else {
                absValue = static_cast<UT>(value);
            }
        } else {
            absValue = static_cast<UT>(value);
        }

        char digits[96]{};
        auto grouping = spec.grouping;
        if (effectiveType == format_spec::format_kind::grouped && grouping == format_spec::grouping_kind::none) {
            grouping = format_spec::grouping_kind::comma;
        }
        const auto digitsLen = write_grouped_digits(absValue, base, uppercase, grouping, effectiveType, digits);

        char prefix[3]{};
        std::size_t prefixLen = 0;
        if (spec.alternate_form) {
            switch (effectiveType) {
                case format_spec::format_kind::binary:
                    prefix[prefixLen++] = '0';
                    prefix[prefixLen++] = uppercase ? 'B' : 'b';
                    break;
                case format_spec::format_kind::octal:
                    prefix[prefixLen++] = '0';
                    prefix[prefixLen++] = 'o';
                    break;
                case format_spec::format_kind::hex_lower:
                    prefix[prefixLen++] = '0';
                    prefix[prefixLen++] = 'x';
                    break;
                case format_spec::format_kind::hex_upper:
                    prefix[prefixLen++] = '0';
                    prefix[prefixLen++] = 'X';
                    break;
                default: break;
            }
        }

        const char signChar = sign_char_for(negative, spec.sign);
        const std::size_t signLen = signChar == '\0' ? 0u : 1u;
        const auto align = effective_alignment(spec, true);
        const auto fill = effective_fill(spec, true);
        const std::size_t contentLen = signLen + prefixLen + digitsLen;
        const std::size_t totalLen = contentLen < spec.width ? spec.width : contentLen;
        const std::size_t pad = totalLen - contentLen;

        std::size_t leftPad = 0;
        std::size_t rightPad = 0;
        std::size_t middlePad = 0;

        switch (align) {
            case format_spec::align_kind::left: rightPad = pad; break;
            case format_spec::align_kind::center:
                leftPad = pad / 2;
                rightPad = pad - leftPad;
                break;
            case format_spec::align_kind::sign_aware: middlePad = pad; break;
            case format_spec::align_kind::right:
            case format_spec::align_kind::none:
            default: leftPad = pad; break;
        }

        std::size_t pos = 0;
        pos = append_repeated(out, pos, cap, fill, leftPad);
        if (signChar != '\0' && pos < cap) out[pos++] = signChar;
        pos = append_span(out, pos, cap, prefix, prefixLen);
        pos = append_repeated(out, pos, cap, fill, middlePad);
        pos = append_span(out, pos, cap, digits, digitsLen);
        pos = append_repeated(out, pos, cap, fill, rightPad);
        return pos;
    }
} // namespace gba::format::bits
