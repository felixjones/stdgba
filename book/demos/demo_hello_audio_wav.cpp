/// @file demo_hello_audio_wav.cpp
/// @brief Trigger a small musical jingle on the wav channel using embedded Piano.wav. (Hello Audio chapter)

#include <gba/bios>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/music>
#include <gba/peripherals>

#include <array>

using namespace gba::music;
using namespace gba::music::literals;

namespace {

    // Embed Piano.wav sample data for the wav channel (64 x 4-bit waveform).
    // The wav_embed() function parses RIFF/WAV headers and resamples to GBA format.
    static constexpr auto piano = wav_embed([] {
        return std::to_array<unsigned char>({
#embed "Piano.wav"
        });
    });

    // A simple melodic phrase played on the wav channel with embedded Piano timbre.
    // Press A to restart playback.
    // .press() applies staccato: each note plays for half duration, rest for half.
    // Compiled at 1_cps (1 cycle per second) for slower, more legato playback.
    static constexpr auto jingle = compile<1_cps>(note("c5 e5 g5 c6").channel(channel::wav, piano).press());

} // namespace

int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    // Basic PSG routing for the WAV channel on both speakers.
    gba::reg_soundcnt_x = {.master_enable = true};
    gba::reg_soundcnt_l = {
        .volume_right = 7,
        .volume_left = 7,
        .enable_4_right = true,
        .enable_4_left = true,
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
