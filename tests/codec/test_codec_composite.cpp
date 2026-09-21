/// @file tests/codec/test_codec_composite.cpp
/// @brief Unit tests for array, tuple, member projection, apply<T>, variant, and optional codecs.

#include <gba/codec>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>

using namespace gba::codec;

namespace {

    struct point {
        std::int32_t x;
        std::int32_t y;

        static constexpr auto codec =
            make_tuple_codec(member<&point::x>(int32_codec), member<&point::y>(int32_codec)).apply<point>();
    };

} // namespace

static_assert(FixedSizeCodec<decltype(array_of<4>(uint8_codec))>);
static_assert(array_of<4>(uint8_codec).encoded_size == 4);
static_assert(FixedSizeCodec<decltype(point::codec)>);
static_assert(point::codec.encoded_size == 8);

int main() {
    gba::test("array_codec round trip", [] {
        constexpr auto codec = array_of<3>(uint16_codec);
        const std::array<std::uint16_t, 3> values{1, 2, 3};
        std::array<std::byte, 6> buffer{};
        (void)encode(codec, values, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(*outcome.value, values);
    });

    gba::test("array_codec of zero-byte elements finishes without consuming bytes", [] {
        constexpr auto codec = array_of<5>(monostate_codec);
        auto decoder = codec.make_decoder();
        gba::test.expect.is_true(decoder.state() == step::done);
    });

    gba::test("tuple_codec heterogeneous round trip", [] {
        constexpr auto codec = make_tuple_codec(uint8_codec, uint32_codec, bool_codec);
        const std::tuple<std::uint8_t, std::uint32_t, bool> value{7, 99999u, true};
        std::array<std::byte, 6> buffer{};
        (void)encode(codec, value, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(*outcome.value == value);
    });

    gba::test("tuple_codec byte-at-a-time decode matches whole-buffer decode", [] {
        constexpr auto codec = make_tuple_codec(uint8_codec, uint16_codec);
        const std::tuple<std::uint8_t, std::uint16_t> value{5, 1000};
        std::array<std::byte, 3> buffer{};
        (void)encode(codec, value, buffer);

        auto decoder = codec.make_decoder();
        for (std::size_t i = 0; i + 1 < buffer.size(); ++i) {
            gba::test.expect.is_true(decoder.feed(buffer[i]) == step::more);
        }
        gba::test.expect.is_true(decoder.feed(buffer.back()) == step::done);
        gba::test.expect.is_true(decoder.value() == value);
    });

    gba::test("member projection and apply<T> round trip a plain struct", [] {
        const point original{-5, 42};
        std::array<std::byte, 8> buffer{};
        (void)encode(point::codec, original, buffer);
        const auto outcome = decode(point::codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(outcome.value->x, -5);
        gba::test.expect.eq(outcome.value->y, 42);
    });

    gba::test("pair_codec round trip", [] {
        constexpr auto codec = pair_codec(uint8_codec, uint32_codec);
        const std::pair<std::uint8_t, std::uint32_t> value{9, 12345u};
        std::array<std::byte, 5> buffer{};
        (void)encode(codec, value, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(*outcome.value == value);
    });

    gba::test("variant_codec round trips each alternative", [] {
        constexpr auto codec = make_variant_codec(uint8_codec, float_codec);
        using value_type = decltype(codec)::value_type;

        {
            const value_type value{std::in_place_index<0>, std::uint8_t{200}};
            std::array<std::byte, 2> buffer{};
            (void)encode(codec, value, buffer);
            const auto outcome = decode(codec, buffer);
            gba::test.expect.is_true(outcome.value.has_value());
            gba::test.expect.is_true(*outcome.value == value);
        }
        {
            const value_type value{std::in_place_index<1>, 1.5f};
            std::array<std::byte, 5> buffer{};
            (void)encode(codec, value, buffer);
            const auto outcome = decode(codec, buffer);
            gba::test.expect.is_true(outcome.value.has_value());
            gba::test.expect.is_true(*outcome.value == value);
        }
    });

    gba::test("variant_codec rejects out-of-range discriminant", [] {
        constexpr auto codec = make_variant_codec(uint8_codec, float_codec);
        const std::array<std::byte, 1> bad_discriminant{std::byte{2}};
        const auto outcome = decode(codec, bad_discriminant);
        gba::test.expect.is_false(outcome.value.has_value());
        gba::test.expect.eq(outcome.value.error(), error::invalid);
    });

    gba::test("optional_codec round trips present and absent", [] {
        constexpr auto codec = optional_of(uint32_codec);

        std::array<std::byte, 5> present_buffer{};
        (void)encode(codec, std::optional<std::uint32_t>{7u}, present_buffer);
        const auto present_outcome = decode(codec, present_buffer);
        gba::test.expect.is_true(present_outcome.value.has_value());
        gba::test.expect.is_true(*present_outcome.value == std::optional<std::uint32_t>{7u});

        std::array<std::byte, 1> absent_buffer{};
        (void)encode(codec, std::optional<std::uint32_t>{}, absent_buffer);
        const auto absent_outcome = decode(codec, absent_buffer);
        gba::test.expect.is_true(absent_outcome.value.has_value());
        gba::test.expect.is_false(absent_outcome.value->has_value());
    });

    return gba::test.finish();
}
