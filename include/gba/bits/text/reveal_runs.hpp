/// @file bits/text/reveal_runs.hpp
/// @brief Compile-time run metadata for text incremental reveal rendering.

#pragma once

#include <gba/bits/format/parse.hpp>
#include <gba/bits/text/text_format.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::text {
    enum class glyph_run_kind : std::uint8_t {
        literal,
        runtime,
    };

    struct glyph_run {
        glyph_run_kind kind = glyph_run_kind::literal;
        std::uint16_t lit_start = 0;
        std::uint16_t lit_length = 0;
        std::uint16_t literal_dense_start = 0;
        std::uint16_t literal_prefix_start = 0;
        std::uint8_t literal_index = 0xFFu;
        std::uint8_t positional_index = 0;
        unsigned int name_hash = 0;
    };

    template<gba::format::fixed_string Fmt, typename Config = text_format_config>
    struct compiled_reveal_runs {
        static constexpr auto ast = gba::format::parse_format<Fmt, Config>();
        static_assert(ast.valid, "Invalid format string");

        static constexpr std::size_t run_count = ast.segment_count;

        static constexpr std::size_t literal_run_count = [] {
            std::size_t n = 0;
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                if (ast.segments[i].type == gba::format::segment_type::literal) ++n;
            }
            return n;
        }();

        static constexpr std::size_t literal_char_count = [] {
            std::size_t n = 0;
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                if (ast.segments[i].type == gba::format::segment_type::literal) {
                    n += ast.segments[i].lit_length;
                }
            }
            return n;
        }();

        static constexpr std::size_t literal_prefix_entry_count = literal_char_count + literal_run_count;

        static constexpr auto runs = [] {
            std::array<glyph_run, gba::format::MAX_SEGMENTS> out{};
            std::size_t literal_index = 0;
            std::size_t literal_dense_start = 0;
            std::size_t literal_prefix_start = 0;
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                const auto& seg = ast.segments[i];
                if (seg.type == gba::format::segment_type::literal) {
                    out[i] = {
                        .kind = glyph_run_kind::literal,
                        .lit_start = seg.lit_start,
                        .lit_length = seg.lit_length,
                        .literal_dense_start = static_cast<std::uint16_t>(literal_dense_start),
                        .literal_prefix_start = static_cast<std::uint16_t>(literal_prefix_start),
                        .literal_index = static_cast<std::uint8_t>(literal_index),
                    };
                    ++literal_index;
                    literal_dense_start += seg.lit_length;
                    literal_prefix_start += seg.lit_length + 1;
                } else if (seg.type == gba::format::segment_type::positional_placeholder) {
                    out[i] = {
                        .kind = glyph_run_kind::runtime,
                        .positional_index = seg.pos_index,
                    };
                } else {
                    out[i] = {
                        .kind = glyph_run_kind::runtime,
                        .name_hash = seg.name_hash,
                    };
                }
            }
            return out;
        }();

        static constexpr auto literal_non_break_spans = [] {
            std::array<std::uint16_t, literal_char_count> out{};
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                const auto& run = runs[i];
                if (run.kind != glyph_run_kind::literal) continue;
                for (std::size_t j = run.lit_length; j > 0; --j) {
                    const auto local_idx = j - 1;
                    const auto dense_idx = static_cast<std::size_t>(run.literal_dense_start) + local_idx;
                    const char ch = ast.format_str.data[run.lit_start + local_idx];
                    const bool is_break = ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
                    if (is_break) {
                        out[dense_idx] = 0;
                        continue;
                    }
                    if (local_idx + 1 < run.lit_length) {
                        out[dense_idx] = static_cast<std::uint16_t>(out[dense_idx + 1] + 1);
                    } else {
                        out[dense_idx] = 1;
                    }
                }
            }
            return out;
        }();

        static constexpr std::size_t runtime_run_count = run_count - literal_run_count;
        static constexpr bool has_runtime_runs = (runtime_run_count != 0);
        static constexpr bool literal_only = (runtime_run_count == 0);

        static constexpr bool literal_has_escape_prefix = [] {
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                const auto& seg = ast.segments[i];
                if (seg.type != gba::format::segment_type::literal) continue;
                for (std::size_t j = 0; j < seg.lit_length; ++j) {
                    if (ast.format_str.data[seg.lit_start + j] == '\x1B') {
                        return true;
                    }
                }
            }
            return false;
        }();
    };

    template<gba::format::fixed_string Fmt, typename Config = text_format_config>
    consteval auto make_reveal_runs() {
        return compiled_reveal_runs<Fmt, Config>{};
    }
} // namespace gba::text
