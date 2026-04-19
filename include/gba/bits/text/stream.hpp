/// @file bits/text/stream.hpp
/// @brief text unified color-escape tokenizer and stream types.

#pragma once

#include <gba/bits/format/compiled_format.hpp>
#include <gba/bits/text/reveal_runs.hpp>
#include <gba/bits/text/text_format.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace gba::text {
    inline constexpr char color_escape_prefix = '\x1B';

    struct no_reveal_runs {
        static constexpr bool has_runtime_runs = true;
        static constexpr bool literal_only = false;
        static constexpr bool literal_has_escape_prefix = true;
    };

    template<typename Generator>
    struct generator_reveal_runs {
        using type = no_reveal_runs;
        static constexpr bool available = false;
    };

    template<gba::format::fixed_string Fmt, typename ArgPack, typename Config>
    struct generator_reveal_runs<gba::format::format_generator<Fmt, ArgPack, Config>> {
        using type = compiled_reveal_runs<Fmt, Config>;
        static constexpr bool available = true;
    };

    [[nodiscard]]
    constexpr bool is_color_escape_prefix(char c) noexcept {
        return c == color_escape_prefix;
    }

    [[nodiscard]]
    constexpr unsigned char decode_color_escape_nibble(char c) noexcept {
        if (c >= '1' && c <= '9') return static_cast<unsigned char>(c - '0');
        if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(10 + (c - 'A'));
        if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(10 + (c - 'a'));
        return 0;
    }

    struct color_token {
        unsigned char palette_index;
    };

    struct char_token {
        unsigned int codepoint;
    };

    struct literal_run_cursor {
        std::size_t segment_index = 0;
        std::uint16_t char_offset = 0;
    };

    template<typename Source>
    struct tokenizer {
    private:
        Source source;
        std::optional<char> next_char;

        [[nodiscard]]
        constexpr bool source_in_literal_run() const noexcept {
            if constexpr (requires { typename Source::reveal_runs_type; source.in_literal_run(); }) {
                return source.in_literal_run();
            }
            return false;
        }

    public:
        using source_type = Source;

        constexpr tokenizer(Source s) : source(s), next_char(source.next()) {}

        [[nodiscard]]
        constexpr bool has_next() const noexcept {
            return next_char.has_value();
        }

        [[nodiscard]]
        constexpr bool in_literal_run() const noexcept {
            if constexpr (requires { typename Source::reveal_runs_type; }) {
                return !Source::reveal_runs_type::literal_has_escape_prefix && source_in_literal_run();
            }
            return false;
        }

        constexpr std::optional<char> next_literal_char() noexcept {
            if (!next_char.has_value()) return std::nullopt;
            if (!in_literal_run()) return std::nullopt;
            const char c = *next_char;
            next_char = source.next();
            return c;
        }

        [[nodiscard]]
        constexpr std::optional<literal_run_cursor> current_literal_position() const noexcept {
            if (!next_char.has_value() || !in_literal_run()) return std::nullopt;
            if constexpr (requires { source.current_literal_position(); }) {
                return source.current_literal_position();
            }
            return std::nullopt;
        }

        constexpr bool skip_literal_chars(std::size_t count) noexcept {
            if (count == 0 || !next_char.has_value() || !in_literal_run()) return false;
            if constexpr (requires { source.skip_literal_chars(std::size_t{}); }) {
                source.skip_literal_chars(count - 1);
                next_char = source.next();
                return true;
            }
            for (std::size_t i = 0; i < count && next_char.has_value() && in_literal_run(); ++i) {
                next_char = source.next();
            }
            return true;
        }

        constexpr std::optional<std::variant<char_token, color_token>> next() noexcept {
            if (!next_char.has_value()) return std::nullopt;

            const bool current_literal = source_in_literal_run();

            const char c = *next_char;
            next_char = source.next();

            if constexpr (requires { typename Source::reveal_runs_type; }) {
                if (!Source::reveal_runs_type::literal_has_escape_prefix && current_literal) {
                    return char_token{static_cast<unsigned int>(static_cast<unsigned char>(c))};
                }
            }

            if (is_color_escape_prefix(c)) {
                if (!next_char.has_value()) {
                    return std::nullopt;
                }
                const char esc_char = *next_char;
                next_char = source.next();
                const auto nibble = decode_color_escape_nibble(esc_char);
                return color_token{nibble};
            }

            return char_token{static_cast<unsigned int>(static_cast<unsigned char>(c))};
        }

        constexpr void reset() noexcept {
            source.reset();
            next_char = source.next();
        }
    };

    struct cstr_source {
    private:
        const char* begin = nullptr;
        const char* ptr = nullptr;

    public:
        constexpr cstr_source(const char* str) : begin(str), ptr(str) {}

        [[nodiscard]]
        constexpr std::optional<char> next() noexcept {
            if (ptr == nullptr || *ptr == '\0') return std::nullopt;
            return *ptr++;
        }

        constexpr void reset() noexcept {
            ptr = begin;
        }
    };

    using cstr_stream = tokenizer<cstr_source>;

    template<typename Generator>
    struct generator_source {
    private:
        Generator generator;

    public:
        using reveal_runs_type = typename generator_reveal_runs<Generator>::type;

        constexpr explicit generator_source(Generator g)
            : generator(std::move(g)) {}

        [[nodiscard]]
        constexpr std::optional<char> next() noexcept {
            return generator();
        }

        constexpr void reset() noexcept {
            if constexpr (requires { generator.reset(); }) {
                generator.reset();
            }
        }

        [[nodiscard]]
        constexpr bool in_literal_run() const noexcept {
            if constexpr (generator_reveal_runs<Generator>::available) {
                if (generator.segment_idx >= Generator::segment_count) return false;
                return Generator::ast.segments[generator.segment_idx].type ==
                    gba::format::segment_type::literal;
            }
            return false;
        }

        [[nodiscard]]
        constexpr std::optional<literal_run_cursor> current_literal_position() const noexcept {
            if constexpr (generator_reveal_runs<Generator>::available) {
                if (!in_literal_run() || generator.char_idx == 0) return std::nullopt;
                return literal_run_cursor{
                    .segment_index = generator.segment_idx,
                    .char_offset = static_cast<std::uint16_t>(generator.char_idx - 1),
                };
            }
            return std::nullopt;
        }

        constexpr void skip_literal_chars(std::size_t count) noexcept {
            if constexpr (generator_reveal_runs<Generator>::available) {
                generator.skip_literal_chars(count);
            } else {
                (void)count;
            }
        }
    };

    template<typename Generator>
    using generated_stream = tokenizer<generator_source<Generator>>;

    template<typename Generator>
    constexpr auto stream(Generator gen) {
        using gen_t = std::decay_t<Generator>;
        return generated_stream<gen_t>{generator_source<gen_t>{std::move(gen)}};
    }

    template<typename Generator, typename Font, typename Metrics>
    constexpr auto stream(Generator gen, const Font&, const Metrics&) {
        return stream(std::move(gen));
    }

    template<fixed_string Fmt>
    struct format_source {
    private:
        using format_gen = decltype(
            gba::text::text_format<Fmt>{}.generator());
        format_gen gen;

    public:
        using reveal_runs_type = compiled_reveal_runs<Fmt>;

        template<typename... Args>
        constexpr format_source(Args... args)
            : gen(gba::text::text_format<Fmt>{}.generator(std::move(args)...)) {}

        [[nodiscard]]
        constexpr std::optional<char> next() noexcept {
            return gen();
        }

        constexpr void reset() noexcept {
            gen.reset();
        }

        [[nodiscard]]
        constexpr bool in_literal_run() const noexcept {
            if (gen.segment_idx >= format_gen::segment_count) return false;
            return format_gen::ast.segments[gen.segment_idx].type == gba::format::segment_type::literal;
        }

        [[nodiscard]]
        constexpr std::optional<literal_run_cursor> current_literal_position() const noexcept {
            if (!in_literal_run() || gen.char_idx == 0) return std::nullopt;
            return literal_run_cursor{
                .segment_index = gen.segment_idx,
                .char_offset = static_cast<std::uint16_t>(gen.char_idx - 1),
            };
        }

        constexpr void skip_literal_chars(std::size_t count) noexcept {
            gen.skip_literal_chars(count);
        }
    };

    template<fixed_string Fmt, typename... Args>
    using format_stream = tokenizer<format_source<Fmt>>;

    template<fixed_string Fmt, typename... Args>
    constexpr auto stream(text_format<Fmt>, Args... args) {
        return format_stream<Fmt, Args...>{format_source<Fmt>{std::move(args)...}};
    }

} // namespace gba::text
