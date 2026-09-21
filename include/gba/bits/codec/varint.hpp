/// @file bits/codec/varint.hpp
/// @brief LEB128-style variable-length integer codecs, built from `delimited_codec` + `transform_codec`.
#pragma once

#include <gba/bits/codec/delimited.hpp>
#include <gba/bits/codec/fundamental.hpp>
#include <gba/bits/codec/transform.hpp>

#include <cstddef>
#include <cstdint>

namespace gba::codec {

    namespace detail {

        /// @brief Base-128 varint continuation predicate: true once the top bit of a byte is clear.
        struct varint_last_byte {
            constexpr bool operator()(std::uint8_t b) const noexcept { return (b & 0x80u) == 0; }
        };

        /// @brief Maximum bytes needed to encode any 64-bit value as a base-128 varint.
        inline constexpr std::size_t varint_max_bytes = 10;

        struct varint_encode_fn {
            constexpr bounded_sequence<std::uint8_t, varint_max_bytes> operator()(std::uintmax_t value) const noexcept {
                bounded_sequence<std::uint8_t, varint_max_bytes> out{};
                do {
                    auto byte = static_cast<std::uint8_t>(value & 0x7Fu);
                    value >>= 7u;
                    if (value != 0) {
                        byte |= 0x80u;
                    }
                    out.push_back(byte);
                } while (value != 0);
                return out;
            }
        };

        struct varint_decode_fn {
            constexpr std::uintmax_t operator()(
                const bounded_sequence<std::uint8_t, varint_max_bytes>& seq) const noexcept {
                std::uintmax_t result = 0;
                for (std::size_t i = 0; i < seq.size(); ++i) {
                    result |= static_cast<std::uintmax_t>(seq[i] & 0x7Fu) << (7 * i);
                }
                return result;
            }
        };

        struct zigzag_encode_fn {
            constexpr std::uintmax_t operator()(const std::intmax_t value) const noexcept {
                constexpr int bits = sizeof(std::intmax_t) * 8;
                return (static_cast<std::uintmax_t>(value) << 1u) ^ static_cast<std::uintmax_t>(value >> (bits - 1));
            }
        };

        struct zigzag_decode_fn {
            constexpr std::intmax_t operator()(const std::uintmax_t value) const noexcept {
                const std::uintmax_t signExtended = ((value & 1u) != 0u) ? ~std::uintmax_t{0} : std::uintmax_t{0};
                return static_cast<std::intmax_t>((value >> 1u) ^ signExtended);
            }
        };

    } // namespace detail

    /// @brief Codec for `std::uintmax_t` using base-128 (LEB128-style) varint encoding.
    ///
    /// Composed of `delimited_codec` (byte-at-a-time, terminated when a
    /// byte's top bit is clear, capped at 10 bytes) and `transform_codec`
    /// (converts the raw byte sequence to/from an integer). Never allocates.
    inline constexpr auto unsigned_varint_codec =
        transform(delimited<detail::varint_max_bytes>(uint8_codec, detail::varint_last_byte{}),
                  detail::varint_encode_fn{}, detail::varint_decode_fn{});

    /// @brief Codec for `std::intmax_t` using zigzag + base-128 varint encoding.
    ///
    /// Built by composing `unsigned_varint_codec` with a zigzag transform, so
    /// small-magnitude negative values stay compact.
    inline constexpr auto signed_varint_codec = transform(unsigned_varint_codec, detail::zigzag_encode_fn{},
                                                          detail::zigzag_decode_fn{});

} // namespace gba::codec
