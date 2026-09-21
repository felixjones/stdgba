/// @file test_link_registers.cpp
/// @brief Register and local state-machine tests for typed GBA link modes.

#include <gba/codec>
#include <gba/link>
#include <gba/peripherals>
#include <gba/testing>

#include <cstddef>
#include <cstdint>

namespace {

    constexpr auto value_codec = gba::codec::uint8_codec;

} // namespace

int main() {
    using normal8 = gba::link_normal<value_codec, 64>;
    using normal32 = gba::link_normal<value_codec, 64, unsigned int>;
    using multi = gba::link_multi<value_codec, 64>;

    gba::test.eq(sizeof(gba::sio_control), sizeof(unsigned short));
    gba::test.eq(sizeof(gba::sio_multi_control), sizeof(unsigned short));
    gba::test.eq(sizeof(gba::sio_uart_control), sizeof(unsigned short));

    normal8::configure(true);
    auto normalCnt = static_cast<gba::sio_control>(gba::reg_siocnt);
    gba::test.eq(normalCnt.mode, gba::sio_mode_normal_8bit);
    gba::test.is_true(normalCnt.shift_clock);
    gba::test.is_false(normalCnt.internal_clock);
    gba::test.is_true(normalCnt.irq_enable);

    normal8::configure(false);
    normalCnt = static_cast<gba::sio_control>(gba::reg_siocnt);
    gba::test.is_false(normalCnt.shift_clock);

    normal32::configure(true);
    normalCnt = static_cast<gba::sio_control>(gba::reg_siocnt);
    gba::test.eq(normalCnt.mode, gba::sio_mode_normal_32bit);

    {
        normal8 link{true};
        gba::test.is_false(link.busy());
        gba::test.is_false(link.sending());
        gba::test.is_true(link.send(std::uint8_t{42}));
        gba::test.is_false(link.send(std::uint8_t{7}));
        link.poll();
        gba::test.is_true(link.busy());
        gba::reg_siodata8 = 0xC0;
        link.irq();
        gba::test.eq(link.pending_wire(), std::size_t{0});
        link.poll();
        gba::test.eq(link.pending_wire(), std::size_t{1});

        for (int i = 0; i < 128 && link.sending(); ++i) {
            link.irq();
            link.poll();
        }
        gba::test.is_false(link.sending());
        link.reset();
        gba::test.is_false(link.busy());
    }

    multi::configure(gba::sio_baud_57600);
    const auto multiCnt = static_cast<gba::sio_multi_control>(gba::reg_siocnt_multi);
    gba::test.eq(multiCnt.baud, gba::sio_baud_57600);
    gba::test.is_true(multiCnt.mode_multi_high);
    gba::test.is_false(multiCnt.mode_multi_low);
    gba::test.is_true(multiCnt.irq_enable);

    {
        multi link;
        gba::test.eq(link.connected_count(), std::size_t{0});
        gba::test.eq(gba::multi_players.size(), std::size_t{4});
        gba::test.eq(gba::multi_player_index(gba::multi_player::player_0), std::size_t{0});
        gba::test.eq(gba::multi_player_index(gba::multi_player::player_3), std::size_t{3});
        for (const bool connected : link.connected()) {
            gba::test.is_false(connected);
        }
    }

    auto& activeMulti = gba::link.emplace<multi>();
    gba::test.is_true(gba::link.holds<multi>());
    gba::test.is_false(gba::link.holds<normal8>());
    gba::test.eq(gba::link.get_if<multi>(), &activeMulti);
    gba::test.eq(gba::link.get_if<normal8>(), static_cast<normal8*>(nullptr));
    gba::test.eq(activeMulti.connected_count(), std::size_t{0});
    auto& activeNormal = (gba::link = normal8{true});
    gba::test.is_true(gba::link.holds<normal8>());
    gba::test.eq(gba::link.get_if<multi>(), static_cast<multi*>(nullptr));
    gba::test.eq(gba::link.get_if<normal8>(), &activeNormal);
    gba::test.is_true(activeNormal.send(std::uint8_t{42}));
    gba::test.is_false(activeNormal.send(std::uint8_t{7}));
    gba::test.is_true(activeNormal.sending());
    gba::link.reset();
    gba::test.is_false(static_cast<bool>(gba::link));

    return gba::test.finish();
}
