/// @file bits/format/format_generator.hpp
/// @brief Typewriter generator implementation for gba::format.

#pragma once

#include <gba/bits/format/parse.hpp>
#include <gba/bits/format/render_value.hpp>

#include <array>
#include <cstddef>
#include <new>
#include <optional>
#include <tuple>
#include <utility>

namespace gba::format {
    struct arg_stream_state {
        std::array<char, 320> rendered{};
        std::size_t len = 0;
        std::size_t pos = 0;

        constexpr void reset() {
            len = 0;
            pos = 0;
        }

        [[nodiscard]] constexpr bool has_next() const { return pos < len; }

        constexpr char next() {
            if (pos >= len) return 0;
            return rendered[pos++];
        }
    };

    template<fixed_string Fmt, typename ArgPack, typename Config = default_format_config>
    struct format_generator {
        static constexpr auto ast = parse_format<Fmt, Config>();
        static constexpr std::size_t segment_count = ast.segment_count;

        ArgPack args;
        std::size_t segment_idx = 0;
        std::size_t char_idx = 0;
        arg_stream_state arg_state{};
        bool arg_initialized = false;

        constexpr format_generator(ArgPack a) : args{std::move(a)} {}

        format_generator(format_generator&&) = default;
        format_generator(const format_generator&) = default;

        format_generator& operator=(format_generator&& other) noexcept {
            if (this != &other) {
                this->~format_generator();
                ::new (this) format_generator(static_cast<format_generator&&>(other));
            }
            return *this;
        }

        format_generator& operator=(const format_generator& other) {
            if (this != &other) {
                this->~format_generator();
                ::new (this) format_generator(other);
            }
            return *this;
        }

        constexpr std::optional<char> operator()() { return next(); }

        constexpr std::optional<char> next() {
            while (segment_idx < segment_count) {
                const auto& seg = ast.segments[segment_idx];
                if (seg.type == segment_type::literal) {
                    if (char_idx < seg.lit_length) return ast.format_str.data[seg.lit_start + char_idx++];
                    advance_segment();
                } else {
                    auto ch = next_arg_char(seg);
                    if (ch) return ch;
                    advance_segment();
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr bool done() const { return segment_idx >= segment_count; }

        [[nodiscard]] constexpr std::size_t until_break() {
            if (done()) return 0;

            std::size_t count = 0;
            auto seg = segment_idx;
            auto idx = char_idx;

            while (seg < segment_count) {
                const auto& s = ast.segments[seg];
                if (s.type == segment_type::literal) {
                    while (idx < s.lit_length) {
                        const char c = ast.format_str.data[s.lit_start + idx];
                        if (bits::is_break_char(c)) return count;
                        ++count;
                        ++idx;
                    }
                } else {
                    if (seg == segment_idx && arg_initialized) {
                        for (std::size_t p = arg_state.pos; p < arg_state.len; ++p) {
                            if (bits::is_break_char(arg_state.rendered[p])) return count;
                            ++count;
                        }
                    } else {
                        arg_stream_state preview{};
                        init_preview_state(preview, s);
                        for (std::size_t p = 0; p < preview.len; ++p) {
                            if (bits::is_break_char(preview.rendered[p])) return count;
                            ++count;
                        }
                    }
                }
                ++seg;
                idx = 0;
            }
            return count;
        }

        constexpr void skip_literal_chars(std::size_t count) {
            if (count == 0 || done()) return;
            const auto& seg = ast.segments[segment_idx];
            if (seg.type != segment_type::literal) return;
            const auto remaining = (char_idx < seg.lit_length) ? (seg.lit_length - char_idx) : 0u;
            if (count > remaining) count = remaining;
            char_idx += count;
        }

        constexpr void reset() {
            segment_idx = 0;
            char_idx = 0;
            arg_state.reset();
            arg_initialized = false;
        }

    private:
        template<typename T>
        constexpr static void render_arg_into(arg_stream_state& out, const T& value, const format_spec& spec) {
            out.reset();
            out.len = bits::render_value<Config>(out.rendered.data(), out.rendered.size(), value, spec);
        }

        constexpr void advance_segment() {
            ++segment_idx;
            char_idx = 0;
            arg_state.reset();
            arg_initialized = false;
        }

        template<typename T>
        constexpr void init_arg_state(const T& value, const format_spec& spec) {
            render_arg_into(arg_state, value, spec);
            arg_initialized = true;
        }

        constexpr void init_preview_state(arg_stream_state& preview, const format_segment& seg) {
            if (seg.type == segment_type::named_placeholder) init_named_arg_into(preview, seg.name_hash, seg.spec);
            else init_positional_arg_into(preview, seg.pos_index, seg.spec);
        }

        constexpr std::optional<char> next_arg_char(const format_segment& seg) {
            if (!arg_initialized) {
                if (seg.type == segment_type::named_placeholder) init_named_arg(seg.name_hash, seg.spec);
                else init_positional_arg(seg.pos_index, seg.spec);
            }
            if (arg_state.has_next()) return arg_state.next();
            return std::nullopt;
        }

        template<typename Fn, std::size_t... Is>
        constexpr void visit_named_impl(unsigned int hash, Fn&& fn, std::index_sequence<Is...>) {
            (void)((std::tuple_element_t<Is, decltype(args.args)>::hash == hash ? (fn(std::get<Is>(args.args)), true)
                                                                                : false) ||
                   ...);
        }

        template<typename Fn>
        constexpr void visit_named_arg(unsigned int hash, Fn&& fn) {
            visit_named_impl(hash, std::forward<Fn>(fn),
                             std::make_index_sequence<std::tuple_size_v<decltype(args.args)>>{});
        }

        template<typename Fn, std::size_t... Is>
        constexpr void visit_positional_impl(std::uint8_t index, Fn&& fn, std::index_sequence<Is...>) {
            std::size_t pos = 0;
            (void)((pos++ == index ? (fn(std::get<Is>(args.args)), true) : false) || ...);
        }

        template<typename Fn>
        constexpr void visit_positional_arg(std::uint8_t index, Fn&& fn) {
            visit_positional_impl(index, std::forward<Fn>(fn),
                                  std::make_index_sequence<std::tuple_size_v<decltype(args.args)>>{});
        }

        constexpr void init_named_arg(unsigned int hash, const format_spec& spec) {
            visit_named_arg(hash, [&](const auto& arg) { init_arg_state(arg.get(), spec); });
        }

        constexpr void init_positional_arg(std::uint8_t index, const format_spec& spec) {
            visit_positional_arg(index, [&](const auto& arg) { init_arg_state(arg.get(), spec); });
        }

        constexpr void init_named_arg_into(arg_stream_state& out, unsigned int hash, const format_spec& spec) {
            visit_named_arg(hash, [&](const auto& arg) { render_arg_into(out, arg.get(), spec); });
        }

        constexpr void init_positional_arg_into(arg_stream_state& out, std::uint8_t index, const format_spec& spec) {
            visit_positional_arg(index, [&](const auto& arg) { render_arg_into(out, arg.get(), spec); });
        }
    };
} // namespace gba::format
