/// @file bits/format/render_numeric.hpp
/// @brief Shared numeric rendering helpers for gba::format.

#pragma once

#include <gba/bits/format/render_common.hpp>

namespace gba::format::bits {
    struct numeric_layout {
        char sign_char = '\0';
        char fill = ' ';
        std::size_t left_pad = 0;
        std::size_t middle_pad = 0;
        std::size_t right_pad = 0;
    };

    constexpr format_spec::grouping_kind normalized_decimal_grouping(const format_spec& spec) {
        if (spec.fmt_type == format_spec::format_kind::grouped && spec.grouping == format_spec::grouping_kind::none) {
            return format_spec::grouping_kind::comma;
        }
        return spec.grouping;
    }

    constexpr numeric_layout make_numeric_layout(const format_spec& spec, bool negative, std::size_t contentLen) {
        const std::size_t totalLen = contentLen < spec.width ? spec.width : contentLen;
        const std::size_t pad = totalLen - contentLen;
        const auto align = effective_alignment(spec, true);

        numeric_layout layout{};
        layout.sign_char = sign_char_for(negative, spec.sign);
        layout.fill = effective_fill(spec, true);

        switch (align) {
            case format_spec::align_kind::left: layout.right_pad = pad; break;
            case format_spec::align_kind::center:
                layout.left_pad = pad / 2;
                layout.right_pad = pad - layout.left_pad;
                break;
            case format_spec::align_kind::sign_aware: layout.middle_pad = pad; break;
            case format_spec::align_kind::right:
            case format_spec::align_kind::none:
            default: layout.left_pad = pad; break;
        }

        return layout;
    }

    template<typename UInt>
    constexpr std::size_t render_decimal_integer_part(UInt value, const format_spec& spec, char* out) {
        return write_grouped_digits(value, 10, false, normalized_decimal_grouping(spec), format_spec::format_kind::decimal,
                                    out);
    }

    constexpr std::size_t append_signed_decimal_parts(char* out, std::size_t cap, const numeric_layout& layout,
                                                      const char* integerDigits, std::size_t integerLen,
                                                      bool showDecimal, const char* fractionalDigits,
                                                      std::size_t fractionalLen, bool appendPercent) {
        std::size_t pos = 0;
        pos = append_repeated(out, pos, cap, layout.fill, layout.left_pad);
        if (layout.sign_char != '\0' && pos < cap) out[pos++] = layout.sign_char;
        pos = append_repeated(out, pos, cap, layout.fill, layout.middle_pad);
        pos = append_span(out, pos, cap, integerDigits, integerLen);
        if (showDecimal && pos < cap) out[pos++] = '.';
        pos = append_span(out, pos, cap, fractionalDigits, fractionalLen);
        if (appendPercent && pos < cap) out[pos++] = '%';
        pos = append_repeated(out, pos, cap, layout.fill, layout.right_pad);
        return pos;
    }

    constexpr std::size_t append_signed_scientific_parts(char* out, std::size_t cap, const numeric_layout& layout,
                                                         unsigned int intDigit, bool showDecimal,
                                                         const char* fractionalDigits, std::size_t fractionalLen,
                                                         const char* expBuf, std::size_t expLen) {
        std::size_t pos = 0;
        pos = append_repeated(out, pos, cap, layout.fill, layout.left_pad);
        if (layout.sign_char != '\0' && pos < cap) out[pos++] = layout.sign_char;
        pos = append_repeated(out, pos, cap, layout.fill, layout.middle_pad);
        if (pos < cap) out[pos++] = static_cast<char>('0' + intDigit);
        if (showDecimal && pos < cap) out[pos++] = '.';
        pos = append_span(out, pos, cap, fractionalDigits, fractionalLen);
        pos = append_span(out, pos, cap, expBuf, expLen);
        pos = append_repeated(out, pos, cap, layout.fill, layout.right_pad);
        return pos;
    }

    constexpr std::size_t build_scientific_exponent(char* expBuf, bool uppercase, int exponent) {
        std::size_t expLen = 0;
        expBuf[expLen++] = uppercase ? 'E' : 'e';
        expBuf[expLen++] = exponent >= 0 ? '+' : '-';

        const auto absExp = exponent >= 0 ? static_cast<unsigned int>(exponent)
                                          : static_cast<unsigned int>(-(exponent + 1)) + 1u;
        if (absExp < 10u) {
            expBuf[expLen++] = '0';
            expBuf[expLen++] = static_cast<char>('0' + absExp);
            return expLen;
        }

        char expDigits[10]{};
        std::size_t expDigitLen = 0;
        auto remaining = absExp;
        while (remaining > 0) {
            expDigits[expDigitLen++] = static_cast<char>('0' + remaining % 10u);
            remaining /= 10u;
        }
        for (std::size_t i = expDigitLen; i > 0; --i) expBuf[expLen++] = expDigits[i - 1];
        return expLen;
    }

