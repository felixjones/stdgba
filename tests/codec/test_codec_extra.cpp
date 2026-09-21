/// @file tests/codec/test_codec_extra.cpp
/// @brief Unit tests for codecs of stdgba value types.

#include <gba/angle>
#include <gba/codec>
#include <gba/color>
#include <gba/fixed_point>
#include <gba/keyinput>
#include <gba/testing>

#include <array>
#include <cstddef>

using namespace gba::literals;

static_assert(gba::codec::FixedSizeCodec<decltype(gba::codec::angle_codec)>);
static_assert(gba::codec::angle_codec.encoded_size == sizeof(gba::angle));
static_assert(gba::codec::FixedSizeCodec<decltype(gba::codec::color_codec)>);
static_assert(gba::codec::color_codec.encoded_size == sizeof(gba::color));
static_assert(gba::codec::FixedSizeCodec<decltype(gba::codec::keypad_codec)>);
static_assert(gba::codec::keypad_codec.encoded_size == sizeof(gba::keypad));
static_assert(gba::codec::FixedSizeCodec<decltype(gba::codec::packed_angle_codec<12>)>);
static_assert(gba::codec::packed_angle_codec<12>.encoded_size == sizeof(gba::packed_angle<12>));
static_assert(gba::codec::FixedSizeCodec<decltype(gba::codec::fixed_point_codec<gba::fixed<short>>)>);
static_assert(gba::codec::fixed_point_codec<gba::fixed<short>>.encoded_size == sizeof(short));

int main() {
    gba::test("color codec preserves native palette representation", [] {
        constexpr gba::color original{.red = 1, .green = 2, .blue = 3, .grn_lo = 1};
        std::array<std::byte, sizeof(original)> buffer{};
        (void)gba::codec::encode(gba::codec::color_codec, original, buffer);
        gba::test.expect.eq(buffer[0], std::byte{0x41});
        gba::test.expect.eq(buffer[1], std::byte{0x8C});

        const auto outcome = gba::codec::decode(gba::codec::color_codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(*outcome.value, original);
    });

    gba::test("keypad codec preserves input edges", [] {
        constexpr auto keys_from = [](const unsigned short pressed) {
            const auto released = static_cast<unsigned short>(~pressed & 0x3FF);
            return std::bit_cast<gba::key_control>(released);
        };

        gba::keypad original;
        original = keys_from(0);
        original = keys_from(0x0009);

        std::array<std::byte, sizeof(original)> buffer{};
        (void)gba::codec::encode(gba::codec::keypad_codec, original, buffer);
        gba::test.expect.eq(buffer[0], std::byte{0xF6});
        gba::test.expect.eq(buffer[1], std::byte{0x03});
        gba::test.expect.eq(buffer[2], std::byte{0x09});
        gba::test.expect.eq(buffer[3], std::byte{0x00});

        const auto outcome = gba::codec::decode(gba::codec::keypad_codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(outcome.value->held(gba::key_a, gba::key_start));
        gba::test.expect.is_true(outcome.value->pressed(gba::key_a, gba::key_start));
        gba::test.expect.is_false(outcome.value->released(gba::key_a, gba::key_start));
    });

    gba::test("angle codec preserves raw representation", [] {
        constexpr gba::angle original{90_deg};
        std::array<std::byte, sizeof(original)> buffer{};
        (void)gba::codec::encode(gba::codec::angle_codec, original, buffer);
        gba::test.expect.eq(buffer[0], std::byte{0x00});
        gba::test.expect.eq(buffer[1], std::byte{0x00});
        gba::test.expect.eq(buffer[2], std::byte{0x00});
        gba::test.expect.eq(buffer[3], std::byte{0x40});

        const auto outcome = gba::codec::decode(gba::codec::angle_codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(gba::bit_cast(*outcome.value), gba::bit_cast(original));
    });

    gba::test("packed angle codec uses its compact storage", [] {
        constexpr gba::packed_angle<12> original = 90_deg;
        std::array<std::byte, sizeof(original)> buffer{};
        (void)gba::codec::encode(gba::codec::packed_angle_codec<12>, original, buffer);
        gba::test.expect.eq(buffer[0], std::byte{0x00});
        gba::test.expect.eq(buffer[1], std::byte{0x04});

        const auto outcome = gba::codec::decode(gba::codec::packed_angle_codec<12>, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(gba::bit_cast(*outcome.value), gba::bit_cast(original));
    });

    gba::test("fixed point codec preserves signed raw representation", [] {
        using fixed_type = gba::fixed<short>;
        constexpr fixed_type original = -1.5_fx;
        std::array<std::byte, sizeof(original)> buffer{};
        (void)gba::codec::encode(gba::codec::fixed_point_codec<fixed_type>, original, buffer);
        gba::test.expect.eq(buffer[0], std::byte{0x80});
        gba::test.expect.eq(buffer[1], std::byte{0xFE});

        const auto outcome = gba::codec::decode(gba::codec::fixed_point_codec<fixed_type>, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(gba::bit_cast(*outcome.value), gba::bit_cast(original));
    });

    return gba::test.finish();
}
