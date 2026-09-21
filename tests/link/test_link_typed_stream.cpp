#include <gba/bits/link/typed_stream.hpp>
#include <gba/codec>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace {

    constexpr auto test_codec = gba::codec::make_variant_codec(gba::codec::uint8_codec,
                                                               gba::codec::array_of<5>(gba::codec::byte_codec));

    using stream_type = gba::bits::link::typed_stream<test_codec, 64, 2>;
    using value_type = stream_type::value_type;

    template<typename Word>
    void transfer(stream_type& from, stream_type& to, const std::size_t peer) {
        int guard = 0;
        while (from.sending() && guard++ < 128) {
            to.receive_word(peer, from.template next_wire_word<Word>());
        }
    }

} // namespace

int main() {
    using gba::bits::link::link_error;

    {
        stream_type parent;
        stream_type child;
        const value_type shortValue{std::in_place_index<0>, std::uint8_t{42}};
        const value_type longValue{
            std::in_place_index<1>,
            std::array{std::byte{1}, std::byte{0xFF}, std::byte{0xC0}, std::byte{0xDB}, std::byte{5}}
        };

        gba::test.is_true(parent.send(shortValue));
        gba::test.is_false(parent.send(shortValue));
        transfer<unsigned short>(parent, child, 0);
        gba::test.is_false(parent.sending());
        const auto shortReceived = child.read(0);
        gba::test.is_true(shortReceived.has_value());
        gba::test.is_true(*shortReceived == shortValue);

        gba::test.is_true(parent.send(longValue));
        transfer<unsigned short>(parent, child, 0);
        const auto longReceived = child.read(0);
        gba::test.is_true(longReceived.has_value());
        gba::test.is_true(*longReceived == longValue);
    }

    {
        gba::bits::link::typed_stream<test_codec, 4, 1> stream;
        const value_type value{std::in_place_index<0>, std::uint8_t{42}};
        gba::test.is_false(stream.send(value));
        gba::test.is_false(stream.sending());
    }

    {
        stream_type sender;
        stream_type receiver;
        const value_type value{std::in_place_index<0>, std::uint8_t{77}};
        gba::test.is_true(sender.send(value));

        const auto first = sender.next_wire_word<unsigned char>();
        receiver.receive_word(0, first);
        receiver.reset_peer(0, true);
        transfer<unsigned char>(sender, receiver, 0);
        gba::test.is_false(receiver.read(0).has_value());
        gba::test.is_true(receiver.has_error(0, link_error::topology_changed));

        gba::test.is_true(sender.send(value));
        transfer<unsigned char>(sender, receiver, 0);
        gba::test.is_true(receiver.read(0).has_value());
        receiver.clear_error(0, link_error::topology_changed);
        gba::test.is_false(receiver.has_error(0, link_error::topology_changed));
    }

    {
        stream_type receiver;
        unsigned int callbackCount = 0;
        receiver.on_error([&callbackCount](const std::size_t peer, const link_error error) {
            if (peer == 1 && error == link_error::receive_overflow) {
                ++callbackCount;
            }
        });
        for (std::size_t i = 0; i < 80; ++i) {
            receiver.receive_word(1, static_cast<unsigned char>(0x42));
        }
        gba::test.is_true(receiver.has_error(1, link_error::receive_overflow));
        gba::test.eq(receiver.pending_wire(1), std::size_t{64});
        gba::test.eq(callbackCount, 0u);
        receiver.dispatch_errors();
        gba::test.eq(callbackCount, 1u);
        receiver.dispatch_errors();
        gba::test.eq(callbackCount, 1u);
        receiver.report_error(1, link_error::receive_overflow);
        receiver.dispatch_errors();
        gba::test.eq(callbackCount, 2u);
    }

    {
        stream_type sender;
        stream_type receiver;
        const value_type value{std::in_place_index<0>, std::uint8_t{5}};
        gba::test.is_true(sender.send(value));
        std::array<unsigned char, 32> wire{};
        std::size_t size = 0;
        while (sender.sending()) {
            wire[size++] = sender.next_wire_word<unsigned char>();
        }
        wire[3] ^= 1;
        for (std::size_t i = 0; i < size; ++i) {
            receiver.receive_word(0, wire[i]);
        }
        gba::test.is_false(receiver.read(0).has_value());
        gba::test.is_true(receiver.has_error(0, link_error::integrity_failure));
    }

    return gba::test.finish();
}