    consteval std::size_t render_real_value(char* out, std::size_t cap, double v, const format_spec& spec,
                                            std::size_t defaultPrecision, bool appendPercent);

    consteval std::size_t render_scientific_real(char* out, std::size_t cap, double v, const format_spec& spec) {
        const bool uppercase = is_scientific_uppercase(spec.fmt_type);
        const bool isGeneral = is_general_type(spec.fmt_type);
        const std::size_t defaultPrec = 6;
        std::size_t precision = spec.has_precision ? spec.precision : defaultPrec;

        bool negative = false;
        if (v < 0.0) {
            negative = true;
            v = -v;
        }

        if (isGeneral && precision == 0) precision = 1;

        int exponent = 0;
        double normalized = v;
        if (v != 0.0) {
            while (normalized >= 10.0) {
                normalized /= 10.0;
                ++exponent;
            }
            while (normalized < 1.0) {
                normalized *= 10.0;
                --exponent;
            }
        }

        if (isGeneral) {
            if (exponent >= -4 && exponent < static_cast<int>(precision)) {
                format_spec adjusted = spec;
                adjusted.fmt_type = uppercase ? format_spec::format_kind::fixed_upper : format_spec::format_kind::fixed_lower;
                const int fracDigits = static_cast<int>(precision) - 1 - exponent;
                if (fracDigits > 0) {
                    adjusted.has_precision = true;
                    adjusted.precision = static_cast<std::uint8_t>(fracDigits);
                } else {
                    adjusted.has_precision = false;
                }
                return render_real_value(out, cap, negative ? -v : v, adjusted, defaultPrec, false);
            }
            precision = precision > 0 ? precision - 1 : 0;
        }

        std::size_t fracLen = precision;
        char fracDigits[96]{};
        double frac = normalized;
        const auto intDigit = static_cast<unsigned int>(frac);
        frac -= static_cast<double>(intDigit);
        for (std::size_t i = 0; i < fracLen; ++i) {
            frac *= 10.0;
            const auto d = static_cast<unsigned int>(frac);
            fracDigits[i] = static_cast<char>('0' + d);
            frac -= static_cast<double>(d);
        }

        if (!spec.has_precision && isGeneral) fracLen = trim_trailing_zeros(fracDigits, fracLen);

        const bool showDecimal = fracLen > 0 || spec.alternate_form || (spec.has_precision && precision > 0);

        char expBuf[12]{};
        const auto expLen = build_scientific_exponent(expBuf, uppercase, exponent);

        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(spec, negative,
                                                signLen + 1u + (showDecimal ? 1u : 0u) + fracLen + expLen);
        return append_signed_scientific_parts(out, cap, layout, intDigit, showDecimal, fracDigits, fracLen, expBuf,
                                              expLen);
    }

    template<bool Runtime>
    constexpr std::size_t render_fixed_parts_impl(char* out, std::size_t cap, bool negative, unsigned long long integerPart,
                                                  unsigned long long fractionalNumerator, unsigned int fracBits,
                                                  const format_spec& spec, bool appendPercent) {
        char integerDigits[96]{};
        const auto integerLen = render_decimal_integer_part(integerPart, spec, integerDigits);

        std::size_t fractionalLen = spec.has_precision ? spec.precision : static_cast<std::size_t>(fracBits);
        char fractionalDigits[96]{};
        const auto denom = 1ULL << fracBits;
        auto remainder = fractionalNumerator;
        if constexpr (Runtime) {
            if (fractionalLen > 0 && fracBits > 0 && fracBits <= 16) {
                _stdgba_fixed_frac_digits_u16(static_cast<std::uint32_t>(fractionalNumerator), fracBits, fractionalDigits,
                                              fractionalLen);
            } else {
                for (std::size_t i = 0; i < fractionalLen; ++i) {
                    remainder *= 10ULL;
                    fractionalDigits[i] = static_cast<char>('0' + (remainder / denom));
                    remainder %= denom;
                }
            }
        } else {
            for (std::size_t i = 0; i < fractionalLen; ++i) {
                remainder *= 10ULL;
                fractionalDigits[i] = static_cast<char>('0' + (remainder / denom));
                remainder %= denom;
            }
        }

        if (!spec.has_precision) fractionalLen = trim_trailing_zeros(fractionalDigits, fractionalLen);

        const bool showDecimal = fractionalLen > 0 || spec.alternate_form || (spec.has_precision && spec.precision > 0);
        const std::size_t suffixLen = appendPercent ? 1u : 0u;
        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(
            spec, negative, signLen + integerLen + (showDecimal ? 1u : 0u) + fractionalLen + suffixLen);
        return append_signed_decimal_parts(out, cap, layout, integerDigits, integerLen, showDecimal, fractionalDigits,
                                           fractionalLen, appendPercent);
    }

