/// @file bits/codec/core.hpp
/// @brief Core concepts, error/result types, and byte-at-a-time drivers for gba::codec.
#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <variant>

namespace gba::codec {

    /// @brief Failure classification shared by every codec in this module.
    ///
    /// Codecs never throw to signal malformed input; every fallible operation
    /// reports one of these values instead.
    enum class error : unsigned char {
        incomplete, ///< Input ended before a value could be fully decoded.
        invalid,    ///< Input bytes do not form a valid encoding of `value_type`.
        overflow,   ///< Output capacity (or a bounded codec's capacity) was exceeded.
    };

    /// @brief Explicit result type: either a decoded/encoded value or a `codec::error`.
    ///
    /// Alias of `std::expected` so callers get the full monadic interface
    /// (`and_then`, `transform`, `value_or`, ...) without a bespoke type.
    template<typename T>
    using result = std::expected<T, error>;

    /// @brief Outcome of feeding one byte into an incremental decoder.
    enum class step : unsigned char {
        more,  ///< Decoder accepted the byte and needs more input.
        done,  ///< Decoder has a complete value available via `value()`.
        error, ///< Input decoded so far is malformed; decoding cannot continue.
    };

    /// @brief An incremental, byte-at-a-time decoder for some `value_type`.
    ///
    /// A decoder starts in `state() == step::more` unless its codec requires
    /// zero bytes (in which case it starts `done`). `feed` is called once per
    /// input byte; once `state()` reports `done`, `value()` may be read.
    template<typename D>
    concept ByteDecoder = requires(D d, std::byte b) {
        { d.state() } -> std::same_as<step>;
        { d.feed(b) } -> std::same_as<step>;
    };

    /// @brief An incremental, byte-at-a-time encoder for some encoded value.
    ///
    /// `next()` yields one byte at a time until the value is fully encoded,
    /// then returns `std::nullopt`.
    template<typename E>
    concept ByteEncoder = requires(E e) {
        { e.next() } -> std::same_as<std::optional<std::byte>>;
    };

    /// @brief The central concept implemented by every codec-as-value in this module.
    ///
    /// A `Codec` exposes `value_type` and can manufacture a decoder for that
    /// type plus an encoder bound to a concrete value. Codec objects are
    /// small, copyable, structural values so they can be composed and passed
    /// around at compile time (including as non-type template parameters when
    /// every data member is itself structural).
    template<typename C>
    concept Codec = requires(const C c, const C::value_type& v) {
        typename C::value_type;
        { c.make_decoder() } -> ByteDecoder;
        { c.make_encoder(v) } -> ByteEncoder;
    };

    /// @brief A codec whose wire format always occupies exactly `encoded_size` bytes.
    ///
    /// Fixed-size codecs never allocate and are suitable for stack buffers
    /// sized at compile time.
    template<typename C>
    concept FixedSizeCodec = Codec<C> && requires {
        { C::encoded_size } -> std::convertible_to<std::size_t>;
    };

    /// @brief Conditionally exposes a static `encoded_size` member.
    ///
    /// Composite codecs (`array_codec`, `tuple_codec`, ...) inherit from
    /// `fixed_size_base<Size, Enabled>` to forward a fixed wire size only when
    /// every sub-codec is itself fixed-size. When `Enabled` is `false` the
    /// member is entirely absent (never declared), so `FixedSizeCodec` correctly
    /// reports the composite as variable-size without triggering an ill-formed
    /// reference to a sub-codec's missing `encoded_size`.
    template<std::size_t Size, bool Enabled>
    struct fixed_size_base {};

    template<std::size_t Size>
    struct fixed_size_base<Size, true> {
        static constexpr std::size_t encoded_size = Size;
    };

    namespace detail {

        /// @brief `C::encoded_size` when `C` is fixed-size, `0` otherwise.
        ///
        /// Lets wrapper codecs (`member_codec`, `transform_codec`, ...) that forward an inner codec's wire size
        /// unchanged compute the `Size` argument for `fixed_size_base` without
        /// an ill-formed reference to a missing `encoded_size` member.
        template<typename C>
        constexpr std::size_t forwarded_encoded_size() noexcept {
            if constexpr (FixedSizeCodec<C>) {
                return C::encoded_size;
            } else {
                return 0;
            }
        }

        /// @brief `d.error_reason()` when a decoder opts in to a specific failure
        /// reason, `error::invalid` otherwise.
        ///
        /// Lets codecs report a specific error through the generic `decode()`
        /// driver without complicating the byte-at-a-time decoder contract.
        template<typename D>
        constexpr error decoder_error_reason(const D& d) noexcept {
            if constexpr (requires {
                              { d.error_reason() } -> std::convertible_to<error>;
                          }) {
                return d.error_reason();
            } else {
                return error::invalid;
            }
        }

    } // namespace detail

