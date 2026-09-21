/// @file bits/codec/fundamental.hpp
/// @brief Leaf codecs for fundamental, fixed-width, and empty value types.
#pragma once

#include <gba/bits/codec/core.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace gba::codec {

    /// @brief Raw byte-for-byte codec: the wire format is the object representation.
    ///
    /// The identity codec is the leaf every fixed-width codec in this file is
    /// built from. It copies `sizeof(T)` bytes verbatim (in the host's native
    /// byte order, matching the little-endian layout of both the GBA target
    /// and the hosts used to run its test suite) with zero runtime overhead
    /// and no allocation.
    ///
    /// @tparam T A trivially copyable, non-empty type free of invalid bit
    ///           patterns (use a dedicated codec such as `bool_codec` for
    ///           types where not every bit pattern is a valid value).
    template<typename T>
        requires std::is_trivially_copyable_v<T> && (sizeof(T) >= 1)
    struct identity {
        using value_type = T;

        /// @brief Fixed wire size: exactly `sizeof(T)` bytes.
        static constexpr std::size_t encoded_size = sizeof(T);

        /// @brief Byte-at-a-time decoder collecting exactly `sizeof(T)` bytes.
        struct decoder {
            std::array<std::byte, sizeof(T)> bytes{};
            std::size_t index = 0;

            [[nodiscard]] constexpr step state() const noexcept { return index >= sizeof(T) ? step::done : step::more; }

            constexpr step feed(std::byte b) noexcept {
                if (index >= sizeof(T)) {
                    return step::error;
                }
                bytes[index++] = b;
                return state();
            }

            constexpr value_type value() const noexcept { return std::bit_cast<value_type>(bytes); }
        };

        /// @brief Byte-at-a-time encoder emitting `sizeof(T)` bytes of the object representation.
        struct encoder {
            std::array<std::byte, sizeof(T)> bytes;
            std::size_t index = 0;

            constexpr std::optional<std::byte> next() noexcept {
                if (index >= bytes.size()) {
                    return std::nullopt;
                }
                return bytes[index++];
            }
        };

        [[nodiscard]] constexpr decoder make_decoder() const noexcept { return decoder{}; }

        [[nodiscard]] constexpr encoder make_encoder(const value_type& value) const noexcept {
            return encoder{.bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
        }
    };

    /// @brief Convenience alias for the identity codec of any fundamental or fixed-width type.
    template<typename T>
    inline constexpr identity<T> identity_codec{};

    inline constexpr identity<std::int8_t> int8_codec{};       ///< Codec for `std::int8_t`.
    inline constexpr identity<std::uint8_t> uint8_codec{};     ///< Codec for `std::uint8_t`.
    inline constexpr identity<std::int16_t> int16_codec{};     ///< Codec for `std::int16_t`.
    inline constexpr identity<std::uint16_t> uint16_codec{};   ///< Codec for `std::uint16_t`.
    inline constexpr identity<std::int32_t> int32_codec{};     ///< Codec for `std::int32_t`.
    inline constexpr identity<std::uint32_t> uint32_codec{};   ///< Codec for `std::uint32_t`.
    inline constexpr identity<std::int64_t> int64_codec{};     ///< Codec for `std::int64_t`.
    inline constexpr identity<std::uint64_t> uint64_codec{};   ///< Codec for `std::uint64_t`.
    inline constexpr identity<float> float_codec{};            ///< Codec for IEEE-754 `float`.
    inline constexpr identity<double> double_codec{};          ///< Codec for IEEE-754 `double`.
    inline constexpr identity<char> char_codec{};              ///< Codec for `char`.
    inline constexpr identity<std::byte> byte_codec{};         ///< Codec for `std::byte` (pass-through).
    inline constexpr identity<std::size_t> size_codec{};       ///< Codec for `std::size_t` (platform width).
    inline constexpr identity<std::ptrdiff_t> ptrdiff_codec{}; ///< Codec for `std::ptrdiff_t` (platform width).

    /// @brief Codec for `bool` validating that the wire byte is exactly `0` or `1`.
    ///
    /// `bool` has bit patterns that are not valid `bool` values, so unlike
    /// `identity<T>` this codec rejects any other byte with `error::invalid`
    /// instead of reinterpreting it.
    struct bool_codec_type {
        using value_type = bool;

        static constexpr std::size_t encoded_size = 1;

        struct decoder {
            bool result = false;
            step current = step::more;

            [[nodiscard]] constexpr step state() const noexcept { return current; }

            constexpr step feed(std::byte b) noexcept {
                if (current != step::more) {
                    return step::error;
                }
                if (b == std::byte{0}) {
                    result = false;
                } else if (b == std::byte{1}) {
                    result = true;
                } else {
                    current = step::error;
                    return current;
                }
                current = step::done;
                return current;
            }

            [[nodiscard]] constexpr value_type value() const noexcept { return result; }
        };

        struct encoder {
            bool emitted = false;
            bool source{};

            constexpr std::optional<std::byte> next() noexcept {
                if (emitted) {
                    return std::nullopt;
                }
                emitted = true;
                return source ? std::byte{1} : std::byte{0};
            }
        };

        static constexpr decoder make_decoder() noexcept { return decoder{}; }
        static constexpr encoder make_encoder(const value_type& value) noexcept {
            return encoder{.emitted = false, .source = value};
        }
    };

    inline constexpr bool_codec_type bool_codec{}; ///< Codec for `bool` (validated 0/1 byte).

    /// @brief Codec requiring and producing exactly zero bytes, always decoding to `Value`.
    ///
    /// `Value` is a non-type template parameter, so `constant_codec<Value>` is
    /// itself usable as a compile-time value (e.g. protocol tags, format
    /// version markers) without carrying any runtime state.
    template<auto Value>
    struct constant_codec {
        using value_type = decltype(Value);

        /// @brief Fixed wire size: always zero bytes.
        static constexpr std::size_t encoded_size = 0;

        struct decoder {
            [[nodiscard]] static constexpr step state() noexcept { return step::done; }
            [[nodiscard]] static constexpr step feed(std::byte) noexcept { return step::error; }
            static constexpr value_type value() noexcept { return Value; }
        };

        struct encoder {
            [[nodiscard]] static constexpr std::optional<std::byte> next() noexcept { return std::nullopt; }
        };

        [[nodiscard]] constexpr decoder make_decoder() const noexcept { return decoder{}; }
        [[nodiscard]] constexpr encoder make_encoder(const value_type&) const noexcept { return encoder{}; }
    };

    /// @brief Zero-byte codec for `std::monostate`, e.g. as a variant's placeholder alternative.
    inline constexpr constant_codec<std::monostate{}> monostate_codec{};

    /// @brief Zero-byte codec for `std::nullptr_t`.
    inline constexpr constant_codec<nullptr> nullptr_codec{};

} // namespace gba::codec
