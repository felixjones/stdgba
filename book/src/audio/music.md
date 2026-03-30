 # Music Composition

The GBA has four PSG (Programmable Sound Generator) channels: two square waves, one wave (sample) channel, and one noise channel. Rather than manually writing register values, stdgba lets you compose music using Strudel notation ([a text-based mini-language for patterns](https://strudel.cc/workshop/getting-started/)) and compiles it to an optimised event table at build time.

## Quick Start

```cpp
#include <gba/music>
#include <gba/peripherals>
#include <gba/bios>

using namespace gba::music;
using namespace gba::music::literals;

int main() {
    // Enable sound output
    gba::reg_soundcnt_x = { .master_enable = true };
    gba::reg_soundcnt_l = {
        .volume_right = 7, .volume_left = 7,
        .enable_1_right = true, .enable_1_left = true,
        .enable_2_right = true, .enable_2_left = true,
        .enable_3_right = true, .enable_3_left = true,
        .enable_4_right = true, .enable_4_left = true
    };
    gba::reg_soundcnt_h = { .psg_volume = 2 };

    // Compile a simple melody
    static constexpr auto music = compile(note("c4 e4 g4 c5"));

    // Play it in a loop
    auto player = music_player<music>{};
    while (player()) {
        gba::VBlankIntrWait();
    }
}
```

## Pattern Syntax

Patterns use Strudel notation. Here's the reference:

- [Strudel mini-notation reference](https://strudel.cc/learn/mini_notation/)
- [Strudel `note()` reference](https://strudel.cc/learn/synths#note)
- [Strudel `s()` reference](https://strudel.cc/learn/samples#s)

| Syntax | Meaning                                           | Example |
|--------|---------------------------------------------------|---------|
| `c4 e4 g4` | Sequence (space-separated notes)                  | `"c4 e4 g4"` |
| `~` | Rest (silence)                                    | `"c4 ~ g4"` |
| `_` | Hold/tie (sustain, no retrigger)                  | `"c4 _ _"` (hold for 3 steps) |
| `[a b]` | Subdivision (fit into one parent step)            | `"[c4 d4] e4"` |
| `<a b c>` | Alternating (cycle through each step)             | `"<c4 d4 e4>"` |
| `<a, b>` | Parallel layers (commas create stacked voices)    | `"<c4, g3>"` |
| `a@3` | Elongation (weight = 3)                           | `"c4@3 e4"` |
| `a!3` | Replicate (repeat 3 times equally)                | `"c4!3"` |
| `a*2` | Fast (play 2x in one step)                        | `"c4*2"` |
| `a/2` | Slow (stretch over 2 cycles)                      | `"c4/2"` |
| `(3,8)` | Euclidean rhythm (Bjorklund: 3 pulses in 8 steps) | `"c4(3,8)"` |
| `eb3` | Flat notation (Eb3 = D#3)                         | `"eb3 f3 g3"` |

## Creating Melodies with `note()`

`note()` is the main function for creating pitched patterns:

```cpp
// Single melody (auto-assigned to square 1)
auto melody = note("c4 e4 g4 c5");

// With modifiers
auto fast = note("c4*2 e4*2");  // Double speed
auto slow = note("c4/2");        // Stretch over 2 cycles
auto rests = note("c4 ~ ~ e4");  // With silences
```

All notes from C2 to B8 are supported. Octave-1 notes (C1-B1) are rejected at compile time because the PSG hardware cannot represent those frequencies.

## Multi-Voice Patterns with Stacking

Create parallel voices using commas inside `<>`:

```cpp
// Two voices: melody (sq1) + bass (sq2)
static constexpr auto music = compile(
    note("<c4 e4 g4 c5, c3 c3 c3 c3>")
);

// Or use the stack() combinator
static constexpr auto music = compile(
    stack(
        note("c4 e4 g4 c5"),
        note("c3 c3 c3 c3"),
        s("bd sd bd sd")  // Drums on noise channel
    )
);
```

The layers are auto-assigned to channels in order: square 1 -> square 2 -> wave -> noise.

## PSG Channels (CH1-CH4)

Use one page per channel when you need hardware details:

- [Square 1 (CH1)](./channel-sq1.md)
- [Square 2 (CH2)](./channel-sq2.md)
- [Wave (CH3)](./channel-wav.md)
- [Noise (CH4)](./channel-noise.md)

Quick inline examples:

```cpp
using namespace gba::music;
using namespace gba::music::literals;

auto lead = "c4 e4 g4 c5"_sq1;
auto bass = note("c3 c3 g2 g2").channel(channel::sq2);
auto pad = note("c4 _ g4 _").channel(channel::wav, waves::triangle);
auto drums = s("bd sd hh sd");

static constexpr auto song = compile(loop(stack(lead, bass, pad, drums)));
```

## Drums with `s()`

The `s()` function creates drum patterns using Strudel percussion names. It auto-assigns to the noise channel:

```cpp
// Kick + snare beat
auto beat = s("bd sd bd sd");

// Euclidean kick pattern
auto kick = s("bd(3,8)");

// Complex drum pattern
auto drums = s("bd [sd rim]*2 bd sd");
```

20 drum presets are supported: `bd`, `sd`, `hh`, `oh`, `cp`, `rs`, `rim`, `lt`, `mt`, `ht`, `cb`, `cr`, `rd`, `hc`, `mc`, `lc`, `cl`, `sh`, `ma`, `ag`.

## Chaining with Sequential (`seq()`)

Combine multiple patterns sequentially. Instrument changes are emitted at boundaries:

```cpp
static constexpr auto music = compile(
    loop(
        seq(
            note("c4 e4 g4 c5"),
            note("d4 f4 a4 d5"),
            note("e4 g4 b4 e5")
        )
    )
);
```

## Compile-Time Tempos

By default, `compile()` uses 0.5 cycles-per-second (120 BPM in 4/4). Override it:

```cpp
// Explicit BPM
static constexpr auto music = compile<120_bpm>(note("c4 e4 g4"));

// Or cycles-per-second
static constexpr auto music = compile<1_cps>(note("c4 e4 g4"));

// Or cycles-per-minute
static constexpr auto music = compile<30_cpm>(note("c4 e4 g4"));
```

## Pattern Functions

All patterns support transformation methods:

```cpp
auto melody = note("c4 e4 g4 c5");

melody.add(12);       // Transpose up one octave
melody.sub(5);        // Transpose down 5 semitones
melody.rev();         // Reverse the sequence
melody.ply(2);        // Stutter (repeat each note 2x)
melody.press();       // Staccato (half duration + rest)
melody.late(1, 8);    // Shift 1/8 cycle later (swing)
```

## User-Defined Literal Shorthands

For convenience, single-note assignments use UDLs:

```cpp
using namespace gba::music::literals;

auto melody = "c4 e4 g4"_sq1;   // Assign to square 1
auto bass = "c3 c3"_sq2;         // Assign to square 2
auto sample = "c4 d4"_wav;       // Use wave channel
auto drums = "bd sd hh"_s;       // Drums (noise channel)
```

## WAV Channel & Custom Waveforms

The wave channel (CH3) can play 4-bit sampled audio. Use built-in waveforms or embed `.wav` files:

For a deeper guide to `wav_embed()`, resampling limits, and custom sample authoring, see [Embedded WAV Samples](./wav-embed.md).

```cpp
// Built-in waveforms
using namespace gba::music::waves;

auto melody = note("c4 e4 g4").channel(channel::wav, sine);

// Embed a .wav file (requires C++26 #embed and GCC 15+)
static constexpr auto piano = gba::music::wav_embed([] {
    return std::to_array<unsigned char>({
#embed "Piano.wav"
    });
});

static constexpr auto music = compile(
    note("<c4 e4 g4, c3>")
        .channels(layer_cfg{channel::wav, piano}, channel::sq2)
);
```

## Playing Music

Use `music_player` with NTTP (non-type template parameter) syntax:

```cpp
static constexpr auto music = compile(note("c4 e4 g4 c5"));

auto player = music_player<music>{};  // Pass as template argument

// Play in VBlank loop
while (player()) {
    gba::VBlankIntrWait();
}
```

`music_player::operator()` returns `false` when the pattern ends (for non-looping patterns) or loops forever.

## Performance

Music playback uses tail-call recursive dispatch over compile-time batches. Per-frame cost:
- **Idle frame** (no events): ~400 cycles (~0.6% of VBlank)
- **4-channel batch dispatch**: ~760 cycles (~1.1% of VBlank)

This leaves >99% of VBlank budget for game logic.
