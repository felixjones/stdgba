/// @file bits/codec/tuple.hpp
/// @brief Heterogeneous sequence codec plus member projection and apply<T>.
#pragma once

#include <gba/bits/codec/core.hpp>

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace gba::codec {

    template<Codec C, typename T>
    struct apply_codec;

    /// @brief Wraps a codec, tagging it as the encoding for one specific data member.
    ///
    /// `member_codec` does not change the wire format at all: it forwards
    /// directly to the underlying codec. Its only purpose is to carry
    /// `member_pointer` so that `tuple_codec::apply<T>()` can project values
    /// into and out of `T` without any reflection.
    template<Codec C, auto MemberPtr>
    struct member_codec : fixed_size_base<detail::forwarded_encoded_size<C>(), FixedSizeCodec<C>> {
        using value_type = C::value_type;
        using decoder = C::decoder;
        using encoder = C::encoder;

        C base{};

        /// @brief The pointer-to-member this codec encodes/decodes.
        static constexpr auto member_pointer = MemberPtr;

        constexpr decoder make_decoder() const noexcept(noexcept(base.make_decoder())) { return base.make_decoder(); }

        constexpr encoder make_encoder(const value_type& value) const noexcept(noexcept(base.make_encoder(value))) {
            return base.make_encoder(value);
        }
    };

    /// @brief Bind `codec` to a data member, for use inside `tuple_codec(...).apply<T>()`.
    ///
    /// Example:
    /// @code{.cpp}
    /// struct point {
    ///     std::int32_t x, y;
    ///
    ///     static constexpr auto codec = gba::codec::tuple_codec(
    ///         gba::codec::member<&point::x>(gba::codec::int32_codec),
    ///         gba::codec::member<&point::y>(gba::codec::int32_codec)
    ///     ).apply<point>();
    /// };
    /// @endcode
    template<auto MemberPtr, Codec C>
    constexpr auto member(C codec) {
        return member_codec<C, MemberPtr>{{}, codec};
    }

    namespace detail {

        template<typename C>
        struct is_member_codec : std::false_type {};
        template<typename Inner, auto Ptr>
        struct is_member_codec<member_codec<Inner, Ptr>> : std::true_type {};

        template<typename... Codecs>
        inline constexpr bool all_member_codecs_v = (is_member_codec<Codecs>::value && ...);

        template<typename Tuple>
        struct all_member_codecs_in_tuple : std::false_type {};
        template<typename... Codecs>
        struct all_member_codecs_in_tuple<std::tuple<Codecs...>> : std::bool_constant<all_member_codecs_v<Codecs...>> {
        };

        template<typename C, typename = void>
        struct has_codecs_member : std::false_type {};
        template<typename C>
        struct has_codecs_member<C, std::void_t<typename C::codecs>> : std::true_type {};

        template<typename C, typename = void>
        struct codecs_of {
            using type = std::tuple<>;
        };
        template<typename C>
        struct codecs_of<C, std::void_t<typename C::codecs>> {
            using type = C::codecs;
        };

        template<typename T>
        concept TupleLike = requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

        template<typename Tuple, typename T, std::size_t... Is>
        constexpr Tuple project_tuple_like(const T& value, std::index_sequence<Is...>) {
            return Tuple{std::get<Is>(value)...};
        }

        template<typename Tuple, typename T, typename... Codecs, std::size_t... Is>
        constexpr Tuple project_members(const T& value, std::tuple<Codecs...>, std::index_sequence<Is...>) {
            return Tuple{(value.*(Codecs::member_pointer))...};
        }

        template<typename T, typename... Elems>
        constexpr T construct_from_tuple(std::tuple<Elems...> tup) {
            return std::apply([](auto&&... elems) { return T{std::forward<decltype(elems)>(elems)...}; },
                              std::move(tup));
        }

        template<typename... Codecs>
        constexpr bool tuple_all_fixed_v = (FixedSizeCodec<Codecs> && ...);

        template<typename... Codecs>
        constexpr std::size_t tuple_encoded_size() noexcept {
            if constexpr (tuple_all_fixed_v<Codecs...>) {
                return (Codecs::encoded_size + ... + std::size_t{0});
            } else {
                return 0;
            }
        }

    } // namespace detail

    /// @brief Codec for a fixed-arity, heterogeneous `std::tuple<T0, T1, ...>`.
    ///
    /// Elements are encoded/decoded strictly in order. When every element
    /// codec is fixed-size, `tuple_codec` is itself fixed-size and performs
    /// no allocation.
    template<Codec... Codecs>
    struct tuple_codec
        : fixed_size_base<detail::tuple_encoded_size<Codecs...>(), detail::tuple_all_fixed_v<Codecs...>> {
        using value_type = std::tuple<typename Codecs::value_type...>;

        /// @brief The element codecs, exposed for `apply<T>()`'s member projection.
        using codecs = std::tuple<Codecs...>;

        static constexpr std::size_t count = sizeof...(Codecs);

        std::tuple<Codecs...> elements{};

        struct decoder {
            std::tuple<Codecs...> elements;
            std::variant<std::monostate, typename Codecs::decoder...> active{};
            value_type result{};
            std::size_t current = 0;

            constexpr explicit decoder(const std::tuple<Codecs...>& e) : elements(e) {
                if constexpr (count > 0) {
                    activate();
                    advance();
                }
            }

            constexpr void activate() noexcept {
                detail::activate_decoder(current, elements, active, std::index_sequence_for<Codecs...>{});
            }

            constexpr void store() noexcept {
                detail::store_decoded(current, result, active, std::index_sequence_for<Codecs...>{});
            }

            [[nodiscard]] constexpr step active_state() const noexcept {
                return std::visit(
                    []<typename D>(const D& d) -> step {
                        if constexpr (std::is_same_v<D, std::monostate>) {
                            return step::error;
                        } else {
                            return d.state();
                        }
                    },
                    active);
            }

            constexpr void advance() noexcept {
                while (current < count && active_state() == step::done) {
                    store();
                    ++current;
                    if (current < count) {
                        activate();
                    }
                }
            }

            [[nodiscard]] constexpr step state() const noexcept {
                if (current >= count) {
                    return step::done;
                }
                return active_state() == step::error ? step::error : step::more;
            }

            constexpr step feed(std::byte b) noexcept {
                if (current >= count) {
                    return step::error;
                }
                const auto s = std::visit(
                    [b]<typename D>(D& d) -> step {
                        if constexpr (std::is_same_v<D, std::monostate>) {
                            return step::error;
                        } else {
                            return d.feed(b);
                        }
                    },
                    active);
                if (s == step::error) {
                    return step::error;
                }
                if (s == step::done) {
                    advance();
                }
                return state();
            }

            constexpr value_type value() const noexcept { return result; }
        };

        struct encoder {
            std::tuple<Codecs...> elements;
            value_type values;
            std::variant<std::monostate, typename Codecs::encoder...> active{};
            std::size_t current = 0;

            constexpr explicit encoder(const std::tuple<Codecs...>& e, const value_type& v) : elements(e), values(v) {
                if constexpr (count > 0) {
                    activate();
                }
            }

            constexpr void activate() noexcept {
                detail::activate_encoder(current, elements, values, active, std::index_sequence_for<Codecs...>{});
            }

            constexpr std::optional<std::byte> active_next() noexcept {
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

            constexpr std::optional<std::byte> next() noexcept {
                while (current < count) {
                    if (const auto b = active_next()) {
                        return b;
                    }
                    ++current;
                    if (current < count) {
                        activate();
                    }
                }
                return std::nullopt;
            }
        };

        constexpr decoder make_decoder() const noexcept { return decoder(elements); }
        constexpr encoder make_encoder(const value_type& value) const noexcept { return encoder(elements, value); }

        /// @brief Project this tuple codec onto `T`.
        ///
        /// Supported when `T` is tuple-like (`std::pair`, `std::array`,
        /// `std::tuple`) or when every element codec is a `member_codec`
        /// bound to `T` (see `gba::codec::member`).
        template<typename T>
        constexpr auto apply() const noexcept {
            return apply_codec<tuple_codec, T>{{}, *this};
        }
    };

    template<Codec... Codecs>
    tuple_codec(Codecs...) -> tuple_codec<Codecs...>;

    /// @brief Build a `tuple_codec` from its element codecs.
    template<Codec... Codecs>
    constexpr auto make_tuple_codec(Codecs... codecs) {
        return tuple_codec<Codecs...>{{}, std::tuple<Codecs...>{codecs...}};
    }

    /// @brief Codec adapter converting a tuple-like codec's `value_type` into `T`.
    ///
    /// @see gba::codec::apply
    template<Codec C, typename T>
    struct apply_codec : fixed_size_base<detail::forwarded_encoded_size<C>(), FixedSizeCodec<C>> {
        using value_type = T;
        using inner_value_type = C::value_type;

        C base{};

        struct decoder {
            C::decoder inner;

            [[nodiscard]] constexpr step state() const noexcept { return inner.state(); }
            constexpr step feed(std::byte b) noexcept { return inner.feed(b); }
            constexpr value_type value() const { return detail::construct_from_tuple<T>(inner.value()); }
        };

        struct encoder {
            C::encoder inner;

            constexpr std::optional<std::byte> next() noexcept { return inner.next(); }
        };

        constexpr decoder make_decoder() const noexcept(noexcept(base.make_decoder())) {
            return decoder{base.make_decoder()};
        }

        constexpr encoder make_encoder(const value_type& value) const {
            if constexpr (detail::TupleLike<value_type>) {
                return encoder{
                    base.make_encoder(detail::project_tuple_like<inner_value_type>(
                        value, std::make_index_sequence<std::tuple_size_v<inner_value_type>>{})),
                };
            } else if constexpr (detail::has_codecs_member<C>::value &&
                                 detail::all_member_codecs_in_tuple<typename detail::codecs_of<C>::type>::value) {
                return encoder{
                    base.make_encoder(detail::project_members<inner_value_type>(
                        value, typename detail::codecs_of<C>::type{},
                        std::make_index_sequence<std::tuple_size_v<inner_value_type>>{})),
                };
            } else {
                static_assert(sizeof(T) == 0,
                              "apply<T>: T must be tuple-like, or every element codec must be a member_codec bound "
                              "to T (see gba::codec::member)");
            }
        }
    };

    /// @brief Project any tuple-like codec onto `T`. See `tuple_codec::apply`.
    template<typename T, Codec C>
    constexpr auto apply(C codec) {
        return apply_codec<C, T>{{}, codec};
    }

    /// @brief Codec for `std::pair<A, B>`, built from two element codecs.
    template<Codec CodecA, Codec CodecB>
    constexpr auto pair_codec(CodecA a, CodecB b) {
        using pair_type = std::pair<typename CodecA::value_type, typename CodecB::value_type>;
        return make_tuple_codec(a, b).template apply<pair_type>();
    }

} // namespace gba::codec
