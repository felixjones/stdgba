/// @file bits/text2/stream.hpp
/// @brief text2 unified color-escape tokenizer and stream types.

#pragma once

#include <gba/bits/format/compiled_format.hpp>
#include <gba/bits/text2/text2_format.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace gba::text2 {
    inline constexpr char color_escape_prefix = '\x1B';

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

    template<typename Source>
    struct tokenizer {
    private:
        Source source;
        std::optional<char> next_char;
        bool in_escape = false;

    public:
        constexpr tokenizer(Source s) : source(s), next_char(source.next()) {}

        constexpr bool has_next() const noexcept {
            return next_char.has_value();
        }

        constexpr std::optional<std::variant<char_token, color_token>> next() noexcept {
            if (!next_char.has_value()) return std::nullopt;

            const char c = *next_char;
            next_char = source.next();

            if (in_escape) {
                in_escape = false;
                const auto nibble = decode_color_escape_nibble(c);
                return color_token{nibble};
            }

            if (is_color_escape_prefix(c)) {
                in_escape = false;
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
            in_escape = false;
        }
    };

    struct cstr_source {
    private:
        const char* ptr = nullptr;

    public:
        constexpr cstr_source(const char* str) : ptr(str) {}

        [[nodiscard]]
        constexpr std::optional<char> next() noexcept {
            if (ptr == nullptr || *ptr == '\0') return std::nullopt;
            return *ptr++;
        }

        constexpr void reset() noexcept {
            if (ptr != nullptr) {
                while (*ptr != '\0') ++ptr;
                ptr = nullptr;
            }
        }
    };

    using cstr_stream = tokenizer<cstr_source>;

    template<typename Generator>
    struct generator_source {
    private:
        Generator generator;

    public:
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
            gba::text2::text2_format<Fmt>{}.generator());
        format_gen gen;

    public:
        template<typename... Args>
        constexpr format_source(Args... args)
            : gen(gba::text2::text2_format<Fmt>{}.generator(std::move(args)...)) {}

        [[nodiscard]]
        constexpr std::optional<char> next() noexcept {
            return gen();
        }

        constexpr void reset() noexcept {
            gen.reset();
        }
    };

    template<fixed_string Fmt, typename... Args>
    using format_stream = tokenizer<format_source<Fmt>>;

} // namespace gba::text2
