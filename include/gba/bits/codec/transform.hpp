/// @file bits/codec/transform.hpp
/// @brief Value-projection combinator: adapt a codec's `value_type` through a pair of pure functions.
#pragma once

#include <gba/bits/codec/core.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace gba::codec {

    /// @brief Adapts `Inner`'s wire format to a different logical `value_type`.
    ///
    /// `DecodeFn` converts a freshly-decoded `Inner::value_type` into
    /// `value_type`; `EncodeFn` converts a `value_type` back into
    /// `Inner::value_type` before handing it to `Inner`. This same mechanism
    /// serves both as a general "transform" combinator (e.g. varint codecs
    /// convert a byte sequence into an integer) and as a "projector" for
    /// single values (the one-value analogue of `member_codec`'s per-field
    /// projection).
    template<Codec Inner, typename EncodeFn, typename DecodeFn>
    struct transform_codec : fixed_size_base<detail::forwarded_encoded_size<Inner>(), FixedSizeCodec<Inner>> {
        using value_type = std::remove_cvref_t<std::invoke_result_t<DecodeFn, typename Inner::value_type>>;

        Inner inner{};
        EncodeFn encode_fn{};
        DecodeFn decode_fn{};

        struct decoder {
            Inner::decoder inner;
            DecodeFn decode_fn;

            [[nodiscard]] constexpr step state() const noexcept { return inner.state(); }
            constexpr step feed(std::byte b) noexcept { return inner.feed(b); }
            constexpr value_type value() const { return decode_fn(inner.value()); }
        };

        struct encoder {
            Inner::encoder inner;

            constexpr std::optional<std::byte> next() noexcept { return inner.next(); }
        };

        constexpr decoder make_decoder() const noexcept(noexcept(inner.make_decoder())) {
            return decoder{inner.make_decoder(), decode_fn};
        }

        constexpr encoder make_encoder(const value_type& value) const {
            return encoder{inner.make_encoder(encode_fn(value))};
        }
    };

    /// @brief Build a `transform_codec` from an inner codec and a pair of pure conversion functions.
    ///
    /// @param inner The wire-format codec, operating on `Inner::value_type`.
    /// @param encodeFn `value_type -> Inner::value_type`, used before encoding.
    /// @param decodeFn `Inner::value_type -> value_type`, used after decoding.
    template<Codec Inner, typename EncodeFn, typename DecodeFn>
    constexpr auto transform(Inner inner, EncodeFn encodeFn, DecodeFn decodeFn) {
        return transform_codec<Inner, EncodeFn, DecodeFn>{{}, inner, encodeFn, decodeFn};
    }

} // namespace gba::codec