    constexpr std::size_t render_fixed_parts(char* out, std::size_t cap, bool negative, unsigned long long integerPart,
                                             unsigned long long fractionalNumerator, unsigned int fracBits,
                                             const format_spec& spec, bool appendPercent) {
        if (std::is_constant_evaluated()) {
            return render_fixed_parts_impl<false>(out, cap, negative, integerPart, fractionalNumerator, fracBits, spec, appendPercent);
        } else {
            return render_fixed_parts_impl<true>(out, cap, negative, integerPart, fractionalNumerator, fracBits, spec, appendPercent);
        }
    }

    template<bool Runtime>
    constexpr std::size_t render_fraction_parts_u32_impl(char* out, std::size_t cap, bool negative,
                                                        unsigned long long integerPart, std::uint32_t fractionalNumerator,
                                                        const format_spec& spec, std::size_t defaultPrecision,
                                                        bool appendPercent) {
        char integerDigits[96]{};
        const auto integerLen = render_decimal_integer_part(integerPart, spec, integerDigits);

        std::size_t fractionalLen = spec.has_precision ? spec.precision : defaultPrecision;
        char fractionalDigits[96]{};
        auto remainder = fractionalNumerator;
        if constexpr (Runtime) {
            if (fractionalLen > 0) {
                _stdgba_frac_digits_u32(remainder, fractionalDigits, fractionalLen);
            }
        } else {
            for (std::size_t i = 0; i < fractionalLen; ++i) {
                const auto scaled = static_cast<unsigned long long>(remainder) * 10ULL;
                fractionalDigits[i] = static_cast<char>('0' + (scaled >> 32));
                remainder = static_cast<std::uint32_t>(scaled);
            }
        }

        if (!spec.has_precision) fractionalLen = trim_trailing_zeros(fractionalDigits, fractionalLen);

        const bool showDecimal = fractionalLen > 0 || spec.alternate_form || (spec.has_precision && spec.precision > 0);
        const std::size_t suffixLen = appendPercent ? 1u : 0u;
        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(
            spec, negative, signLen + integerLen + (showDecimal ? 1u : 0u) + fractionalLen + suffixLen);
        return append_signed_decimal_parts(out, cap, layout, integerDigits, integerLen, showDecimal, fractionalDigits,
                                           fractionalLen, appendPercent);
    }

    constexpr std::size_t render_fraction_parts_u32(char* out, std::size_t cap, bool negative,
                                                    unsigned long long integerPart, std::uint32_t fractionalNumerator,
                                                    const format_spec& spec, std::size_t defaultPrecision,
                                                    bool appendPercent) {
        if (std::is_constant_evaluated()) {
            return render_fraction_parts_u32_impl<false>(out, cap, negative, integerPart, fractionalNumerator, spec, defaultPrecision, appendPercent);
        } else {
            return render_fraction_parts_u32_impl<true>(out, cap, negative, integerPart, fractionalNumerator, spec, defaultPrecision, appendPercent);
        }
    }

