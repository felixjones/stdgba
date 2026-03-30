/// @file demo_timer_clock.cpp
/// @brief Visual timer demo with rotating analogue clock.
///
/// Uses compile-time timer configuration, BIOS affine sprite transforms, and the shapes API
/// to draw an analogue clock with rotating hands. The clock hands rotate in real-time based
/// on elapsed seconds measured via hardware timer interrupt.

#include <gba/angle>
#include <gba/bios>
#include <gba/color>
#include <gba/interrupt>
#include <gba/peripherals>
#include <gba/shapes>
#include <gba/timer>
#include <gba/video>

#include <array>
#include <cstdint>
#include <cstring>

using namespace std::chrono_literals;
using namespace gba::shapes;
using namespace gba::literals;
using namespace gba;

namespace {

    constexpr auto second_timer = compile_timer(1s, true);
    static_assert(second_timer.size() == 1);

    constexpr int clock_center_x = 120;
    constexpr int clock_center_y = 80;
    constexpr int sprite_half_extent = 32;

    // Clock face: visible outline, hour markers, and center hub.
    constexpr auto clock_face = sprite_64x64(palette_idx(1), circle_outline(32.0, 32.0, 30.0, 2), palette_idx(1),
                                             rect(31, 4, 2, 6), palette_idx(1), rect(31, 54, 2, 6), palette_idx(1),
                                             rect(4, 31, 6, 2), palette_idx(1), rect(54, 31, 6, 2), palette_idx(1),
                                             circle(32.0, 32.0, 2.5));

    // Hands are authored pointing straight up.
    // ObjAffineSet rotates visually anti-clockwise for positive angles, so the
    // runtime clock update negates angles to get normal clockwise clock motion.
    constexpr auto hand_hour = sprite_64x64(palette_idx(3), rect(30, 18, 4, 15));

    constexpr auto hand_minute = sprite_64x64(palette_idx(3), rect(31, 12, 2, 21));

    constexpr auto hand_second = sprite_64x64(palette_idx(2), rect(31, 8, 2, 25));

} // namespace

int main() {
    // Set up IRQ.
    std::uint32_t elapsed_seconds = 0;
    irq_handler = {[&elapsed_seconds](irq flags) {
        if (flags.timer2) {
            elapsed_seconds += 1;
        }
    }};
    reg_dispstat = {.enable_irq_vblank = true};
    reg_ie = {.vblank = true, .timer2 = true};
    reg_ime = true;

    // Start a 1-second timer on timer 2.
    reg_tmcnt[2] = second_timer[0];

    // Set up video mode 0 with sprites.
    reg_dispcnt = {
        .video_mode = 0,
        .linear_obj_tilemap = true,
        .enable_obj = true,
    };

    // Bank 0, colour 0 stays transparent for all sprites.
    pal_obj_bank[0][0] = "black"_clr;
    pal_obj_bank[0][1] = "firebrick"_clr;
    pal_obj_bank[0][2] = "lime"_clr;
    pal_obj_bank[0][3] = "royalblue"_clr;

    // Copy sprite data to OBJ VRAM using byte offsets.
    auto* objVram = reinterpret_cast<std::uint8_t*>(memory_map(mem_vram_obj));
    const auto baseTileIndex = tile_index(memory_map(mem_vram_obj));
    std::uint16_t vramOffset = 0;

    std::memcpy(objVram + vramOffset, clock_face.data(), clock_face.size());
    const auto tileIdxFace = static_cast<unsigned short>(baseTileIndex + vramOffset / sizeof(tile4bpp));
    vramOffset += static_cast<std::uint16_t>(clock_face.size());

    std::memcpy(objVram + vramOffset, hand_hour.data(), hand_hour.size());
    const auto tileIdxHour = static_cast<unsigned short>(baseTileIndex + vramOffset / sizeof(tile4bpp));
    vramOffset += static_cast<std::uint16_t>(hand_hour.size());

    std::memcpy(objVram + vramOffset, hand_minute.data(), hand_minute.size());
    const auto tileIdxMinute = static_cast<unsigned short>(baseTileIndex + vramOffset / sizeof(tile4bpp));
    vramOffset += static_cast<std::uint16_t>(hand_minute.size());

    std::memcpy(objVram + vramOffset, hand_second.data(), hand_second.size());
    const auto tileIdxSecond = static_cast<unsigned short>(baseTileIndex + vramOffset / sizeof(tile4bpp));

    auto faceObj = clock_face.obj(tileIdxFace);
    faceObj.x = clock_center_x - sprite_half_extent;
    faceObj.y = clock_center_y - sprite_half_extent;
    obj_mem[0] = faceObj;

    auto hourObj = hand_hour.obj_aff(tileIdxHour);
    hourObj.x = clock_center_x - sprite_half_extent;
    hourObj.y = clock_center_y - sprite_half_extent;
    hourObj.affine_index = 0;
    obj_aff_mem[1] = hourObj;

    auto minuteObj = hand_minute.obj_aff(tileIdxMinute);
    minuteObj.x = clock_center_x - sprite_half_extent;
    minuteObj.y = clock_center_y - sprite_half_extent;
    minuteObj.affine_index = 1;
    obj_aff_mem[2] = minuteObj;

    auto secondObj = hand_second.obj_aff(tileIdxSecond);
    secondObj.x = clock_center_x - sprite_half_extent;
    secondObj.y = clock_center_y - sprite_half_extent;
    secondObj.affine_index = 2;
    obj_aff_mem[3] = secondObj;

    // Disable remaining OAM entries.
    for (int i = 4; i < 128; ++i) {
        obj_mem[i] = {.disable = true};
    }

    std::array<object_parameters, 3> affineParams{
        {
         {.sx = 1.0_fx, .sy = 1.0_fx, .alpha = 0_deg},
         {.sx = 1.0_fx, .sy = 1.0_fx, .alpha = 0_deg},
         {.sx = 1.0_fx, .sy = 1.0_fx, .alpha = 0_deg},
         }
    };

    ObjAffineSet(affineParams.data(), memory_map(mem_obj_aff), affineParams.size(), 8);

    while (true) {
        VBlankIntrWait();

        const std::uint32_t secs = elapsed_seconds;
        const auto hours = static_cast<unsigned int>((secs / 3600U) % 12U);
        const auto mins = static_cast<unsigned int>((secs / 60U) % 60U);
        const auto secUnits = static_cast<unsigned int>(secs % 60U);

        affineParams[0].alpha = -(30_deg * hours + 0.5_deg * mins);
        affineParams[1].alpha = -(6_deg * mins + 0.1_deg * secUnits);
        affineParams[2].alpha = -(6_deg * secUnits);

        ObjAffineSet(affineParams.data(), memory_map(mem_obj_aff), affineParams.size(), 8);
    }
}
