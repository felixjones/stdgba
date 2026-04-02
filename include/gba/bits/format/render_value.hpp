/// @file bits/format/render_value.hpp
/// @brief Internal value renderers for gba::format.

#pragma once

#include <gba/bits/format/render_angle.hpp>
#include <gba/bits/format/render_fixed.hpp>
#include <gba/bits/format/render_scalar.hpp>

#include <type_traits>

namespace gba::format::bits {

    template<typename T>
    constexpr std::size_t render_value(char* out, std::size_t cap, const T& value, const format_spec& spec) {
        if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
            return render_string_value(out, cap, value, spec);
        } else if constexpr (std::is_integral_v<T>) {
            return render_integer_value(out, cap, value, spec);
        } else if constexpr (::gba::fixed_point<std::decay_t<T>>) {
            return render_fixed_point_value(out, cap, value, spec);
        } else if constexpr (std::is_same_v<std::decay_t<T>, literals::angle_literal>) {
            return render_angle_value(out, cap, static_cast<angle>(value), spec);
        } else if constexpr (::gba::AnyAngleType<std::decay_t<T>>) {
            return render_angle_value(out, cap, value, spec);
        } else {
            static_assert(dependent_false_v<T>, "Formatting for this value category is not implemented yet");
            return 0;
        }
    }
} // namespace gba::format::bits
