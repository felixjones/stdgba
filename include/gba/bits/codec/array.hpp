/// @file bits/codec/array.hpp
/// @brief Fixed-count homogeneous sequence codec.
#pragma once

#include <gba/bits/codec/core.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace gba::codec {

    namespace detail {

        template<typename ElemCodec, std::size_t N>
        constexpr std::size_t array_encoded_size() noexcept {
            if constexpr (FixedSizeCodec<ElemCodec>) {
                return ElemCodec::encoded_size * N;
            } else {
                return 0;
            }
        }

        template<typename ElemCodec, std::size_t N, std::size_t... Is>
        constexpr auto make_decoder_array(const ElemCodec& element, std::index_sequence<Is...>) {
            return std::array<typename ElemCodec::decoder, N>{((void)Is, element.make_decoder())...};
        }

        template<typename ElemCodec, std::size_t N, std::size_t... Is>
        constexpr auto make_encoder_array(const ElemCodec& element,
                                          const std::array<typename ElemCodec::value_type, N>& values,
                                          std::index_sequence<Is...>) {
            return std::array<typename ElemCodec::encoder, N>{element.make_encoder(std::get<Is>(values))...};
        }

    } // namespace detail

    /// @brief Codec for a fixed-length, homogeneous `std::array<T, N>`.
    ///
    /// All `N` elements share the same element codec. When `ElemCodec` is a
    /// `FixedSizeCodec`, `array_codec` is itself fixed-size (`N * encoded_size`)
    /// and performs no allocation.
    template<Codec ElemCodec, std::size_t N>
    struct array_codec : fixed_size_base<detail::array_encoded_size<ElemCodec, N>(), FixedSizeCodec<ElemCodec>> {
        using value_type = std::array<typename ElemCodec::value_type, N>;

        ElemCodec element{};

        struct decoder {
            std::array<typename ElemCodec::decoder, N> elements;
            std::size_t current = 0;

            constexpr explicit decoder(const ElemCodec& e)
                : elements(detail::make_decoder_array<ElemCodec, N>(e, std::make_index_sequence<N>{})) {
                advance();
            }

            constexpr void advance() noexcept {
                while (current < N && elements[current].state() == step::done) {
                    ++current;
                }
            }

            [[nodiscard]] constexpr step state() const noexcept {
                if (current >= N) {
                    return step::done;
                }
                return elements[current].state() == step::error ? step::error : step::more;
            }

            constexpr step feed(std::byte b) noexcept {
                if (current >= N) {
                    return step::error;
                }
                const auto s = elements[current].feed(b);
                if (s == step::error) {
                    return step::error;
                }
                if (s == step::done) {
                    ++current;
                    advance();
                }
                return state();
            }

            constexpr value_type value() const noexcept {
                value_type out{};
                for (std::size_t i = 0; i < N; ++i) {
                    out[i] = elements[i].value();
                }
                return out;
            }
        };

        struct encoder {
            std::array<typename ElemCodec::encoder, N> elements;
            std::size_t current = 0;

            constexpr explicit encoder(const ElemCodec& e, const value_type& values)
                : elements(detail::make_encoder_array<ElemCodec, N>(e, values, std::make_index_sequence<N>{})) {}

            constexpr std::optional<std::byte> next() noexcept {
                while (current < N) {
                    if (const auto b = elements[current].next()) {
                        return b;
                    }
                    ++current;
                }
                return std::nullopt;
            }
        };

        constexpr decoder make_decoder() const noexcept { return decoder(element); }
        constexpr encoder make_encoder(const value_type& values) const noexcept { return encoder(element, values); }
    };

    /// @brief Build an `array_codec<ElemCodec, N>` from an element codec, deducing `N` from the caller.
    template<std::size_t N, Codec ElemCodec>
    constexpr auto array_of(ElemCodec element) {
        return array_codec<ElemCodec, N>{{}, element};
    }

} // namespace gba::codec