    /// @brief Result of decoding a value from a bounded byte span.
    template<typename T>
    struct decode_outcome {
        result<T> value;      ///< The decoded value, or the failure reason.
        std::size_t consumed; ///< Number of bytes consumed from the input span.
    };

    /// @brief Decode one value from a contiguous byte span using a codec's incremental decoder.
    ///
    /// @param codec Codec describing the wire format of `value_type`.
    /// @param input Bytes available for decoding; not all of them need be consumed.
    /// @return The decoded value and the number of bytes consumed, or `error::incomplete`
    ///         if `input` ran out before a value completed, or `error::invalid` if the
    ///         bytes seen so far are malformed.
    template<Codec C>
    constexpr decode_outcome<typename C::value_type> decode(const C& codec, const std::span<const std::byte> input) {
        auto decoder = codec.make_decoder();
        std::size_t consumed = 0;
        if (decoder.state() == step::done) {
            return {
                result<typename C::value_type>{std::in_place, decoder.value()},
                consumed,
            };
        }
        for (const auto b : input) {
            const auto s = decoder.feed(b);
            ++consumed;
            if (s == step::done) {
                return {
                    result<typename C::value_type>{std::in_place, decoder.value()},
                    consumed,
                };
            }
            if (s == step::error) {
                return {
                    result<typename C::value_type>{std::unexpect, detail::decoder_error_reason(decoder)},
                    consumed,
                };
            }
        }
        return {
            result<typename C::value_type>{std::unexpect, error::incomplete},
            consumed,
        };
    }

    /// @brief Encode one value into a fixed-capacity byte span.
    ///
    /// @return The number of bytes written, or `error::overflow` if `output` is too small.
    template<Codec C>
    constexpr result<std::size_t> encode(const C& codec, const typename C::value_type& value,
                                         std::span<std::byte> output) {
        auto encoder = codec.make_encoder(value);
        std::size_t written = 0;
        while (const auto b = encoder.next()) {
            if (written >= output.size()) {
                return result<std::size_t>{std::unexpect, error::overflow};
            }
            output[written++] = *b;
        }
        return written;
    }

    /// @brief Encode one value by appending bytes to a dynamic sink (e.g. `std::vector<std::byte>`).
    ///
    /// @tparam Sink Any type supporting `push_back(std::byte)`.
    template<Codec C, typename Sink>
    constexpr void encode_to(const C& codec, const typename C::value_type& value, Sink& sink) {
        auto encoder = codec.make_encoder(value);
        while (const auto b = encoder.next()) {
            sink.push_back(*b);
        }
    }

    namespace detail {

        /// @brief Construct alternative `Index + 1` of `active` from `std::get<Index>(codecs).make_decoder()`.
        ///
        /// `active` is expected to be `std::variant<std::monostate, Codecs::decoder...>`;
        /// the leading `std::monostate` alternative represents "nothing activated yet".
        /// Used by composite codecs (`tuple_codec`, `variant_codec`) to activate the
        /// decoder for whichever sub-codec is currently in play, selected at runtime
        /// by `index`.
        template<typename Codecs, typename VariantOfDecoders, std::size_t... Is>
        constexpr void activate_decoder(std::size_t index, const Codecs& codecs, VariantOfDecoders& active,
                                        std::index_sequence<Is...>) {
            ((index == Is ? static_cast<void>(active.template emplace<Is + 1>(std::get<Is>(codecs).make_decoder()))
                          : static_cast<void>(0)),
             ...);
        }

        /// @brief Store `std::get<Index + 1>(active).value()` into `std::get<Index>(result)`.
        ///
        /// Used by `tuple_codec` to stash each field's decoded value before moving on
        /// to the next sub-decoder (the shared `active` variant is about to be reused).
        template<typename ResultTuple, typename VariantOfDecoders, std::size_t... Is>
        constexpr void store_decoded(std::size_t index, ResultTuple& result, const VariantOfDecoders& active,
                                     std::index_sequence<Is...>) {
            ((index == Is ? static_cast<void>(std::get<Is>(result) = std::get<Is + 1>(active).value())
                          : static_cast<void>(0)),
             ...);
        }

        /// @brief Construct alternative `Index + 1` of `active` from
        /// `std::get<Index>(codecs).make_encoder(std::get<Index>(values))`.
        ///
        /// `active` is expected to be `std::variant<std::monostate, Codecs::encoder...>`.
        /// Used by `tuple_codec` to activate the encoder for whichever field is
        /// currently being emitted.
        template<typename Codecs, typename Values, typename VariantOfEncoders, std::size_t... Is>
        constexpr void activate_encoder(std::size_t index, const Codecs& codecs, const Values& values,
                                        VariantOfEncoders& active, std::index_sequence<Is...>) {
            ((index == Is ? static_cast<void>(active.template emplace<Is + 1>(
                                std::get<Is>(codecs).make_encoder(std::get<Is>(values))))
                          : static_cast<void>(0)),
             ...);
        }

    } // namespace detail

} // namespace gba::codec
