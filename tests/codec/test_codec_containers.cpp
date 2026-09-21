/// @file tests/codec/test_codec_containers.cpp
/// @brief Unit tests for the allocating dynamic container codecs (vector_of, string_codec).

#include <gba/codec>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace gba::codec;

int main() {
    gba::test("vector_of round trip", [] {
        constexpr auto codec = vector_of(uint16_codec);
        const std::vector<std::uint16_t> values{10, 20, 30, 40};
        std::vector<std::byte> buffer;
        encode_to(codec, values, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(*outcome.value == values);
        gba::test.expect.eq(outcome.consumed, buffer.size());
    });

    gba::test("vector_of empty vector round trip", [] {
        constexpr auto codec = vector_of(uint8_codec);
        const std::vector<std::uint8_t> values{};
        std::vector<std::byte> buffer;
        encode_to(codec, values, buffer);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(outcome.value->empty());
    });

    gba::test("vector_of byte-at-a-time decode", [] {
        constexpr auto codec = vector_of(uint8_codec);
        const std::vector<std::uint8_t> values{1, 2, 3};
        std::vector<std::byte> buffer;
        encode_to(codec, values, buffer);

        auto decoder = codec.make_decoder();
        for (std::size_t i = 0; i + 1 < buffer.size(); ++i) {
            gba::test.expect.is_true(decoder.feed(buffer[i]) == step::more);
        }
        gba::test.expect.is_true(decoder.feed(buffer.back()) == step::done);
        gba::test.expect.is_true(decoder.value() == values);
    });

    gba::test("string_codec round trip", [] {
        const std::string text = "hello, gba";
        std::vector<std::byte> buffer;
        encode_to(string_codec, text, buffer);
        const auto outcome = decode(string_codec, buffer);
        gba::test.expect.is_true(outcome.value.has_value());
        gba::test.expect.is_true(*outcome.value == text);
    });

    gba::test("vector_of reports incomplete when length prefix promises more elements", [] {
        constexpr auto codec = vector_of(uint32_codec);
        std::vector<std::byte> buffer;
        encode_to(codec, std::vector<std::uint32_t>{1, 2, 3}, buffer);
        buffer.resize(buffer.size() - 1);
        const auto outcome = decode(codec, buffer);
        gba::test.expect.is_false(outcome.value.has_value());
        gba::test.expect.eq(outcome.value.error(), error::incomplete);
    });

    return gba::test.finish();
}
