/// @file tests/codec/test_codec_fundamental.cpp
/// @brief Unit tests for fundamental/fixed-width codecs, including malformed and incremental decoding.

#include <gba/codec>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <span>

using namespace gba::codec;

static_assert(FixedSizeCodec<decltype(uint32_codec)>);
static_assert(uint32_codec.encoded_size == 4);
static_assert(FixedSizeCodec<decltype(byte_codec)>);
static_assert(byte_codec.encoded_size == 1);
static_assert(FixedSizeCodec<decltype(bool_codec)>);
static_assert(bool_codec.encoded_size == 1);
static_assert(FixedSizeCodec<decltype(monostate_codec)>);
static_assert(monostate_codec.encoded_size == 0);
static_assert(FixedSizeCodec<decltype(nullptr_codec)>);

int main() {
    gba::test("uint32 whole-buffer round trip", [] {
        std::array<std::byte, 4> buffer{};
        const auto written = encode(uint32_codec, 0xDEADBEEFu, buffer);
        gba::test.expect.is_true(written.has_value());
        gba::test.expect.eq(*written, 4u);
        const auto outcome = decode(uint32_codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.eq(*outcome.value, 0xDEADBEEFu);
        gba::test.expect.eq(outcome.consumed, 4u);
    });

    gba::test("int16 negative round trip", [] {
        std::array<std::byte, 2> buffer{};
        (void)encode(int16_codec, static_cast<std::int16_t>(-1234), buffer);
        const auto outcome = decode(int16_codec, buffer);
        gba::test.expect.eq(*outcome.value, static_cast<std::int16_t>(-1234));
    });

    gba::test("float round trip", [] {
        std::array<std::byte, 4> buffer{};
        (void)encode(float_codec, 3.5f, buffer);
        const auto outcome = decode(float_codec, buffer);
        gba::test.expect.eq(*outcome.value, 3.5f);
    });

    gba::test("byte-at-a-time incremental decode", [] {
        auto decoder = uint16_codec.make_decoder();
        gba::test.expect.is_true(decoder.state() == step::more);
        gba::test.expect.is_true(decoder.feed(std::byte{0x34}) == step::more);
        gba::test.expect.is_true(decoder.feed(std::byte{0x12}) == step::done);
        gba::test.expect.eq(decoder.value(), static_cast<std::uint16_t>(0x1234));
    });

    gba::test("byte-at-a-time incremental encode", [] {
        auto encoder = uint16_codec.make_encoder(static_cast<std::uint16_t>(0x1234));
        gba::test.expect.eq(*encoder.next(), std::byte{0x34});
        gba::test.expect.eq(*encoder.next(), std::byte{0x12});
        gba::test.expect.is_false(encoder.next().has_value());
    });

    gba::test("incomplete input reports error::incomplete", [] {
        const std::array<std::byte, 2> short_buffer{};
        const auto outcome = decode(uint32_codec, short_buffer);
        gba::test.expect.is_false(outcome.value.has_value());
        gba::test.expect.eq(outcome.value.error(), error::incomplete);
        gba::test.expect.eq(outcome.consumed, 2u);
    });

    gba::test("undersized output reports error::overflow", [] {
        std::array<std::byte, 2> tiny_buffer{};
        const auto written = encode(uint32_codec, 1u, tiny_buffer);
        gba::test.expect.is_false(written.has_value());
        gba::test.expect.eq(written.error(), error::overflow);
    });

    gba::test("bool_codec accepts 0 and 1", [] {
        const std::array<std::byte, 1> zero_byte{std::byte{0}};
        const std::array<std::byte, 1> one_byte{std::byte{1}};
        gba::test.expect.eq(*decode(bool_codec, zero_byte).value, false);
        gba::test.expect.eq(*decode(bool_codec, one_byte).value, true);
    });

    gba::test("bool_codec rejects malformed byte", [] {
        const std::array<std::byte, 1> bad_byte{std::byte{2}};
        const auto outcome = decode(bool_codec, bad_byte);
        gba::test.expect.is_false(outcome.value.has_value());
        gba::test.expect.eq(outcome.value.error(), error::invalid);
    });

    gba::test("monostate_codec and nullptr_codec are zero-byte and done immediately", [] {
        auto m_decoder = monostate_codec.make_decoder();
        gba::test.expect.is_true(m_decoder.state() == step::done);
        gba::test.expect.is_true(m_decoder.value() == std::monostate{});

        auto n_decoder = nullptr_codec.make_decoder();
        gba::test.expect.is_true(n_decoder.state() == step::done);
        gba::test.expect.is_true(n_decoder.value() == nullptr);

        auto n_encoder = nullptr_codec.make_encoder(nullptr);
        gba::test.expect.is_false(n_encoder.next().has_value());
    });

    gba::test("constant_codec always yields its NTTP value", [] {
        constexpr auto tag_codec = constant_codec<std::uint8_t{42}>{};
        static_assert(FixedSizeCodec<decltype(tag_codec)>);
        static_assert(tag_codec.encoded_size == 0);
        auto decoder = tag_codec.make_decoder();
        gba::test.expect.is_true(decoder.state() == step::done);
        gba::test.expect.eq(decoder.value(), std::uint8_t{42});
    });

    gba::test("size_codec and ptrdiff_codec round trip platform-width values", [] {
        std::array<std::byte, sizeof(std::size_t)> buffer{};
        (void)encode(size_codec, std::size_t{12345}, buffer);
        gba::test.expect.eq(*decode(size_codec, buffer).value, std::size_t{12345});
    });

    return gba::test.finish();
}
