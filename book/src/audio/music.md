# Music Composition

stdgba includes a compile-time music composition system inspired by [Strudel](https://strudel.cc/). Patterns are written using mini-notation string literals that compile down to generator-style players -- there is zero runtime parsing or interpretation.

## Quick start

```cpp
#include <gba/music>
#include <gba/peripherals>
#include <gba/bios>

using namespace gba::music;
using namespace gba::music::literals;

int main() {
    // Initialize sound hardware
    gba::reg_soundcnt_x = { .master_enable = true };
    gba::reg_soundcnt_l = {
        .volume_right = 7, .volume_left = 7,
        .enable_1_right = true, .enable_2_right = true,
        .enable_3_right = true, .enable_4_right = true,
        .enable_1_left = true, .enable_2_left = true,
        .enable_3_left = true, .enable_4_left = true
    };
    gba::reg_soundcnt_h = { .psg_volume = 2 };

    // A simple 4-note melody on square wave channel 1
    constexpr auto melody = "c4 e4 g4 c5"_sq1;
    auto player = compile<120_bpm>(melody);

    while (player()) {
        gba::VBlankIntrWait();
    }
}
```

## Mini-notation syntax

Patterns are written as string literals with a channel suffix (`_sq1`, `_sq2`, `_wav`, `_noise`):

| Syntax | Meaning | Example |
|--------|---------|---------|
| `c4 e4` | Sequence | Notes played in order |
| `~` | Rest | Silence for one step |
| `[a b]` | Subdivision | `[c4 e4]` splits one step in half |
| `<a b>` | Timeline | `<c4 e4>` alternates per cycle |
| `a*N` | Fast | `c4*2` plays twice as fast |
| `a/N` | Slow | `c4/2` plays twice as slow |
| `a!N` | Replicate | `c4!3` = `c4 c4 c4` |
| `a(k,n)` | Euclidean | `c4(3,8)` = 3 hits over 8 steps |
| `a*<N M>` | Timeline fast | Speed cycles through values |
| `a/<N M>` | Timeline slow | Slowdown cycles through values |

## Channels

The GBA has 4 PSG sound channels. Assign patterns to channels with the string literal suffix:

```cpp
constexpr auto lead = "c5 e5 g5 c6"_sq1;      // Square wave 1 (with sweep)
constexpr auto harmony = "c4 e4 g4 c5"_sq2;    // Square wave 2
constexpr auto bass = "c3 e3 g3 c4"_wav;       // Wave channel
constexpr auto drums = "bd hh sd hh"_noise;     // Noise channel
```

### Noise channel drum sounds

The noise channel supports named drum sounds:

| Name | Sound | Name | Sound |
|------|-------|------|-------|
| `bd` | Bass drum (kick) | `lt` | Low tom |
| `sd` | Snare drum | `mt` | Mid tom |
| `hh` | Hi-hat (closed) | `ht` | High tom |
| `oh` | Open hi-hat | `cb` | Cowbell |
| `cp` | Clap | `cr` | Crash cymbal |
| `rs` | Rimshot | `rd` | Ride cymbal |

```cpp
constexpr auto beat = "bd hh sd hh"_noise;
constexpr auto fill = "bd sd sd cp"_noise;
```

### Wave channel

The wave channel requires wave data. Built-in waveforms are provided:

```cpp
// Load waveform before playing
waves::sine.load();

constexpr auto notes = "c4 e4 g4"_wav;
auto player = compile<120_bpm>(notes(waves::sine));
```

Built-in waveforms: `waves::sine`, `waves::triangle`, `waves::saw`, `waves::square`

## Multi-channel with stack()

Use `stack()` to play patterns simultaneously on multiple channels:

```cpp
constexpr auto melody = "c5 e5 g5 c6"_sq1;
constexpr auto bass = "c3 ~ g3 ~"_sq2;
constexpr auto beat = "bd hh sd hh"_noise;

auto player = compile<120_bpm>(stack(melody, bass, beat));
```

## Sequential patterns with seq()

Use `seq()` to play patterns one after another:

```cpp
constexpr auto intro = "c4 c4 c4 c4"_sq1;
constexpr auto verse = "c4 e4 g4 c5"_sq1;
constexpr auto chorus = "c5 g4 e4 c4"_sq1;

auto player = compile<120_bpm>(seq(intro, verse, chorus));
```

## Looping

Use `loop()` for infinite repetition:

```cpp
auto player = compile<120_bpm>(loop("c4 e4 g4"_sq1));
// player() always returns true

auto bgm = compile<120_bpm>(loop(stack(melody, bass, beat)));
```

## Sweep effects (SQ1 only)

SQ1 has hardware frequency sweep for pitch bend effects:

```cpp
constexpr auto laser = fast_up("c4 c5"_sq1);       // Pitch rises
constexpr auto bomb = fast_down("c5 c4"_sq1);      // Pitch falls
constexpr auto chirp = chirp_up("c4 e4 g4"_sq1);   // Very fast rise
```

## Callback events

Insert function callbacks into sequences for game synchronization:

```cpp
void on_beat() { flash_screen(); }

auto player = compile<120_bpm>(seq(
    event(on_beat),
    "c4 e4"_sq1,
    event(on_beat),
    "g4 c5"_sq1
));
```

## Compiling

`compile<BPM>(pattern)` converts a pattern into a generator-style player at compile time:

```cpp
auto player = compile<120_bpm>(melody);

// Call player() once per frame; returns false when done
while (player()) {
    gba::VBlankIntrWait();
}
```

The entire pattern compilation happens at build time. The player is a lightweight frame-stepping generator at runtime.