    template<bool Runtime>
    constexpr std::size_t render_fraction_parts_generic_impl(char* out, std::size_t cap, bool negative,
                                                            unsigned long long integerPart,
                                                            unsigned long long fractionalNumerator,
                                                            unsigned long long denominator, const format_spec& spec,
                                                            std::size_t defaultPrecision, bool appendPercent) {
        char integerDigits[96]{};
        const auto integerLen = render_decimal_integer_part(integerPart, spec, integerDigits);

        std::size_t fractionalLen = spec.has_precision ? spec.precision : defaultPrecision;
        char fractionalDigits[96]{};
        if constexpr (Runtime) {
            if (fractionalLen > 0 && denominator <= 0x19999999u) {
                auto rem32 = static_cast<std::uint32_t>(fractionalNumerator);
                const auto den32 = static_cast<std::uint32_t>(denominator);
                for (std::size_t i = 0; i < fractionalLen; ++i) {
                    rem32 *= 10u;
                    fractionalDigits[i] = static_cast<char>('0' + rem32 / den32);
                    rem32 %= den32;
                }
            } else {
                auto remainder = fractionalNumerator;
                for (std::size_t i = 0; i < fractionalLen; ++i) {
                    remainder *= 10ULL;
                    fractionalDigits[i] = static_cast<char>('0' + (remainder / denominator));
                    remainder %= denominator;
                }
            }
        } else {
            auto remainder = fractionalNumerator;
            for (std::size_t i = 0; i < fractionalLen; ++i) {
                remainder *= 10ULL;
                fractionalDigits[i] = static_cast<char>('0' + (remainder / denominator));
                remainder %= denominator;
            }
        }

        if (!spec.has_precision) fractionalLen = trim_trailing_zeros(fractionalDigits, fractionalLen);

        const bool showDecimal = fractionalLen > 0 || spec.alternate_form || (spec.has_precision && spec.precision > 0);
        const std::size_t suffixLen = appendPercent ? 1u : 0u;
        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(
            spec, negative, signLen + integerLen + (showDecimal ? 1u : 0u) + fractionalLen + suffixLen);
        return append_signed_decimal_parts(out, cap, layout, integerDigits, integerLen, showDecimal, fractionalDigits,
                                           fractionalLen, appendPercent);
    }

    constexpr std::size_t render_fraction_parts_generic(char* out, std::size_t cap, bool negative,
                                                        unsigned long long integerPart,
                                                        unsigned long long fractionalNumerator,
                                                        unsigned long long denominator, const format_spec& spec,
                                                        std::size_t defaultPrecision, bool appendPercent) {
        if (std::is_constant_evaluated()) {
            return render_fraction_parts_generic_impl<false>(out, cap, negative, integerPart, fractionalNumerator, denominator, spec, defaultPrecision, appendPercent);
        } else {
            return render_fraction_parts_generic_impl<true>(out, cap, negative, integerPart, fractionalNumerator, denominator, spec, defaultPrecision, appendPercent);
        }
    }

