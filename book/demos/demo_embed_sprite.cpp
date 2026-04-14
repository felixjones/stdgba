/// @file demo_embed_sprite.cpp
/// @brief Scrollable 2x1 background with a movable sprite.

#include <gba/bios>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/video>

#include <cstring>

constexpr auto bg = gba::embed::indexed4([] {
    return std::to_array<unsigned char>({
#embed "bg_2x1.png"
    });
});

constexpr auto hero = gba::embed::indexed4<gba::embed::dedup::none>([] {
    return std::to_array<unsigned char>({
#embed "sprite.png"
    });
});

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    gba::reg_dispcnt = {.video_mode = 0, .linear_obj_tilemap = true, .enable_bg0 = true, .enable_obj = true};
    gba::reg_bgcnt[0] = {.screenblock = 30, .size = 1}; // 512x256

    for (auto&& x : gba::obj_mem) {
        x = {.disable = true};
    }

    // Background palette + tiles
    std::memcpy(gba::memory_map(gba::pal_bg_mem), bg.palette.data(), sizeof(bg.palette));
    std::memcpy(gba::memory_map(gba::mem_tile_4bpp[0]), bg.sprite.data(), bg.sprite.size());

    // Background map: stored in screenblock order, memcpy directly
    std::memcpy(gba::memory_map(gba::mem_se[30]), bg.map.data(), sizeof(bg.map));

    // Sprite palette + tiles (no deduplication - sequential for 1D mapping)
    std::memcpy(gba::memory_map(gba::pal_obj_bank[0]), hero.palette.data(), sizeof(hero.palette));
    std::memcpy(gba::memory_map(gba::mem_vram_obj), hero.sprite.data(), hero.sprite.size());

    int scroll_x = 0, scroll_y = 0;
    int sprite_x = 112, sprite_y = 72;

    gba::object hero_obj = hero.sprite.obj();
    hero_obj.y = static_cast<unsigned short>(sprite_y & 0xFF);
    hero_obj.x = static_cast<unsigned short>(sprite_x & 0x1FF);
    gba::obj_mem[0] = hero_obj;

    gba::keypad keys;
    for (;;) {
        gba::VBlankIntrWait();
        keys = gba::reg_keyinput;

        if (keys.held(gba::key_a)) {
            // A + D-pad moves the sprite
            sprite_x += keys.xaxis();
            sprite_y += keys.i_yaxis();

            hero_obj.y = static_cast<unsigned short>(sprite_y & 0xFF);
            hero_obj.x = static_cast<unsigned short>(sprite_x & 0x1FF);
            gba::obj_mem[0] = hero_obj;
        } else {
            // D-pad scrolls the background
            scroll_x += keys.xaxis();
            scroll_y += keys.i_yaxis();

            gba::reg_bgofs[0][0] = static_cast<short>(scroll_x);
            gba::reg_bgofs[0][1] = static_cast<short>(scroll_y);
        }
    }
}
