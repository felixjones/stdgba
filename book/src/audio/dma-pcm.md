# DMA PCM Playback

The GBA has two Direct Sound channels for playing signed 8-bit PCM. A hardware timer sets the sample rate while DMA keeps the sound FIFO supplied without a CPU write for every sample.

## The playback pipeline

Direct Sound A consumes one sample whenever its selected timer overflows. FIFO A holds 32 bytes; when it falls to half full, the hardware asks DMA1 for another four words.

Playback requires four pieces of hardware configuration:

1. `reg_soundcnt_x` enables the sound hardware.
2. `reg_soundcnt_h` routes Direct Sound A to the speakers, selects timer 0, and clears FIFO A.
3. `dma::to_fifo_a()` configures DMA1 for repeating, FIFO-triggered word transfers.
4. Timer 0 overflows at the required sample rate.

Direct Sound samples range from `-128` to `127`, with `0` representing silence. WAV files store 8-bit PCM as unsigned values, so they must be converted before playback.

## Looping a sine wave

The first demo generates a 32-sample sine wave and streams it continuously:

```cpp
{{#include ../../demos/demo_dma_pcm_sine.cpp:4:}}
```

The GBA takes 280896 CPU cycles to draw one frame. The demo plays 96 samples per frame, giving an exact timer period:

```text
timer period = 280896 / 96 = 2926 cycles
sample rate = 16777216 / 2926 = 5734 Hz
timer reload = 65536 - 2926 = 62610
```

The 32-sample waveform repeats three times per frame. At VBlank, the interrupt handler restarts DMA1 from the beginning of the buffer without breaking the waveform's phase.

The DMA repeat flag keeps the channel active, but does not reset its source address. The sample buffer therefore includes one extra waveform period for FIFO read-ahead, preventing the final DMA burst from reading beyond the array.

## Playing an embedded WAV once

`embed::wav()` loads an uncompressed mono 8-bit PCM WAV at compile time. It converts unsigned WAV samples to signed Direct Sound samples and pads the result to a complete FIFO DMA burst.

The second demo plays `Piano.wav` when the program starts. Press `A` to play it again:

```cpp
{{#include ../../demos/demo_dma_pcm_wav.cpp:4:}}
```

Timer 1 cascades from the sample timer and raises an interrupt after the final sample. The interrupt stops both timers, disables DMA1, and clears FIFO A, making each playback one-shot.

`Piano.wav` is mono 8-bit PCM at 8363 Hz. The demo uses `embed::wav<16384>()` to linearly resample it at compile time. This gives timer 0 an exact 1024-cycle period and moves low-rate playback images above the most audible range. Omitting the template argument preserves the source sample rate.

The same WAV is used by the [Embedded WAV Samples](./wav-embed.md) demo for the hardware wave channel. That path extracts a short 4-bit waveform; `embed::wav()` retains the complete PCM recording for Direct Sound.

## Next steps

- Read [DMA Transfers](../utilities/dma.md) for the other DMA helpers.
- Read [Timers](../concepts/timers.md) for compile-time and raw timer configuration.
- See the [Sound Peripheral Reference](../reference/peripherals/sound.md) for the Direct Sound register layout.
