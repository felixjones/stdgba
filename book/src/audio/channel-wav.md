ma# Wave (CH3)

Channel 3 plays a 32x4-bit waveform (64 nibbles in stdgba's 1x64 view). Use it for softer leads, pseudo-samples, and custom timbres.

## Typical use

```cpp
#include <gba/music>

using namespace gba::music;
using namespace gba::music::waves;

auto pad = note("c4 _ g4 _").channel(channel::wav, triangle);
auto pluck = note("c5 e5 g5").channel(channel::wav, square);

static constexpr auto song = compile(loop(seq(pad, pluck)));
```

## Embedded waveform example

```cpp
static constexpr auto piano = wav_embed([] {
    return std::to_array<unsigned char>({
#embed "Piano.wav"
    });
});

auto melody = note("e5 d5 c5 b4").channel(channel::wav, piano);
```

## Notes

- CH3 ignores square/noise envelope semantics; waveform data defines the timbre.
- Built-ins live in `gba::music::waves` (`sine`, `triangle`, `saw`, `square`).
- `wav_embed()` requires a compiler that supports `#embed` (GCC 15+).
- For full sample-embedding workflow details, see [Embedded WAV Samples](./wav-embed.md).
