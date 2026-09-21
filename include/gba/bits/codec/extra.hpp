/// @file bits/codec/extra.hpp
/// @brief Codecs for stdgba value types.
#pragma once

#include <gba/angle>
#include <gba/bits/codec/fundamental.hpp>
#include <gba/bits/codec/transform.hpp>
#include <gba/color>
#include <gba/fixed_point>
#include <gba/keyinput>

#include <bit>
#include <cstdint>

namespace gba::codec {

    namespace detail {

        struct angle_encode {
            constexpr angle::value_type operator()(const angle value) const noexcept { return gba::bit_cast(value); }
        };

        struct angle_decode {
            constexpr angle operator()(const angle::value_type value) const noexcept { return angle{value}; }
        };

        struct color_encode {
            constexpr unsigned short operator()(const color value) const noexcept {
                return std::bit_cast<unsigned short>(value);
            }
        };

        struct color_decode {
            constexpr color operator()(const unsigned short value) const noexcept {
                return std::bit_cast<color>(value);
            }
        };

        struct keypad_encode {
            constexpr std::uint32_t operator()(const keypad value) const noexcept {
                return std::bit_cast<std::uint32_t>(value);
            }
        };

        struct keypad_decode {
            constexpr keypad operator()(const std::uint32_t value) const noexcept {
                return std::bit_cast<keypad>(value);
            }
        };

        template<unsigned int Bits>
        struct packed_angle_encode {
            using value_type = packed_angle<Bits>::storage_type;

            constexpr value_type operator()(const packed_angle<Bits> value) const noexcept {
                return gba::bit_cast(value);
            }
        };

        template<unsigned int Bits>
        struct packed_angle_decode {
            using value_type = packed_angle<Bits>::storage_type;

            constexpr packed_angle<Bits> operator()(const value_type value) const noexcept {
                return packed_angle<Bits>{value};
            }
        };

        template<fixed_point Fixed>
        struct fixed_point_encode {
            using rep = fixed_point_traits<Fixed>::rep;

            constexpr rep operator()(const Fixed value) const noexcept { return gba::bit_cast(value); }
        };

        template<fixed_point Fixed>
        struct fixed_point_decode {
            using rep = fixed_point_traits<Fixed>::rep;

            constexpr Fixed operator()(const rep value) const noexcept { return std::bit_cast<Fixed>(value); }
        };

    } // namespace detail

    /// @brief Codec for the raw 32-bit representation of `gba::angle`.
    inline constexpr auto angle_codec = transform(uint32_codec, detail::angle_encode{}, detail::angle_decode{});

    /// @brief Codec for the native 16-bit GBA palette representation of `gba::color`.
    inline constexpr auto color_codec = transform(uint16_codec, detail::color_encode{}, detail::color_decode{});

    /// @brief Codec for the current and previous samples held by `gba::keypad`.
    inline constexpr auto keypad_codec = transform(uint32_codec, detail::keypad_encode{}, detail::keypad_decode{});

    /// @brief Codec for the raw storage representation of `gba::packed_angle<Bits>`.
    template<unsigned int Bits>
    inline constexpr auto packed_angle_codec = transform(identity_codec<typename packed_angle<Bits>::storage_type>,
                                                         detail::packed_angle_encode<Bits>{},
                                                         detail::packed_angle_decode<Bits>{});

    /// @brief Codec for the raw representation of a `gba::fixed` type.
    template<fixed_point Fixed>
    inline constexpr auto fixed_point_codec = transform(identity_codec<typename fixed_point_traits<Fixed>::rep>,
                                                        detail::fixed_point_encode<Fixed>{},
                                                        detail::fixed_point_decode<Fixed>{});

} // namespace gba::codec
