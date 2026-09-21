#include <gba/bits/link/frame.hpp>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

    template<std::size_t Capacity, std::size_t PayloadSize>
    constexpr std::size_t encode_frame(std::array<std::byte, Capacity>& output,
                                       const std::array<std::byte, PayloadSize>& payload) {
        std::size_t size = 0;
        output[size++] = gba::bits::link::frame_end;

        std::uint16_t crc = gba::bits::link::frame_crc_initial;
        const auto append = [&](const std::byte byte) {
            const auto escaped = gba::bits::link::escape_frame_byte(byte);
            for (std::size_t i = 0; i < escaped.size; ++i) {
                output[size++] = escaped.bytes[i];
            }
        };

        for (const auto byte : payload) {
            crc = gba::bits::link::frame_crc_update(crc, byte);
            append(byte);
        }
        append(std::byte{static_cast<unsigned char>(crc)});
        append(std::byte{static_cast<unsigned char>(crc >> 8)});
        output[size++] = gba::bits::link::frame_end;
        return size;
    }

} // namespace

int main() {
    using namespace gba::bits::link;

    {
        constexpr std::array payload{
            std::byte{0x00}, std::byte{0xC0}, std::byte{0xDB}, std::byte{0xFF}, std::byte{0x42},
        };

        frame_encoder encoder;
        gba::test.is_true(encoder.start());
        gba::test.is_false(encoder.start());

        std::array<std::byte, 32> wire{};
        std::size_t wireSize = 0;
        std::size_t payloadIndex = 0;
        while (encoder.active()) {
            if (encoder.needs_payload()) {
                if (payloadIndex < payload.size()) {
                    gba::test.is_true(encoder.write(payload[payloadIndex++]));
                } else {
                    gba::test.is_true(encoder.finish());
                }
            }
            if (const auto byte = encoder.next()) {
                wire[wireSize++] = *byte;
            }
        }

        frame_parser parser;
        std::array<std::byte, payload.size()> decoded{};
        std::size_t decodedSize = 0;
        frame_event_kind final = frame_event_kind::none;
        for (std::size_t i = 0; i < wireSize; ++i) {
            const auto event = parser.push(wire[i]);
            if (event.kind == frame_event_kind::payload) {
                decoded[decodedSize++] = event.byte;
            } else if (event.kind != frame_event_kind::none) {
                final = event.kind;
            }
        }

        gba::test.eq(payloadIndex, payload.size());
        gba::test.eq(decodedSize, payload.size());
        gba::test.is_true(decoded == payload);
        gba::test.eq(final, frame_event_kind::complete);
    }

    {
        constexpr std::array payload{
            std::byte{0x00}, std::byte{0xC0}, std::byte{0xDB}, std::byte{0xFF}, std::byte{0x42},
        };
        std::array<std::byte, 32> wire{};
        const auto wireSize = encode_frame(wire, payload);

        frame_parser parser;
        std::array<std::byte, payload.size()> decoded{};
        std::size_t decodedSize = 0;
        frame_event_kind final = frame_event_kind::none;
        for (std::size_t i = 0; i < wireSize; ++i) {
            const auto event = parser.push(wire[i]);
            if (event.kind == frame_event_kind::payload) {
                decoded[decodedSize++] = event.byte;
            } else if (event.kind != frame_event_kind::none) {
                final = event.kind;
            }
        }

        gba::test.eq(decodedSize, payload.size());
        gba::test.is_true(decoded == payload);
        gba::test.eq(final, frame_event_kind::complete);
    }

    {
        constexpr std::array first_payload{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
        constexpr std::array second_payload{std::byte{0x44}, std::byte{0x55}};
        std::array<std::byte, 32> first{};
        std::array<std::byte, 32> second{};
        const auto firstSize = encode_frame(first, first_payload);
        const auto secondSize = encode_frame(second, second_payload);

        frame_parser parser;
        for (std::size_t i = 2; i < firstSize; ++i) {
            static_cast<void>(parser.push(first[i]));
        }

        std::array<std::byte, second_payload.size()> decoded{};
        std::size_t decodedSize = 0;
        frame_event_kind final = frame_event_kind::none;
        for (std::size_t i = 0; i < secondSize; ++i) {
            const auto event = parser.push(second[i]);
            if (event.kind == frame_event_kind::payload) {
                decoded[decodedSize++] = event.byte;
            } else if (event.kind != frame_event_kind::none) {
                final = event.kind;
            }
        }

        gba::test.eq(decodedSize, second_payload.size());
        gba::test.is_true(decoded == second_payload);
        gba::test.eq(final, frame_event_kind::complete);
    }

    {
        constexpr std::array payload{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
        std::array<std::byte, 32> wire{};
        const auto wireSize = encode_frame(wire, payload);
        wire[2] ^= std::byte{0x01};

        frame_parser parser;
        frame_event_kind final = frame_event_kind::none;
        for (std::size_t i = 0; i < wireSize; ++i) {
            const auto event = parser.push(wire[i]);
            if (event.kind != frame_event_kind::none && event.kind != frame_event_kind::payload) {
                final = event.kind;
            }
        }
        gba::test.eq(final, frame_event_kind::integrity_error);
    }

    {
        frame_parser parser;
        static_cast<void>(parser.push(frame_end));
        static_cast<void>(parser.push(frame_escape));
        static_cast<void>(parser.push(std::byte{0x7F}));
        const auto event = parser.push(frame_end);
        gba::test.eq(event.kind, frame_event_kind::malformed);
    }

    {
        frame_parser parser;
        static_cast<void>(parser.push(frame_end));
        static_cast<void>(parser.push(frame_escape));
        static_cast<void>(parser.push(frame_idle));
        const auto event = parser.push(frame_end);
        gba::test.eq(event.kind, frame_event_kind::malformed);
    }

    return gba::test.finish();
}
