# Embedded WAV Samples

The `<gba/music>` header provides consteval WAV parsing and resampling for the GBA's wave channel (PWM output with 64??4-bit custom waveforms). Combined with C23's `#embed` directive, custom acoustic instruments and samples can be baked into the ROM at compile time.

For procedural sprite generation, see [Shapes](../utilities/shapes.md). For music composition with square-wave channels, see [Music Composition](./music.md).

## Why embed WAV samples

The GBA wave channel (CH4) plays back a 64-sample, 4-bit waveform at a frequency determined by the timer reload value. Instead of generic square/triangle/saw tones, embedded PCM samples add:

- **Acoustic instruments**: Piano, flute, bells, drums
- **Sound effects**: Explosions, coins, hits, chimes
- **Complex timbres**: Any 64-sample periodic waveform

Since the GBA only has 32 KB of EWRAM and 256 KB of WRAM, samples must be highly compressed. The 4-bit quantization and 64-sample limit constraint audio to short, punchy instruments - not long-form music or speech.

## WAV embedding API

| Function | Input                                         | Output | Use case |
|----------|-----------------------------------------------|--------|----------|
| `wav_embed()` | C-array or supplier lambda                    | `std::array<uint8_t, 64>` | Parse .wav file + resample |
| `wav_from_samples()` | `std::array<uint8_t, 64>` (4-bit values 0-15) | `std::array<uint8_t, 64>` | Direct 4-bit waveform data |
| `wav_from_pcm8()` | `const uint8_t (&data)[N]` (8-bit PCM)        | `std::array<uint8_t, 64>` | Resample 8-bit PCM to 64 samples |

All three are `consteval` and produce compile-time waveform constants.

Built-in waveforms (no file needed):

| Waveform | Access | Description |
|----------|--------|-------------|
| Sine | `gba::music::waves::sine` | Smooth sine wave |
| Triangle | `gba::music::waves::triangle` | Continuous triangle |
| Sawtooth | `gba::music::waves::saw` | Linear sawtooth |
| Square | `gba::music::waves::square` | 50% duty cycle |


## Simple example: embedded Piano

The `demo_hello_audio_wav` demo plays a four-note jingle using embedded Piano.wav:

```cpp
{{#include ../../demos/demo_hello_audio_wav.cpp:4:}}
```

Place `Piano.wav` in the demos directory. The `#embed` directive is placed on its own line inside the compound initialiser braces.

## Resampling and quantization

`wav_embed()` performs nearest-neighbor resampling: PCM samples are read from the RIFF/WAV file header (supporting mono/stereo, 8/16-bit formats) and resampled to exactly 64 x 4-bit samples for the GBA hardware. Stereo input is mixed to mono; stereo is not supported by the hardware.

Quantization from N-bit to 4-bit uses simple scaling: `(sample >> (N - 4))`. Complex samples (speech, noise) lose clarity; sine waves and simple acoustic timbres sound best.

## Built-in waveforms

For fast prototyping without external `.wav` files:

```cpp
#include <gba/music>

using namespace gba::music;

// Use compiled sine wave (always available)
auto sine_melody = compile(
    note("c4 e4 g4 c5").channel(channel::wav, waves::sine)
);

// Mix instruments: sine bass layer, square melody layer  
auto layered = compile(
    stack(
        note("c2 c2 c2 c2").channel(channel::wav, waves::sine),
        note("c5 e5 g5 c6").channel(channel::sq1)
    )
);
```

## Advanced: custom waveforms from samples

For hand-crafted 4-bit waveforms, use `wav_from_samples()`:

```cpp
#include <gba/music>

// Organ pipe sound: 64 custom 4-bit values
static constexpr auto organ = gba::music::wav_from_samples(
    std::array<uint8_t, 64>{
        // First 16 samples of a custom profile
        15, 14, 12, 10, 8, 6, 5, 4, 4, 4, 5, 6, 8, 10, 12, 14,
        // Continue pattern...
        15, 14, 12, 10, 8, 6, 5, 4, 4, 4, 5, 6, 8, 10, 12, 14,
        15, 14, 12, 10, 8, 6, 5, 4, 4, 4, 5, 6, 8, 10, 12, 14,
        15, 14, 12, 10, 8, 6, 5, 4, 4, 4, 5, 6, 8, 10, 12, 14,
    }
);

auto synth = compile(
    note("c4 e4 g4 c5").channel(channel::wav, organ)
);
```

Values are clamped to 0-15 (4-bit range). Each full period should smoothly loop back to avoid clicks at the waveform boundary.

## Practical constraints

- **64 samples maximum**: The GBA hardware uses a fixed 64-byte waveform buffer for CH4.
- **4-bit quantization**: ~24 dB dynamic range. Loud timpani and quiet pizzicato do not mix well.
- **No polyphony**: Only one waveform plays at a time on CH4. Combine with `stack()` to play multiple square-wave channels simultaneously.
- **Frequency limits**: WAV channel operates from ~32 Hz (timer reload = 255) to ~131 kHz (reload = 0). Most musical pitches fall in the 32 Hz-8 kHz range due to the timer's integer reload values.

See [Music Composition](./music.md) for combining WAV with square-wave and noise channels, and [Channel WAV/CH4](./channel-wav.md) for register-level details.
