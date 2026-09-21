/// @file bits/codec/optional.hpp
/// @brief Codec for `std::optional<T>`.
#pragma once

#include <gba/bits/codec/core.hpp>
#include <gba/bits/codec/fundamental.hpp>

#include <cstddef>
#include <optional>

namespace gba::codec {

    namespace detail {

        template<typename Inner>
        constexpr std::size_t optional_encoded_size() noexcept {
            if constexpr (FixedSizeCodec<Inner>) {
                return 1 + Inner::encoded_size;
            } else {
                return 0;
            }
        }

    } // namespace detail

    /// @brief Codec for `std::optional<T>`.
    ///
    /// Wire format is one presence byte (via `bool_codec`) followed by the
    /// wrapped value's bytes when present. Fixed-size only when `Inner` is
    /// itself fixed-size (the presence byte is always one extra byte).
    template<Codec Inner>
    struct optional_codec : fixed_size_base<detail::optional_encoded_size<Inner>(), FixedSizeCodec<Inner>> {
        using value_type = std::optional<typename Inner::value_type>;

        Inner inner{};

        struct decoder {
            Inner inner;
            bool_codec_type::decoder presence;
            std::optional<typename Inner::decoder> element{};
            bool known_absent = false;

            constexpr explicit decoder(const Inner& i) : inner(i), presence(bool_codec.make_decoder()) {}

            [[nodiscard]] constexpr step state() const noexcept {
                if (presence.state() != step::done) {
                    return presence.state();
                }
                if (known_absent) {
                    return step::done;
                }
                return element->state();
            }

            constexpr step feed(std::byte b) noexcept {
                if (presence.state() != step::done) {
                    const auto s = presence.feed(b);
                    if (s != step::done) {
                        return s;
                    }
                    if (presence.value()) {
                        element.emplace(inner.make_decoder());
                        return element->state();
                    }
                    known_absent = true;
                    return step::done;
                }
                if (known_absent) {
                    return step::error;
                }
                return element->feed(b);
            }

            constexpr value_type value() const {
                if (known_absent) {
                    return std::nullopt;
                }
                return value_type{element->value()};
            }
        };

        struct encoder {
            bool_codec_type::encoder presence;
            std::optional<typename Inner::encoder> element{};
            bool presence_emitted = false;

            constexpr explicit encoder(const Inner& i, const value_type& value)
                : presence(bool_codec.make_encoder(value.has_value())) {
                if (value.has_value()) {
                    element.emplace(i.make_encoder(*value));
                }
            }

            constexpr std::optional<std::byte> next() noexcept {
                if (!presence_emitted) {
                    if (const auto b = presence.next()) {
                        return b;
                    }
                    presence_emitted = true;
                }
                if (element) {
                    return element->next();
                }
                return std::nullopt;
            }
        };

        constexpr decoder make_decoder() const noexcept { return decoder(inner); }
        constexpr encoder make_encoder(const value_type& value) const { return encoder(inner, value); }
    };

    /// @brief Build an `optional_codec<Inner>` from an element codec.
    template<Codec Inner>
    constexpr auto optional_of(Inner inner) {
        return optional_codec<Inner>{{}, inner};
    }

} // namespace gba::codec
