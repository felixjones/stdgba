/// @file demo_wav.cpp
/// @brief WAV channel showcase - cycling through waveform timbres.
///
/// Demonstrates the GBA wave channel (sound 3) by playing the same ascending
/// scale with six different waveforms, switched via seq() instrument changes.
///
/// Waveforms:
///   1. Sine      - warm, smooth fundamental
///   2. Triangle  - bright, nasal, odd harmonics
///   3. Sawtooth  - buzzy, rich in all harmonics
///   4. Square    - hollow, retro, strong odd harmonics
///   5. Custom    - user-defined via wav_from_samples()
///   6. Piano     - embedded from Piano.wav via wav_embed()
///
/// The wave channel has no volume envelope - it plays at a fixed volume
/// (100%, 50%, 25%, or 75%-force) and must be explicitly silenced.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/music>
#include <gba/peripherals>

using namespace gba::music;
using namespace gba::music::literals;

// -- Custom waveform: organ-like (strong fundamental + third harmonic) ---------
//
// 64 x 4-bit samples (values 0-15). Packed into wav_instrument by wav_from_samples().
static constexpr auto organ = wav_from_samples({
    8,  9, 11, 12, 14, 15, 15, 14,  12, 11,  9,  8,  6,  5,  3,  2,
    3,  5,  7,  8, 10, 12, 13, 13,  12, 10,  8,  7,  5,  4,  3,  3,
    4,  6,  8,  9, 11, 13, 14, 15,  14, 13, 11,  9,  8,  6,  4,  3,
    2,  2,  3,  4,  6,  8, 10, 12,  13, 13, 12, 10,  8,  7,  5,  4,
});

// -- Embedded waveform: Piano.wav -> wav_instrument ----------------------------
//
// wav_embed(supplier) auto-extracts one waveform cycle from the sustain
// portion of Piano.wav, normalizes it, and resamples to 64 x 4-bit -
// all at compile time via #embed.
static constexpr auto piano = wav_embed([] {
    return std::to_array<unsigned char>({
#embed "Piano.wav"
    });
});

// -- Music: same melody, six timbres -----------------------------------------
//
// seq() emits instrument_change events at segment boundaries, uploading
// new wave RAM data before the first note of each segment fires.
static constexpr auto wav_demo = compile(loop(seq(
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, waves::sine),
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, waves::triangle),
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, waves::saw),
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, waves::square),
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, organ),
    note("[c4 d4 e4 f4] [g4 a4 b4 c5]").channel(channel::wav, piano)
)));

// -- Main ---------------------------------------------------------------------
int main() {
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie       = {.vblank = true};
    gba::reg_ime      = true;

    // PSG master enable
    gba::reg_soundcnt_x = {.master_enable = true};

    // Enable wave channel (sound 3) on both speakers
    gba::reg_soundcnt_l = {
        .volume_right   = 7, .volume_left    = 7,
        .enable_3_right = true,
        .enable_3_left  = true,
    };

    // PSG output volume: 100%
    gba::reg_soundcnt_h = {.psg_volume = 2};

    // Play - loops forever through the six waveforms
    music_player<wav_demo> player{};
    while (player()) {
        gba::VBlankIntrWait();
    }
}
