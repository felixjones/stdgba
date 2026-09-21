/// @file bits/codec/range.hpp
/// @brief Length-prefixed dynamic sequence codec for vectors and strings.
#pragma once

#include <gba/bits/codec/core.hpp>
#include <gba/bits/codec/fundamental.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gba::codec {

    /// @brief Codec for a dynamically-sized, homogeneous sequence (e.g. `std::vector<T>`, `std::basic_string<T>`).
    ///
    /// Wire format is a length prefix (encoded with `LengthCodec`, `std::uint32_t`
    /// by default) followed by that many elements encoded with `ElemCodec`.
    /// This is the one category of codec in `gba::codec` explicitly permitted
    /// to allocate: decoding reserves and grows `Container` as elements arrive.
    template<Codec ElemCodec, typename Container = std::vector<typename ElemCodec::value_type>,
             Codec LengthCodec = identity<std::uint32_t>>
    struct range_codec {
        using value_type = Container;

        ElemCodec element{};
        LengthCodec length{};

        struct decoder {
            ElemCodec element;
            LengthCodec::decoder length_decoder;
            bool have_length = false;
            std::size_t length = 0;
            std::size_t filled = 0;
            Container result{};
            std::optional<typename ElemCodec::decoder> current_elem{};

            constexpr decoder(const ElemCodec& e, const LengthCodec& l)
                : element(e), length_decoder(l.make_decoder()) {}

            constexpr void advance() noexcept {
                while (have_length && filled < length && current_elem.has_value() &&
                       current_elem->state() == step::done) {
                    result.push_back(current_elem->value());
                    ++filled;
                    if (filled < length) {
                        current_elem.emplace(element.make_decoder());
                    }
                }
            }

            [[nodiscard]] constexpr step state() const noexcept {
                if (!have_length) {
                    return length_decoder.state() == step::error ? step::error : step::more;
                }
                if (filled >= length) {
                    return step::done;
                }
                return (current_elem.has_value() && current_elem->state() == step::error) ? step::error : step::more;
            }

            constexpr step feed(std::byte b) noexcept {
                if (!have_length) {
                    const auto s = length_decoder.feed(b);
                    if (s == step::error) {
                        return step::error;
                    }
                    if (s == step::done) {
                        length = static_cast<std::size_t>(length_decoder.value());
                        have_length = true;
                        if constexpr (requires(Container c, std::size_t n) { c.reserve(n); }) {
                            result.reserve(length);
                        }
                        if (length > 0) {
                            current_elem.emplace(element.make_decoder());
                            advance();
                        }
                    }
                    return state();
                }
                if (filled >= length) {
                    return step::error;
                }
                const auto s = current_elem->feed(b);
                if (s == step::error) {
                    return step::error;
                }
                if (s == step::done) {
                    advance();
                }
                return state();
            }

            constexpr value_type value() const { return result; }
        };

        struct encoder {
            ElemCodec element;
            LengthCodec::encoder length_encoder;
            Container values;
            std::size_t index = 0;
            bool length_done = false;
            std::optional<typename ElemCodec::encoder> current{};

            constexpr encoder(const ElemCodec& e, const LengthCodec& l, const Container& v)
                : element(e), length_encoder(l.make_encoder(static_cast<LengthCodec::value_type>(v.size()))),
                  values(v) {}

            constexpr void activate() noexcept {
                if (index < values.size()) {
                    current.emplace(element.make_encoder(values[index]));
                }
            }

            constexpr std::optional<std::byte> next() noexcept {
                if (!length_done) {
                    if (const auto b = length_encoder.next()) {
                        return b;
                    }
                    length_done = true;
                    activate();
                }
                while (index < values.size()) {
                    if (const auto b = current->next()) {
                        return b;
                    }
                    ++index;
                    activate();
                }
                return std::nullopt;
            }
        };

        [[nodiscard]] constexpr decoder make_decoder() const noexcept { return decoder(element, length); }
        [[nodiscard]] constexpr encoder make_encoder(const value_type& value) const {
            return encoder(element, length, value);
        }
    };

    /// @brief Build a `range_codec<ElemCodec, std::vector<ElemCodec::value_type>>` from an element codec.
    template<Codec ElemCodec>
    constexpr auto vector_of(ElemCodec element) {
        return range_codec<ElemCodec>{element};
    }

    /// @brief Build a `range_codec<CharCodec, std::basic_string<CharCodec::value_type>>` from a character codec.
    template<Codec CharCodec>
    constexpr auto basic_string_of(CharCodec charCodec) {
        return range_codec<CharCodec, std::basic_string<typename CharCodec::value_type>>{charCodec};
    }

    /// @brief Ready-made codec for `std::string`, one byte per `char`.
    inline constexpr auto string_codec = basic_string_of(char_codec);

} // namespace gba::codec