    template<bool Runtime>
    constexpr std::size_t render_scientific_rational_impl(char* out, std::size_t cap, bool negative,
                                                         unsigned long long numerator, unsigned long long denominator,
                                                         const format_spec& spec) {
        const bool uppercase = is_scientific_uppercase(spec.fmt_type);
        const bool isGeneral = is_general_type(spec.fmt_type);
        const std::size_t defaultPrec = 6;
        std::size_t precision = spec.has_precision ? spec.precision : defaultPrec;

        if (isGeneral && precision == 0) precision = 1;

        if (numerator == 0) {
            if (isGeneral) {
                format_spec adjusted = spec;
                adjusted.fmt_type = uppercase ? format_spec::format_kind::fixed_upper : format_spec::format_kind::fixed_lower;
                adjusted.has_precision = false;
                return render_fraction_parts_generic_impl<Runtime>(out, cap, negative, 0u, 0u, denominator, adjusted, 0u, false);
            }
        }

        int exponent = 0;
        auto normNum = numerator;
        auto normDen = denominator;
        while (normNum >= normDen * 10ULL) {
            normDen *= 10ULL;
            ++exponent;
        }
        while (normNum < normDen) {
            normNum *= 10ULL;
            --exponent;
        }

        if (isGeneral) {
            if (exponent >= -4 && exponent < static_cast<int>(precision)) {
                format_spec adjusted = spec;
                adjusted.fmt_type = uppercase ? format_spec::format_kind::fixed_upper : format_spec::format_kind::fixed_lower;
                const int fracDigits = static_cast<int>(precision) - 1 - exponent;
                if (fracDigits > 0) {
                    adjusted.has_precision = true;
                    adjusted.precision = static_cast<std::uint8_t>(fracDigits);
                } else {
                    adjusted.has_precision = false;
                }
                if constexpr (Runtime) {
                    if (denominator <= 0xFFFFFFFFu && numerator <= 0xFFFFFFFFu) {
                        const auto d32 = static_cast<std::uint32_t>(denominator);
                        const auto n32 = static_cast<std::uint32_t>(numerator);
                        return render_fraction_parts_generic_impl<Runtime>(out, cap, negative, n32 / d32, n32 % d32, denominator,
                                                                           adjusted, defaultPrec, false);
                    }
                }
                return render_fraction_parts_generic_impl<Runtime>(out, cap, negative, numerator / denominator,
                                                                   numerator % denominator, denominator, adjusted, defaultPrec,
                                                                   false);
            }
            precision = precision > 0 ? precision - 1 : 0;
        }

        std::size_t fracLen = precision;
        char fracDigits[96]{};
        unsigned int intDigit{};
        if constexpr (Runtime) {
            if (normDen <= 0x19999999u && normNum <= 0xFFFFFFFFu) {
                const auto nd = static_cast<std::uint32_t>(normDen);
                auto nn = static_cast<std::uint32_t>(normNum);
                intDigit = nn / nd;
                auto rem32 = nn % nd;
                for (std::size_t i = 0; i < fracLen; ++i) {
                    rem32 *= 10u;
                    fracDigits[i] = static_cast<char>('0' + rem32 / nd);
                    rem32 %= nd;
                }
            } else {
                intDigit = static_cast<unsigned int>(normNum / normDen);
                auto remainder = normNum % normDen;
                for (std::size_t i = 0; i < fracLen; ++i) {
                    remainder *= 10ULL;
                    fracDigits[i] = static_cast<char>('0' + (remainder / normDen));
                    remainder %= normDen;
                }
            }
        } else {
            intDigit = static_cast<unsigned int>(normNum / normDen);
            auto remainder = normNum % normDen;
            for (std::size_t i = 0; i < fracLen; ++i) {
                remainder *= 10ULL;
                fracDigits[i] = static_cast<char>('0' + (remainder / normDen));
                remainder %= normDen;
            }
        }

        if (!spec.has_precision && isGeneral) fracLen = trim_trailing_zeros(fracDigits, fracLen);

        const bool showDecimal = fracLen > 0 || spec.alternate_form || (spec.has_precision && precision > 0);

        char expBuf[12]{};
        const auto expLen = build_scientific_exponent(expBuf, uppercase, exponent);

        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(spec, negative,
                                                signLen + 1u + (showDecimal ? 1u : 0u) + fracLen + expLen);
        return append_signed_scientific_parts(out, cap, layout, intDigit, showDecimal, fracDigits, fracLen, expBuf,
                                              expLen);
    }

    constexpr std::size_t render_scientific_rational(char* out, std::size_t cap, bool negative,
                                                     unsigned long long numerator, unsigned long long denominator,
                                                     const format_spec& spec) {
        if (std::is_constant_evaluated()) {
            return render_scientific_rational_impl<false>(out, cap, negative, numerator, denominator, spec);
        } else {
            return render_scientific_rational_impl<true>(out, cap, negative, numerator, denominator, spec);
        }
    }

    consteval std::size_t render_real_value(char* out, std::size_t cap, double v, const format_spec& spec,
                                            std::size_t defaultPrecision, bool appendPercent) {
        bool negative = false;
        if (v < 0.0) {
            negative = true;
            v = -v;
        }

        const auto integerPart = static_cast<unsigned long long>(v);
        auto frac = v - static_cast<double>(integerPart);
        std::size_t fractionalLen = spec.has_precision ? spec.precision : defaultPrecision;
        char fractionalDigits[96]{};
        for (std::size_t i = 0; i < fractionalLen; ++i) {
            frac *= 10.0;
            const auto digit = static_cast<unsigned int>(frac);
            fractionalDigits[i] = static_cast<char>('0' + digit);
            frac -= static_cast<double>(digit);
        }

        if (!spec.has_precision) fractionalLen = trim_trailing_zeros(fractionalDigits, fractionalLen);

        char integerDigits[96]{};
        const auto integerLen = render_decimal_integer_part(integerPart, spec, integerDigits);
        const bool showDecimal = fractionalLen > 0 || spec.alternate_form || (spec.has_precision && spec.precision > 0);
        const std::size_t suffixLen = appendPercent ? 1u : 0u;
        const auto signLen = sign_char_for(negative, spec.sign) == '\0' ? 0u : 1u;
        const auto layout = make_numeric_layout(
            spec, negative, signLen + integerLen + (showDecimal ? 1u : 0u) + fractionalLen + suffixLen);
        return append_signed_decimal_parts(out, cap, layout, integerDigits, integerLen, showDecimal, fractionalDigits,
                                           fractionalLen, appendPercent);
    }
} // namespace gba::format::bits
