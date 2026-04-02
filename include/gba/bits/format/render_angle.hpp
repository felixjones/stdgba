/// @file bits/format/render_angle.hpp
/// @brief Internal angle renderers for gba::format.

#pragma once

#include <gba/angle>
#include <gba/bits/format/render_numeric.hpp>
#include <gba/bits/format/render_scalar.hpp>

#include <type_traits>

namespace gba::format::bits {
    template<typename T>
    constexpr unsigned long long angle_raw_storage(T value) {
        return static_cast<unsigned long long>(gba::bit_cast(value));
    }

    template<typename T>
    constexpr std::size_t angle_native_hex_digits() {
        if constexpr (::gba::is_angle_v<std::decay_t<T>>) return 8;
        else return (std::decay_t<T>::bits + 3u) / 4u;
    }

    template<typename T>
    constexpr std::size_t render_angle_hex_value(char* out, std::size_t cap, T value, const format_spec& spec) {
        const auto uppercase = spec.fmt_type == format_spec::type_t::hex_upper;
        const auto raw = angle_raw_storage(value);
        const auto nativeDigits = angle_native_hex_digits<T>();
        const auto requestedDigits = spec.has_precision ? static_cast<std::size_t>(spec.precision) : nativeDigits;
        const auto shownDigits = requestedDigits == 0 ? 1u : requestedDigits;

        unsigned long long shownValue = raw;
        if (shownDigits < nativeDigits) shownValue >>= (nativeDigits - shownDigits) * 4u;

        char digits[96]{};
        for (std::size_t i = 0; i < shownDigits; ++i) {
            const auto shift = static_cast<unsigned int>((shownDigits - 1u - i) * 4u);
            const auto nibble = static_cast<unsigned int>((shownValue >> shift) & 0xFu);
            digits[i] = nibble < 10 ? static_cast<char>('0' + nibble)
                                    : static_cast<char>((uppercase ? 'A' : 'a') + nibble - 10);
        }

        char groupedDigits[120]{};
        std::size_t digitsLen = 0;
        if (spec.grouping == format_spec::grouping_t::none) {
            for (std::size_t i = 0; i < shownDigits; ++i) groupedDigits[digitsLen++] = digits[i];
        } else {
            const auto separator = grouping_char(spec.grouping);
            for (std::size_t i = 0; i < shownDigits; ++i) {
                groupedDigits[digitsLen++] = digits[i];
                const auto remaining = shownDigits - i - 1u;
                if (remaining > 0 && (remaining % 4u) == 0u) groupedDigits[digitsLen++] = separator;
            }
        }

        char prefix[2]{};
        std::size_t prefixLen = 0;
        if (spec.alternate_form) {
            prefix[prefixLen++] = '0';
            prefix[prefixLen++] = uppercase ? 'X' : 'x';
        }

        const auto align = effective_alignment(spec, true);
        const auto fill = effective_fill(spec, true);
        const std::size_t contentLen = prefixLen + digitsLen;
        const std::size_t totalLen = contentLen < spec.width ? spec.width : contentLen;
        const std::size_t pad = totalLen - contentLen;
        std::size_t leftPad = 0;
        std::size_t rightPad = 0;
        std::size_t middlePad = 0;

        switch (align) {
            case format_spec::align_t::left: rightPad = pad; break;
            case format_spec::align_t::center:
                leftPad = pad / 2u;
                rightPad = pad - leftPad;
                break;
            case format_spec::align_t::sign_aware: middlePad = pad; break;
            case format_spec::align_t::right:
            case format_spec::align_t::none:
            default: leftPad = pad; break;
        }

        std::size_t pos = 0;
        pos = append_repeated(out, pos, cap, fill, leftPad);
        pos = append_span(out, pos, cap, prefix, prefixLen);
        pos = append_repeated(out, pos, cap, fill, middlePad);
        pos = append_span(out, pos, cap, groupedDigits, digitsLen);
        pos = append_repeated(out, pos, cap, fill, rightPad);
        return pos;
    }

    template<typename T, bool Runtime>
    constexpr std::size_t render_angle_value_impl(char* out, std::size_t cap, T value, const format_spec& spec) {
        if (spec.fmt_type == format_spec::type_t::raw_int) {
            format_spec adjusted = spec;
            adjusted.fmt_type = format_spec::type_t::decimal;
            return render_integer_value(out, cap, angle_raw_storage(value), adjusted);
        }
        if (spec.fmt_type == format_spec::type_t::hex_lower || spec.fmt_type == format_spec::type_t::hex_upper) {
            return render_angle_hex_value(out, cap, value, spec);
        }

        const auto raw32 = bit_cast(static_cast<angle>(value));
        if (spec.fmt_type == format_spec::type_t::default_fmt || spec.fmt_type == format_spec::type_t::decimal) {
            const auto scaled = static_cast<unsigned long long>(raw32) * 360ULL;
            const auto integerPart = scaled >> 32;
            const auto fractionalNumerator = static_cast<std::uint32_t>(scaled);
            return render_fraction_parts_u32(out, cap, false, integerPart, fractionalNumerator, spec, 6u, false);
        }
        if (spec.fmt_type == format_spec::type_t::turns) {
            return render_fraction_parts_u32(out, cap, false, 0u, raw32, spec, 6u, false);
        }
        if (spec.fmt_type == format_spec::type_t::radians) {
            if constexpr (!Runtime) {
                constexpr unsigned long long two_pi_scaled = 628318530ULL;
                constexpr unsigned long long turns_den_scaled = 4294967296ULL * 100000000ULL;
                const auto numerator = static_cast<unsigned long long>(raw32) * two_pi_scaled;
                return render_fraction_parts_generic(out, cap, false, numerator / turns_den_scaled,
                                                     numerator % turns_den_scaled, turns_den_scaled, spec, 6u, false);
            } else {
                constexpr std::uint32_t two_pi_frac = 1216263846u;
                const auto p1 = static_cast<unsigned long long>(raw32) * 6ULL;
                const auto p2 = static_cast<unsigned long long>(raw32) * static_cast<unsigned long long>(two_pi_frac);
                const auto combined = p1 + (p2 >> 32);
                return render_fraction_parts_u32(out, cap, false, combined >> 32, static_cast<std::uint32_t>(combined),
                                                 spec, 6u, false);
            }
        }
        return render_fraction_parts_u32(out, cap, false, 0u, raw32, spec, 6u, false);
    }

    template<typename T>
    constexpr std::size_t render_angle_value(char* out, std::size_t cap, T value, const format_spec& spec) {
        if (std::is_constant_evaluated()) {
            return render_angle_value_impl<T, false>(out, cap, value, spec);
        } else {
            return render_angle_value_impl<T, true>(out, cap, value, spec);
        }
    }
} // namespace gba::format::bits
