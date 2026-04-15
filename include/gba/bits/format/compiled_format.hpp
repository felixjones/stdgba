/// @file bits/format/compiled_format.hpp
/// @brief Compiled format object for gba::format.

#pragma once

#include <gba/args>
#include <gba/bits/format/format_generator.hpp>
#include <gba/bits/format/workspace.hpp>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace gba::format {
    template<fixed_string Fmt>
    struct compiled_format {
        static constexpr auto fmt = Fmt;
        static constexpr auto ast = parse_format<Fmt>();
        static_assert(ast.valid, "Invalid format string");

        template<typename... Args>
        constexpr auto generator(Args... args) const {
            using pack_t = arg_pack<Args...>;
            return format_generator<Fmt, pack_t>{pack_t{std::move(args)...}};
        }

        template<typename... Args>
        constexpr auto generator_with(external_workspace ws, Args... args) const {
            (void)ws;
            return generator(std::move(args)...);
        }

        template<typename... Args>
        constexpr std::size_t to(char* buf, Args... args) const {
            auto gen = generator(std::move(args)...);
            char* start = buf;
            while (auto ch = gen()) *buf++ = *ch;
            *buf = '\0';
            return static_cast<std::size_t>(buf - start);
        }

        template<std::size_t N = 64, typename... Args>
        constexpr auto to_array(Args... args) const {
            std::array<char, N> result{};
            to(result.data(), std::move(args)...);
            return result;
        }

        template<std::size_t N = 64, typename... Args>
        static consteval auto to_static(Args... args) {
            std::array<char, N> result{};
            std::size_t pos = 0;

            for (std::size_t i = 0; i < ast.segment_count && pos < N - 1; ++i) {
                const auto& seg = ast.segments[i];

                if (seg.type == segment_type::literal) {
                    append_literal_segment(result, pos, seg);
                } else if (seg.type == segment_type::named_placeholder) {
                    pos = format_named_arg_static<N>(result, pos, seg.name_hash, seg.spec, args...);
                } else if (seg.type == segment_type::positional_placeholder) {
                    pos = format_positional_arg_static<N>(result, pos, seg.pos_index, seg.spec, args...);
                }
            }
            result[pos] = '\0';
            return result;
        }

        template<typename... Args>
        constexpr auto operator()(Args... args) const {
            return to_array(std::move(args)...);
        }

    private:
        template<std::size_t N>
        static consteval void append_literal_segment(std::array<char, N>& out, std::size_t& pos,
                                                     const format_segment& seg) {
            for (std::size_t j = 0; j < seg.lit_length && pos < N - 1; ++j) {
                out[pos++] = ast.format_str.data[seg.lit_start + j];
            }
        }

        template<std::size_t N, typename Predicate>
        static consteval std::size_t format_matching_arg_static(std::array<char, N>&, std::size_t pos,
                                                                const format_spec&, Predicate) {
            return pos;
        }

        template<std::size_t N, typename Predicate, typename First, typename... Rest>
        static consteval std::size_t format_matching_arg_static(std::array<char, N>& out, std::size_t pos,
                                                                const format_spec& spec, Predicate predicate,
                                                                First first, Rest... rest) {
            if (predicate(first)) return format_value_static<N>(out, pos, first.stored, spec);
            if constexpr (sizeof...(Rest) > 0) {
                return format_matching_arg_static<N>(out, pos, spec, predicate, rest...);
            }
            return pos;
        }

        template<std::size_t N, typename... Args>
        static consteval std::size_t format_named_arg_static(std::array<char, N>& out, std::size_t pos,
                                                             unsigned int hash, const format_spec& spec, Args... args) {
            return format_matching_arg_static<N>(out, pos, spec,
                                                 [hash](const auto& arg) {
                                                     using arg_t = std::decay_t<decltype(arg)>;
                                                     return arg_t::hash == hash;
                                                 },
                                                 args...);
        }

        template<std::size_t N, typename... Args>
        static consteval std::size_t format_positional_arg_static(std::array<char, N>& out, std::size_t pos,
                                                                  std::uint8_t index, const format_spec& spec,
                                                                  Args... args) {
            std::uint8_t current = 0;
            return format_matching_arg_static<N>(out, pos, spec,
                                                 [index, &current](const auto&) mutable { return current++ == index; },
                                                 args...);
        }

        template<std::size_t N>
        static consteval std::size_t format_value_static(std::array<char, N>& out, std::size_t pos, const auto& value,
                                                         const format_spec& spec) {
            char tmp[320]{};
            std::size_t len = 0;
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, literals::fixed_literal>) {
                len = bits::render_fixed_literal_value(tmp, sizeof(tmp), value, spec);
            } else {
                len = bits::render_value(tmp, sizeof(tmp), value, spec);
            }
            for (std::size_t i = 0; i < len && pos < N - 1; ++i) out[pos++] = tmp[i];
            return pos;
        }
    };
} // namespace gba::format
