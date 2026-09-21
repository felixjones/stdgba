/// @file tests/codec/test_codec_combinators.cpp
/// @brief Unit tests for transform, delimited, and varint combinators.

#include <gba/codec>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace gba::codec;

namespace {

    struct doubled_u8 {
        constexpr std::uint16_t operator()(std::uint8_t v) const noexcept { return static_cast<std::uint16_t>(v) * 2; }
    };

    struct halved_u16 {
        constexpr std::uint8_t operator()(std::uint16_t v) const noexcept { return static_cast<std::uint8_t>(v / 2); }
    };

    struct last_byte_predicate {
        constexpr bool operator()(std::uint8_t b) const noexcept { return (b & 0x80u) == 0; }
    };

} // namespace

int main() {
    gba::test("transform_codec adapts value_type both ways", [] {
        constexpr auto codec = transform(uint8_codec, halved_u16{}, doubled_u8{});
        std::array<std::byte, 1> buffer{};
        (void)encode(codec, static_cast<std::uint16_t>(84), buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(*outcome.value, static_cast<std::uint16_t>(84));
        gba::test.expect.eq(buffer[0], std::byte{42});
    });

    gba::test("delimited_codec round trip", [] {
        constexpr auto codec = delimited<3>(uint8_codec, last_byte_predicate{});

        bounded_sequence<std::uint8_t, 3> value{};
        value.push_back(std::uint8_t{0x81});
        value.push_back(std::uint8_t{0x01});

        std::array<std::byte, 2> buffer{};
        (void)encode(codec, value, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(*outcome.value == value);
    });

    gba::test("delimited_codec reports error::overflow past MaxElements", [] {
        constexpr auto codec = delimited<3>(uint8_codec, last_byte_predicate{});
        // Four continuation bytes never terminate within a 3-element capacity.
        const std::array<std::byte, 4> never_terminates{std::byte{0x81}, std::byte{0x81}, std::byte{0x81},
                                                        std::byte{0x81}};
        const auto outcome = decode(codec, never_terminates);
        gba::test.expect.is_false(outcome.value.has_value());
        gba::test.expect.eq(outcome.value.error(), error::overflow);
    });

    gba::test("unsigned_varint_codec round trips small and large values", [] {
        for (const std::uintmax_t value : {0u, 1u, 127u, 128u, 300u, 16384u, 1000000u}) {
            std::vector<std::byte> buffer;
            encode_to(unsigned_varint_codec, value, buffer);
            const auto outcome = decode(unsigned_varint_codec, buffer);
            gba::test.expect.is_true(outcome.value.has_value());
            gba::test.expect.eq(*outcome.value, value);
            gba::test.expect.eq(outcome.consumed, buffer.size());
        }
    });

    gba::test("unsigned_varint_codec single-byte values fit in one byte", [] {
        std::vector<std::byte> buffer;
        encode_to(unsigned_varint_codec, std::uintmax_t{100}, buffer);
        gba::test.expect.eq(buffer.size(), std::size_t{1});
    });

    gba::test("signed_varint_codec round trips positive and negative values", [] {
        for (const std::intmax_t value : {0, 1, -1, 63, -64, 1000, -1000, 1000000, -1000000}) {
            std::vector<std::byte> buffer;
            encode_to(signed_varint_codec, value, buffer);
            const auto outcome = decode(signed_varint_codec, buffer);
            gba::test.expect.is_true(outcome.value.has_value());
            gba::test.expect.eq(*outcome.value, value);
        }
    });

    return gba::test.finish();
}
