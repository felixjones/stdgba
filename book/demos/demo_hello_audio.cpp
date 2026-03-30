/// @file demo_hello_audio.cpp
/// @brief Trigger a small PSG jingle from keypad input.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/music>
#include <gba/peripherals>

using namespace gba::music;
using namespace gba::music::literals;

namespace {

    // One-shot PSG jingle (SQ1). Press A to restart playback.
    // .press() applies staccato: each note plays for half duration, rest for half.
    // Compiled at 2_cps (2 cycles per second) for a snappy tempo.
    static constexpr auto jingle = compile<2_cps>(note("c5 e5 g5 c6").channel(channel::sq1).press());

} // namespace

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    // Basic PSG routing for SQ1 on both speakers.
    gba::reg_soundcnt_x = {.master_enable = true};
    gba::reg_soundcnt_l = {
        .volume_right = 7,
        .volume_left = 7,
        .enable_1_right = true,
        .enable_1_left = true,
    };
    gba::reg_soundcnt_h = {.psg_volume = 2};

    gba::keypad keys;
    auto player = music_player<jingle>{};

    while (true) {
        gba::VBlankIntrWait();
        keys = gba::reg_keyinput;

        if (keys.pressed(gba::key_a)) {
            player = {};
        }

        player();
    }
}
