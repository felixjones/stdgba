# Music Composition

stdgba includes a compile-time music composition system inspired by [Strudel](https://strudel.cc/). Patterns are defined in C++ and compile down to raw frame data arrays -- there is zero runtime parsing or interpretation.

## Quick start

```cpp
#include <gba/music>
using namespace gba::music;

// A simple 4-note melody on the square wave channel
constexpr auto melody = seq(
    atom(note::C4), atom(note::E4), atom(note::G4), atom(note::C5)
).sq1();

// Compile to frame data: 120 BPM, 4 cycles
constexpr auto song = compile<120, 4>(melody);

// Play it
auto player = make_player(song);
while (player()) {
    gba::VBlankIntrWait();
}
```

## Core pattern types

### `atom(note, channel)`

A single note lasting one cycle:

```cpp
auto c4 = atom(note::C4);    // Single note
auto rest = atom(note::rest); // Silence
```

### `seq(patterns...)`

Sequence patterns one after another in time. This is the primary way to build melodies:

```cpp
auto melody = seq(
    atom(note::C4),
    atom(note::D4),
    atom(note::E4),
    atom(note::F4),
);
```

### `stack(patterns...)`

Layer patterns simultaneously. Use this for chords or multi-channel arrangements:

```cpp
auto chord = stack(
    atom(note::C4).sq1(),
    atom(note::E4).sq2(),
    atom(note::G4).wave(),
);
```

### Time scaling

```cpp
auto fast_melody = fast<2>(melody);  // Play at 2x speed
auto slow_melody = slow<2>(melody);  // Play at half speed
```

### Reverse

```cpp
auto backwards = rev(melody);
```

### Arpeggiation

```cpp
auto arp_chord = arp(chord, arp_style::up);
```

## Channels

The GBA has 4 PSG sound channels. Assign patterns to channels with method syntax:

```cpp
auto sq1_melody = melody.sq1();    // Square wave 1
auto sq2_harmony = harmony.sq2();  // Square wave 2
auto wave_bass = bass.wave();      // Wavetable
auto noise_drums = drums.noise();  // Noise
```

## Compiling

`compile<BPM, Cycles>(pattern)` converts a pattern tree into a flat array of frame data at compile time:

```cpp
// 120 BPM, 8 cycles (each cycle = one beat)
constexpr auto song = compile<120, 8>(melody);
```

The entire compilation happens at build time. The resulting array is stored in ROM.

## Concatenation

Use `.then()` to chain compiled songs:

```cpp
constexpr auto intro = compile<120, 4>(intro_pattern);
constexpr auto verse = compile<120, 8>(verse_pattern);
constexpr auto full = intro.then(verse);
```

Prefer `seq()` within a single `compile()` call when possible -- `.then()` is for joining independently compiled sections.

## Tick rates

| Rate | Frequency | Use case |
|------|-----------|----------|
| `tick_rates::vblank` | 60 Hz | Default; simple PSG music |
| `tick_rates::timer_240` | 240 Hz | Smooth volume fades |
| `tick_rates::timer_18k` | ~18 kHz | PCM mixing (future) |
