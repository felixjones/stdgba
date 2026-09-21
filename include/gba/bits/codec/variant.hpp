/// @file bits/codec/variant.hpp
/// @brief Discriminated-union codec for `std::variant<Ts...>`.
#pragma once

#include <gba/bits/codec/core.hpp>

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

namespace gba::codec {

    namespace detail {

        template<typename Codecs, typename Value, typename VariantOfEncoders, std::size_t... Is>
        constexpr void activate_encoder_for_alternative(const Codecs& codecs, const Value& value,
                                                        VariantOfEncoders& active, std::index_sequence<Is...>) {
            ((value.index() == Is ? static_cast<void>(active.template emplace<Is + 1>(
                                        std::get<Is>(codecs).make_encoder(std::get<Is>(value))))
                                  : static_cast<void>(0)),
             ...);
        }

        template<typename Value, typename VariantOfDecoders, std::size_t... Is>
        constexpr Value value_for_alternative(std::size_t index, const VariantOfDecoders& active,
                                              std::index_sequence<Is...>) {
            Value out{};
            ((index == Is ? static_cast<void>(out = Value{std::in_place_index<Is>, std::get<Is + 1>(active).value()})
                          : static_cast<void>(0)),
             ...);
            return out;
        }

    } // namespace detail

    /// @brief Codec for `std::variant<T0, T1, ...>`.
    ///
    /// Wire format is one discriminant byte (the active alternative's index,
    /// `sizeof...(Codecs)` must not exceed 256) followed by that alternative's
    /// bytes. Always variable-size (never a `FixedSizeCodec`), even when every
    /// alternative happens to share the same wire size.
    template<Codec... Codecs>
    struct variant_codec {
        static_assert(sizeof...(Codecs) <= 256, "variant_codec: at most 256 alternatives fit in one discriminant byte");

        using value_type = std::variant<typename Codecs::value_type...>;

        static constexpr std::size_t count = sizeof...(Codecs);

        std::tuple<Codecs...> alternatives{};

        struct decoder {
            std::tuple<Codecs...> alternatives;
            std::variant<std::monostate, typename Codecs::decoder...> active{};
            std::size_t index = 0;
            bool have_index = false;
            bool failed = false;

            constexpr explicit decoder(const std::tuple<Codecs...>& a) : alternatives(a) {}

            [[nodiscard]] constexpr step active_state() const noexcept {
                return std::visit(
                    []<typename D>(const D& d) -> step {
                        if constexpr (std::is_same_v<D, std::monostate>) {
                            return step::more;
                        } else {
                            return d.state();
                        }
                    },
                    active);
            }

            [[nodiscard]] constexpr step state() const noexcept {
                if (failed) {
                    return step::error;
                }
                if (!have_index) {
                    return step::more;
                }
                return active_state();
            }

            constexpr step feed(std::byte b) noexcept {
                if (failed) {
                    return step::error;
                }
                if (!have_index) {
                    const auto raw = static_cast<unsigned char>(b);
                    if (raw >= count) {
                        failed = true;
                        return step::error;
                    }
                    index = raw;
                    have_index = true;
                    detail::activate_decoder(index, alternatives, active, std::index_sequence_for<Codecs...>{});
                    return active_state() == step::error ? step::error : active_state();
                }
                return std::visit(
                    [b]<typename D>(D& d) -> step {
                        if constexpr (std::is_same_v<D, std::monostate>) {
                            return step::error;
                        } else {
                            return d.feed(b);
                        }
                    },
                    active);
            }

            constexpr value_type value() const {
                return detail::value_for_alternative<value_type>(index, active, std::index_sequence_for<Codecs...>{});
            }
        };

        struct encoder {
            std::tuple<Codecs...> alternatives;
            std::byte discriminant;
            bool discriminant_emitted = false;
            std::variant<std::monostate, typename Codecs::encoder...> active{};

            constexpr explicit encoder(const std::tuple<Codecs...>& a, const value_type& value)
                : alternatives(a), discriminant(static_cast<std::byte>(value.index())) {
                detail::activate_encoder_for_alternative(alternatives, value, active,
                                                         std::index_sequence_for<Codecs...>{});
            }

            constexpr std::optional<std::byte> next() noexcept {
                if (!discriminant_emitted) {
                    discriminant_emitted = true;
                    return discriminant;
                }
                return std::visit(
                    []<typename D>(D& d) -> std::optional<std::byte> {
                        if constexpr (std::is_same_v<D, std::monostate>) {
                            return std::nullopt;
                        } else {
                            return d.next();
                        }
                    },
                    active);
            }
        };

        constexpr decoder make_decoder() const noexcept { return decoder(alternatives); }
        constexpr encoder make_encoder(const value_type& value) const noexcept { return encoder(alternatives, value); }
    };

    template<Codec... Codecs>
    variant_codec(Codecs...) -> variant_codec<Codecs...>;

    /// @brief Build a `variant_codec` from its alternative codecs.
    template<Codec... Codecs>
    constexpr auto make_variant_codec(Codecs... codecs) {
        return variant_codec<Codecs...>{std::tuple<Codecs...>{codecs...}};
    }

} // namespace gba::codec
