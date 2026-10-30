/// @file demo_reciprocal_floor.cpp
/// @brief Interactive perspective floor using exact reciprocal scanline division.

#include <gba/angle>
#include <gba/bios>
#include <gba/dma>
#include <gba/embed>
#include <gba/fixed_point>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/reciprocal>
#include <gba/video>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <utility>

using namespace gba::literals;

constexpr int horizon = -8;
constexpr int near_row = 8;
constexpr int focal_length = 128;
constexpr int map_size = 512;
constexpr auto row_divide =
    gba::reciprocal::corrected<gba::reciprocal::direct<near_row, gba::screen_height - 1 - horizon>>;

constexpr auto headings = [] consteval {
    std::array<std::pair<short, short>, 256> table{};
    for (unsigned i = 0; i < table.size(); ++i) {
        const gba::object_parameters parameters{.sx = 1.0_fx, .sy = 1.0_fx, .alpha = gba::angle{i << 24}};
        std::array<gba::fixed<short>, 4> rotation{};
        gba::ObjAffineSet(&parameters, rotation.data(), 1);
        table[i] = {gba::bit_cast(rotation[0]), gba::bit_cast(rotation[2])};
    }
    return table;
}();

constexpr auto floor_image = gba::embed::affine8([] {
    return std::to_array<unsigned char>({
#embed "reciprocal_floor.png"
    });
});

struct camera {
    int x = 256 * 256;
    int z = 128 * 256;
    int height = 80;
    unsigned heading = 0;
};

alignas(4) std::array<std::array<gba::background_matrix, gba::screen_height + 1>, 2> scanlines;

void move_camera(camera& view, const gba::keypad& keys) {
    const int turn = int(keys.held(gba::key_right)) - int(keys.held(gba::key_left));
    view.heading = (view.heading - turn) & 255u;
    const auto [cosine, sine] = headings[view.heading];
    const int drive = int(keys.held(gba::key_up)) - int(keys.held(gba::key_down));
    const int strafe = int(keys.held(gba::key_r)) - int(keys.held(gba::key_l));
    view.x = (view.x + (strafe * cosine - drive * sine) * 2) & (map_size * 256 - 1);
    view.z = (view.z + (drive * cosine + strafe * sine) * 2) & (map_size * 256 - 1);
    view.height = std::clamp(view.height + int(keys.held(gba::key_a)) - int(keys.held(gba::key_b)), 32, 112);
}

void project_floor(const camera& view, std::array<gba::background_matrix, gba::screen_height + 1>& rows) {
    const auto [cosine, sine] = headings[view.heading];
    const auto height_cosine = std::bit_cast<gba::fixed<int, 8>>(view.height * cosine);
    const auto height_sine = std::bit_cast<gba::fixed<int, 8>>(view.height * sine);

    for (int y = horizon + near_row; y < gba::screen_height; ++y) {
        const auto row = row_divide(y - horizon);
        const auto dx = row(height_cosine);
        const auto dz = row(height_sine);
        rows[y] = {
            .p = {gba::fixed<short>{dx}, {}, gba::fixed<short>{dz}, {}},
            .x = std::bit_cast<gba::fixed<int, 8>>(view.x) - dz * focal_length - dx * (gba::screen_width / 2),
            .y = std::bit_cast<gba::fixed<int, 8>>(view.z) + dx * focal_length - dz * (gba::screen_width / 2)
        };
    }
    rows[gba::screen_height] = rows[gba::screen_height - 1];
}

void initialize_video() {
    gba::reg_dispcnt = {.disable = true};
    std::memcpy(gba::memory_map(gba::mem_tile_8bpp[0]), floor_image.tiles.data(), sizeof(floor_image.tiles));
    std::memcpy(gba::memory_map(gba::mem_se[28]), floor_image.map.data(), sizeof(floor_image.map));
    std::memcpy(gba::memory_map(gba::pal_bg_mem), floor_image.palette.data(), sizeof(floor_image.palette));
    gba::reg_bgcnt[2] = {.priority = 1, .bpp8 = true, .screenblock = 28, .wrap_affine_tiles = true, .size = 2};
}

int main() {
    initialize_video();
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    camera view;
    gba::keypad keys;
    unsigned next = 0;
    project_floor(view, scanlines[next]);

    while (true) {
        gba::VBlankIntrWait();
        gba::reg_dmacnt_h[0] = {};
        gba::reg_bg_affine[0] = scanlines[next][0];
        // HBlank follows line y: stream row y+1, with a guard row after line 159.
        gba::reg_dma[0] = gba::dma::on_hblank(scanlines[next].data() + 1, gba::memory_map(gba::reg_bg_affine[0]),
                                              sizeof(gba::background_matrix) / sizeof(short));
        gba::reg_dispcnt = {.video_mode = 1, .enable_bg2 = true};
        next ^= 1;
        keys = gba::reg_keyinput;
        move_camera(view, keys);
        project_floor(view, scanlines[next]);
    }
}
