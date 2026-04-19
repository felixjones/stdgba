/// @file bits/text/text_format.hpp
/// @brief text-specific format extensions.

#pragma once

#include <gba/format>

#include <type_traits>

namespace gba::text {
    struct text_format_config {
        static inline constexpr unsigned int palette_type_hash = gba::format::fnv1a_hash<"pal">();

        static consteval bool parse_extension_type(const char* str, std::size_t& pos, std::size_t end,
                                                   format::format_spec& spec) {
            const auto len = end - pos;
            if (len != 3) return false;
            if (str[pos] != 'p' || str[pos + 1] != 'a' || str[pos + 2] != 'l') return false;
            spec.set_extension_type(palette_type_hash, 3);
            pos = end;
            return true;
        }

        static consteval bool validate(const format::format_spec& spec) {
            using format_spec = format::format_spec;
            if (spec.fmt_type != format_spec::format_kind::extension) return true;
            if (spec.extension_type_hash != palette_type_hash || spec.extension_type_len != 3) return false;
            return spec.alignment == format_spec::align_kind::none && spec.sign == format_spec::sign_kind::none &&
                   spec.grouping == format_spec::grouping_kind::none && spec.fill == ' ' && spec.width == 0 &&
                   spec.precision == 0 && !spec.has_precision && !spec.alternate_form && !spec.zero_pad;
        }

        template<typename T>
        static constexpr std::size_t render_extension(char* out, std::size_t cap, const T& value,
                                                      const format::format_spec& spec) {
            if (spec.extension_type_hash != palette_type_hash || spec.extension_type_len != 3) return 0;
            static_assert(std::is_integral_v<std::decay_t<T>>, "text palette formatting requires an integral value");
            if (cap > 0) out[0] = '\x1B';
            if (cap > 1) out[1] = static_cast<char>(value);
            return cap >= 2 ? 2u : cap;
        }
    };

    template<fixed_string Fmt>
    using text_format = format::compiled_format<Fmt, text_format_config>;

    template<fixed_string Fmt>
    consteval auto make_format() {
        return text_format<Fmt>{};
    }

    namespace literals {
        using gba::literals::operator""_fmt;
    }
} // namespace gba::text
