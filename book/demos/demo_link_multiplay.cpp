#include <gba/bios>
#include <gba/codec>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/link>
#include <gba/memory>
#include <gba/peripherals>
#include <gba/video>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace codec = gba::codec;

constexpr auto p1_sheet = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "link_multiplay_p1.png"

    });
});
constexpr auto p2_sheet = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "link_multiplay_p2.png"

    });
});
constexpr auto p3_sheet = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "link_multiplay_p3.png"

    });
});
constexpr auto p4_sheet = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "link_multiplay_p4.png"

    });
});
using sheet_type = decltype(p1_sheet);

constexpr std::size_t link_capacity = 16; // Reserve enough wire storage for one framed player update

enum facing : unsigned char {
    facing_down = 0,
    facing_right = 1,
    facing_up = 2,
    facing_left = 3
};

struct player {
    unsigned char x = 0;
    unsigned char y = 0;
    unsigned char dir = facing_down;
    unsigned char anim = 0;
    gba::object obj{};

    constexpr player() = default;
    constexpr player(unsigned char x, unsigned char y, unsigned char dir, unsigned char anim);
    constexpr player(unsigned char x, unsigned char y, gba::object obj);

    static constexpr auto codec = codec::make_tuple_codec(codec::member<&player::x>(codec::uint8_codec),
                                                          codec::member<&player::y>(codec::uint8_codec),
                                                          codec::member<&player::dir>(codec::uint8_codec),
                                                          codec::member<&player::anim>(codec::uint8_codec))
                                      .apply<player>();

    void move(int dx, int dy, unsigned int frame);
    void apply(const player& state);
    const gba::object& draw(const gba::object& base, bool visible);
};

std::array<player, 4> make_players(const std::array<gba::object, 4>& objects);

int main() {
    gba::bitpool objVram{gba::memory_map(gba::mem_vram_obj), 1024};
    const auto upload = [&objVram](const auto& sheet, const unsigned short palIdx) {
        std::memcpy(gba::memory_map(gba::pal_obj_bank[palIdx]), sheet.palette.data(), sizeof(sheet.palette));
        void* dest = objVram.allocate(sheet.sprite.size());
        std::memcpy(dest, sheet.sprite.data(), sheet.sprite.size());
        return sheet.frame_obj(gba::tile_index(dest), 0, palIdx);
    };
    const std::array baseObj{
        upload(p1_sheet, 0),
        upload(p2_sheet, 1),
        upload(p3_sheet, 2),
        upload(p4_sheet, 3),
    };

    // Configure the serial port for typed four-player communication
    auto& link = gba::link.emplace<gba::link_multi<player::codec, link_capacity>>(gba::sio_baud_115200);

    std::ranges::fill(gba::obj_mem, gba::object{.disable = true});
    gba::reg_dispcnt = {.linear_obj_tilemap = true, .enable_obj = true};
    gba::irq_handler = [&link](const gba::irq flags) {
        if (flags.serial) {
            link.irq(); // Capture the completed transfer and stage the next link word
        }
    };
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true, .serial = true}; // Enable the serial link interrupt
    gba::reg_ime = true;

    // Wait until at least one remote player joins the link
    while (link.connected_count() < 2) {
        link.poll(); // Process link transfers and start the next multiplayer round
    }
    const auto myPlayer = link.player();

    auto players = make_players(baseObj);

    unsigned int frame = 0;
    gba::keypad keys;

    while (true) {
        ++frame;

        keys = gba::reg_keyinput;
        const int dx = keys.xaxis();
        const int dy = keys.i_yaxis();
        players[gba::multi_player_index(myPlayer)].move(dx, dy, frame);

        // Do not replace a player update that is still being transmitted
        if (!link.sending()) {
            link.send(players[gba::multi_player_index(myPlayer)]); // Encode and queue the local player's latest state
        }

        // Check each possible remote player's independent receive stream
        for (const auto player : gba::multi_players) {
            if (player == myPlayer) {
                continue; // The local state is authoritative and never arrives over the link
            }
            // Drain all complete updates received from this remote player
            while (auto incoming = link.read(player)) {
                players[gba::multi_player_index(player)].apply(*incoming);
            }
        }

        while (gba::reg_vcount < 159) {
            link.poll(); // Keep the link moving while waiting for the safe OAM update period
        }

        gba::VBlankIntrWait();

        // Snapshot which player slots currently have a connected system
        const auto connected = link.connected();
        for (std::size_t id = 0; id < 4; ++id) {
            // Hide remote sprites whose systems are not connected
            gba::obj_mem[id] =
                players[id].draw(baseObj[id], id == gba::multi_player_index(myPlayer) || connected[id]);
        }

        while (gba::reg_vcount >= 160) {
            link.poll(); // Continue servicing the link throughout the remainder of VBlank
        }
    }
}

constexpr int max_x = 240 - sheet_type::frame_width;
constexpr int max_y = 160 - sheet_type::frame_height;
constexpr int spawn_inset = 40;
constexpr std::array walk_table{0, 1, 0, 2};

constexpr int clampi(const int value, const int low, const int high) noexcept {
    return value < low ? low : (value > high ? high : value);
}

constexpr player::player(const unsigned char x, const unsigned char y, const unsigned char dir,
                         const unsigned char anim)
    : x(x), y(y), dir(dir), anim(anim) {}

constexpr player::player(const unsigned char x, const unsigned char y, const gba::object obj) : x(x), y(y), obj(obj) {}

void player::move(const int dx, const int dy, const unsigned int frame) {
    if (dx != 0) {
        dir = dx > 0 ? facing_right : facing_left;
    } else if (dy != 0) {
        dir = dy > 0 ? facing_down : facing_up;
    }
    x = static_cast<unsigned char>(clampi(x + dx, 0, max_x));
    y = static_cast<unsigned char>(clampi(y + dy, 0, max_y));
    anim = dx != 0 || dy != 0 ? walk_table[(frame / 10) % 4] : 0;
}

void player::apply(const player& state) {
    x = state.x;
    y = state.y;
    dir = state.dir;
    anim = state.anim;
}

const gba::object& player::draw(const gba::object& base, const bool visible) {
    obj.disable = !visible;
    if (visible) {
        const unsigned int frame = static_cast<unsigned int>(dir) * sheet_type::columns + anim;
        obj.tile_index = base.tile_index + sheet_type::tile_offset(frame);
        obj.x = static_cast<unsigned short>(x);
        obj.y = static_cast<unsigned short>(y);
    }
    return obj;
}

std::array<player, 4> make_players(const std::array<gba::object, 4>& objects) {
    constexpr std::array spawn_positions{
        std::array{        spawn_inset,         spawn_inset},
        std::array{max_x - spawn_inset,         spawn_inset},
        std::array{        spawn_inset, max_y - spawn_inset},
        std::array{max_x - spawn_inset, max_y - spawn_inset},
    };

    return {
        player{spawn_positions[0][0], spawn_positions[0][1], objects[0]},
        player{spawn_positions[1][0], spawn_positions[1][1], objects[1]},
        player{spawn_positions[2][0], spawn_positions[2][1], objects[2]},
        player{spawn_positions[3][0], spawn_positions[3][1], objects[3]},
    };
}
