/// @file demo_tetris.cpp
/// @brief Tetris Theme A - Strudel mini-notation on GBA PSG with drums.
///
/// Three-voice arrangement: melody (WAV/Piano), bass (SQ2), drums (noise).
///
/// Original Strudel source (https://strudel.cc/tutorial/#tetris):
///   note(`< [e5 [b4 c5] ...] , [[e2 e3]*4] ... >`)
///
/// GBA adaptation notes:
///   * .channels() overrides auto-assignment: melody -> WAV (Piano.wav timbre),
///     bass -> SQ2. Without .channels(), commas in <> auto-assign sq1, sq2.
///   * Piano.wav is embedded at compile time via #embed and converted to
///     a 64x4-bit waveform for the GBA wave channel by wav_embed().
///   * g#2 / g#3 use the # sharp notation supported by the parser.
///   * Original Strudel bass uses a1, b1 (below GBA PSG minimum ~64 Hz).
///     These are raised to a2, b2 - the hardware cannot produce sub-C2 pitches
///     and the compiler rejects them at compile time.
///   * Tempo: Strudel default (0.5 cps = 120 BPM in 4/4). No setcps() call
///     needed - stdgba's compile() uses the same default.


#include <gba/bios>
#include <gba/interrupt>
#include <gba/music>
#include <gba/peripherals>

using namespace gba::music;
using namespace gba::music::literals;

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

// -- Tetris Theme A ------------------------------------------------------------
//
// Three voices: melody (WAV/Piano), bass (SQ2), drums (noise).
// .channels() assigns: layer 0 (melody) -> WAV with Piano timbre,
//                       layer 1 (bass)   -> SQ2 with default instrument.
// stack() adds the drum layer on the noise channel.
static constexpr auto tetris_music = compile(loop(stack(
    note(R"(<
[e5 [b4 c5] d5 [c5 b4]]
[a4 [a4 c5] e5 [d5 c5]]
[b4 [~ c5] d5 e5]
[c5 a4 a4 ~]
[[~ d5] [~ f5] a5 [g5 f5]]
[e5 [~ c5] e5 [d5 c5]]
[b4 [b4 c5] d5 e5]
[c5 a4 a4 ~]
,
[[e2 e3]*4]
[[a2 a3]*4]
[[g#2 g#3]*2 [e2 e3]*2]
[a2 a3 a2 a3 a2 a3 b2 c2]
[[d2 d3]*4]
[[c2 c3]*4]
[[b2 b3]*2 [e2 e3]*2]
[[a2 a3]*4]
>)").channels(layer_cfg{channel::wav, piano}, channel::sq2),
    s("bd sd bd sd")
)));


// -- Main ---------------------------------------------------------------------
int main() {
    // -- Interrupt setup (required for VBlankIntrWait) ------------------------
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie       = {.vblank = true};
    gba::reg_ime      = true;

    // -- PSG master enable -----------------------------------------------------
    gba::reg_soundcnt_x = {.master_enable = true};

    // Enable WAV (sound 3) + SQ2 (sound 2) + noise (sound 4) on both speakers.
    // Melody uses the wave channel with Piano.wav timbre via .channels().
    gba::reg_soundcnt_l = {
        .volume_right   = 7, .volume_left    = 7,
        .enable_2_right = true, .enable_3_right = true, .enable_4_right = true,
        .enable_2_left  = true, .enable_3_left  = true, .enable_4_left  = true,
    };

    // PSG output volume: 100% (psg_volume = 2).
    gba::reg_soundcnt_h = {.psg_volume = 2};

    // -- Play -----------------------------------------------------------------
    // compile<>() emits instrument_change events at t=0 for WAV and SQ2
    // (from .channels() overrides), so both channels are initialised before
    // the first note fires. The Piano waveform is uploaded to wave RAM
    // during the WAV instrument_change.
    // With looping = true the player never returns false - it runs forever.
    music_player<tetris_music> player{};
    while (player()) {
        gba::VBlankIntrWait();
    }
}
