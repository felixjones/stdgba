/// @file bits/text2/reveal_runs.hpp
/// @brief Compile-time run metadata for text2 incremental reveal rendering.

#pragma once

#include <gba/bits/format/parse.hpp>
#include <gba/bits/text2/text2_format.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::text2 {
    enum class glyph_run_kind : std::uint8_t {
        literal,
        runtime,
    };

    struct glyph_run {
        glyph_run_kind kind = glyph_run_kind::literal;
        std::uint16_t lit_start = 0;
        std::uint16_t lit_length = 0;
        std::uint8_t positional_index = 0;
        unsigned int name_hash = 0;
    };

    template<gba::format::fixed_string Fmt, typename Config = text2_format_config>
    struct compiled_reveal_runs {
        static constexpr auto ast = gba::format::parse_format<Fmt, Config>();
        static_assert(ast.valid, "Invalid format string");

        static constexpr std::size_t run_count = ast.segment_count;

        static constexpr auto runs = [] {
            std::array<glyph_run, gba::format::MAX_SEGMENTS> out{};
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                const auto& seg = ast.segments[i];
                if (seg.type == gba::format::segment_type::literal) {
                    out[i] = {
                        .kind = glyph_run_kind::literal,
                        .lit_start = seg.lit_start,
                        .lit_length = seg.lit_length,
                    };
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

        static constexpr std::size_t literal_run_count = [] {
            std::size_t n = 0;
            for (std::size_t i = 0; i < ast.segment_count; ++i) {
                if (ast.segments[i].type == gba::format::segment_type::literal) ++n;
            }
            return n;
        }();

        static constexpr std::size_t runtime_run_count = run_count - literal_run_count;
        static constexpr bool has_runtime_runs = (runtime_run_count != 0);
        static constexpr bool literal_only = (runtime_run_count == 0);
    };

    template<gba::format::fixed_string Fmt, typename Config = text2_format_config>
    consteval auto make_reveal_runs() {
        return compiled_reveal_runs<Fmt, Config>{};
    }
} // namespace gba::text2
