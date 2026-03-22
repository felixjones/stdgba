# `music_player` Reference

`music_player` is the runtime dispatcher that plays compiled music patterns created by `gba::music::compile()`.

It uses NTTP (non-type template parameter) syntax to bind a compile-time `compiled_music` value at type-instantiation time, eliminating runtime overhead.

## Include

```cpp
#include <gba/music>
```

## Type summary

```cpp
template<const gba::music::compiled_music& Music>
struct music_player {
    constexpr music_player() noexcept;
    
    bool operator()() noexcept;
};
```

## Template parameter

`Music` - A compile-time reference to a `gba::music::compiled_music` object created by `gba::music::compile()`.

Must be marked `static constexpr` to ensure it lives in ROM.

```cpp
static constexpr auto music = gba::music::compile(gba::music::note("c4 e4 g4"));
auto player = gba::music::music_player<music>{};
```

## Constructor

```cpp
constexpr music_player() noexcept;
```

Default constructor. Resets internal playback state to the start of the pattern.

## `operator()`

```cpp
bool operator()() noexcept;
```

### Behaviour

Advances the pattern by one VBlank frame (1/60 second). Dispatches any note-on, note-off, and instrument-change events scheduled for the current time.

### Return value

- **`true`** if the pattern is still playing (looped patterns always return `true`).
- **`false`** if the pattern has ended (only for non-looping patterns compiled without `loop()`).

### Call frequency

Call exactly once per frame, typically in the main game loop after `gba::VBlankIntrWait()`.

```cpp
auto player = gba::music::music_player<music>{};

while (true) {
    gba::VBlankIntrWait();
    if (!player()) {
        break;  // Pattern finished (non-looping only)
    }
    // Game logic
}
```

## Looping patterns

If the pattern is wrapped in `gba::music::loop()`, the player cycles back to the start when the sequence ends and always returns `true`.

```cpp
static constexpr auto looped = gba::music::compile(
    gba::music::loop(gba::music::note("c4 e4 g4"))
);

auto player = gba::music::music_player<looped>{};

while (true) {
    gba::VBlankIntrWait();
    player();  // Always returns true; pattern repeats forever
}
```

## Non-looping patterns

If the pattern is not wrapped in `loop()`, the player stops after the final event and returns `false`.

```cpp
static constexpr auto oneshot = gba::music::compile(
    gba::music::note("c4 e4 g4 c5")
);

auto player = gba::music::music_player<oneshot>{};

while (player()) {
    gba::VBlankIntrWait();
}
```

## Performance

- **Idle frame** (no events scheduled): ~400 cycles (~0.6% of VBlank budget)
- **4-channel batch dispatch**: ~760 cycles (~1.1% of VBlank budget)
- **Tail-call recursive dispatch**: Compile-time specialised for each timepoint, zero runtime overhead for pattern structure

## Sound setup

Before instantiating a player, enable sound output:

```cpp
#include <gba/peripherals>

gba::reg_soundcnt_x = { .master_enable = true };
gba::reg_soundcnt_l = {
    .volume_right = 7, .volume_left = 7,
    .enable_1_right = true, .enable_1_left = true,
    .enable_2_right = true, .enable_2_left = true,
    .enable_3_right = true, .enable_3_left = true,
    .enable_4_right = true, .enable_4_left = true
};
gba::reg_soundcnt_h = { .psg_volume = 2 };

static constexpr auto music = gba::music::compile(gba::music::note("c4 e4 g4"));
auto player = gba::music::music_player<music>{};
```

## Common pattern

```cpp
#include <gba/bios>
#include <gba/music>
#include <gba/peripherals>

using namespace gba::music;

int main() {
    // Enable sound
    gba::reg_soundcnt_x = { .master_enable = true };
    gba::reg_soundcnt_l = {
        .volume_right = 7, .volume_left = 7,
        .enable_1_right = true, .enable_1_left = true,
        .enable_2_right = true, .enable_2_left = true,
        .enable_3_right = true, .enable_3_left = true,
        .enable_4_right = true, .enable_4_left = true
    };
    gba::reg_soundcnt_h = { .psg_volume = 2 };

    // Compile and play
    static constexpr auto music = compile(loop(note("c4 e4 g4 c5")));
    auto player = music_player<music>{};

    while (true) {
        gba::VBlankIntrWait();
        player();
    }
}
```

## Related pages

- [Music Composition](../audio/music.md) - pattern syntax and compilation
- [Embedded WAV Samples](../audio/wav-embed.md) - waveform and sample embedding
- [Square 1 Channel](./channel-sq1.md), [Square 2 Channel](./channel-sq2.md), [Wave Channel](./channel-wav.md), [Noise Channel](./channel-noise.md) - per-channel hardware details
