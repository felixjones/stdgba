/// @file bits/text/stream.hpp
/// @brief Character stream adapters for proportional text rendering.
#pragma once

#include <gba/bits/text/types.hpp>

#include <optional>
#include <type_traits>
#include <utility>

namespace gba::text {

    template<typename Font>
    class cstr_stream {
    public:
        constexpr cstr_stream(const char* text, const Font& font, stream_metrics metrics = {})
            : m_text(text), m_font(font), m_metrics(metrics) {}

        constexpr std::optional<char> next() {
            if (*m_text == '\0') return std::nullopt;
            return *m_text++;
        }

        [[nodiscard]]
        constexpr std::size_t until_break_px(break_policy policy) const {
            const char* p = m_text;
            std::size_t width = 0;
            bool emitted = false;

            while (*p != '\0') {
                if (is_color_escape_prefix(*p)) {
                    const char next = *(p + 1);
                    if (next == '\0') break;
                    if (decode_color_escape_nibble(next) != 0) {
                        p += 2;
                        continue;
                    }
                }

                if (is_break_char(*p, policy)) break;
                if (emitted) width += m_metrics.letter_spacing_px;
                width += glyph_advance(*p);
                ++p;
                emitted = true;
            }

            if (!emitted && *p == '\t') return m_metrics.tab_width_px;
            return width;
        }

        [[nodiscard]]
        constexpr std::size_t until_break_px() const {
            return until_break_px(m_metrics.break_chars);
        }

        [[nodiscard]]
        constexpr bool done() const {
            return *m_text == '\0';
        }

    private:
        const char* m_text{};
        const Font& m_font;
        stream_metrics m_metrics{};


        [[nodiscard]]
        constexpr std::size_t glyph_advance(char ch) const {
            if (ch == '\t') return m_metrics.tab_width_px;
            if (ch == '\n' || ch == '\r') return 0;
            const auto& g = m_font.glyph_or_default(static_cast<unsigned char>(ch));
            return g.dwidth;
        }
    };

    template<typename Generator, typename Font>
    class format_stream {
    public:
        static_assert(std::is_copy_constructible_v<Generator>,
                      "format_stream requires a copyable format generator for lookahead");

        constexpr format_stream(Generator gen, const Font& font, stream_metrics metrics = {})
            : m_gen(std::move(gen)), m_font(&font), m_metrics(metrics) {}

        std::optional<char> next() { return m_gen.next(); }

        [[nodiscard]]
        std::size_t until_break_px(break_policy policy) const {
            auto lookahead = m_gen;
            std::size_t width = 0;
            bool emitted = false;
            bool prev_non_break = false;

            while (auto ch = lookahead.next()) {
                if (is_color_escape_prefix(*ch)) {
                    if (auto code = lookahead.next(); code && decode_color_escape_nibble(*code) != 0) {
                        continue;
                    }
                    break;
                }
                if (is_break_char(*ch, policy)) break;
                if (prev_non_break) width += m_metrics.letter_spacing_px;
                width += glyph_advance(*ch);
                emitted = true;
                prev_non_break = true;
            }

            if (!emitted) {
                auto probe = m_gen;
                if (auto ch = probe.next(); ch && *ch == '\t') return m_metrics.tab_width_px;
            }
            return width;
        }

        [[nodiscard]]
        std::size_t until_break_px() const {
            return until_break_px(m_metrics.break_chars);
        }

        [[nodiscard]]
        bool done() const {
            return m_gen.done();
        }

    private:
        mutable Generator m_gen;
        const Font* m_font;
        stream_metrics m_metrics{};

        [[nodiscard]]
        std::size_t glyph_advance(char ch) const {
            if (ch == '\t') return m_metrics.tab_width_px;
            if (ch == '\n' || ch == '\r') return 0;
            const auto& g = m_font->glyph_or_default(static_cast<unsigned char>(ch));
            return g.dwidth;
        }
    };

} // namespace gba::text
