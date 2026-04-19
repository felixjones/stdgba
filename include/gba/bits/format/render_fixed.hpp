/// @file bits/format/render_fixed.hpp
/// @brief Internal fixed-point renderers for gba::format.

#pragma once

#include <gba/bits/format/render_numeric.hpp>
#include <gba/fixed_point>

#include <type_traits>

namespace gba::format::bits {
    template<fixed_point T>
    constexpr std::size_t render_fixed_point_value(char* out, std::size_t cap, T value, const format_spec& spec) {
        if (spec.fmt_type == format_spec::format_kind::percent) {
            auto percentValue = value * 100;
            format_spec adjusted = spec;
            adjusted.fmt_type = spec.has_precision ? format_spec::format_kind::fixed_lower
                                                   : format_spec::format_kind::default_fmt;
            const auto len = render_fixed_point_value(out, cap > 0 ? cap - 1 : 0, percentValue, adjusted);
            if (len < cap) {
                out[len] = '%';
                return len + 1;
            }
            return len;
        }

        if (is_scientific_type(spec.fmt_type) || is_general_type(spec.fmt_type)) {
            using traits = fixed_point_traits<T>;
            using rep = typename traits::rep;
            using urep = std::make_unsigned_t<rep>;
            const rep raw = gba::bit_cast(value);
            bool negative = false;
            urep absRaw{};
            if constexpr (std::is_signed_v<rep>) {
                if (raw < 0) {
                    negative = true;
                    absRaw = static_cast<urep>(0) - static_cast<urep>(raw);
                } else {
                    absRaw = static_cast<urep>(raw);
                }
            } else {
                absRaw = static_cast<urep>(raw);
            }
            constexpr auto fracBits = traits::frac_bits;
            return render_scientific_rational(out, cap, negative, static_cast<unsigned long long>(absRaw),
                                              static_cast<unsigned long long>(1) << fracBits, spec);
        }

        using traits = fixed_point_traits<T>;
        using rep = typename traits::rep;
        using urep = std::make_unsigned_t<rep>;

        const rep raw = gba::bit_cast(value);
        bool negative = false;
        urep absRaw{};
        if constexpr (std::is_signed_v<rep>) {
            if (raw < 0) {
                negative = true;
                absRaw = static_cast<urep>(0) - static_cast<urep>(raw);
            } else {
                absRaw = static_cast<urep>(raw);
            }
        } else {
            absRaw = static_cast<urep>(raw);
        }

        constexpr auto fracBits = traits::frac_bits;
        const auto integerPart = static_cast<unsigned long long>(absRaw >> fracBits);
        const auto fracMask = (static_cast<unsigned long long>(1) << fracBits) - 1ULL;
        const auto fractionalNumerator = static_cast<unsigned long long>(absRaw) & fracMask;
        return render_fixed_parts(out, cap, negative, integerPart, fractionalNumerator, fracBits, spec, false);
    }

    consteval std::size_t render_fixed_literal_value(char* out, std::size_t cap, literals::fixed_literal value,
                                                     const format_spec& spec) {
        auto v = value.value;
        bool appendPercent = false;
        if (spec.fmt_type == format_spec::format_kind::percent) {
            v *= 100.0;
            appendPercent = true;
        }

        if (is_scientific_type(spec.fmt_type) || is_general_type(spec.fmt_type)) {
            return render_scientific_real(out, cap, v, spec);
        }

        return render_real_value(out, cap, v, spec, 8u, appendPercent);
    }
} // namespace gba::format::bits
