# DMA PCM Sine Wave

The GBA has two Direct Sound channels for playing signed 8-bit PCM samples. This demo uses DMA and a hardware timer to play a continuous sine wave through Direct Sound A.

## The code

```cpp
{{#include ../../demos/demo_dma_pcm_sine.cpp:4:}}
```

## What is happening?

Direct Sound A consumes one signed 8-bit PCM sample whenever its selected timer overflows. Its FIFO holds 32 bytes. When the FIFO falls to half full, the hardware asks DMA1 for another four 32-bit words, so the CPU does not have to write every sample.

### Step by step

1. `reg_soundcnt_x` enables the sound hardware.
2. `reg_soundcnt_h` routes Direct Sound A to both speakers at full volume, selects timer 0, and clears FIFO A.
3. `dma::to_fifo_a()` configures DMA1 for repeating, FIFO-triggered 32-bit transfers.
4. Timer 0 overflows once per PCM sample.
5. The VBlank interrupt disables and re-enables DMA1 so its source address returns to the start of the sample buffer.

## Sample data

Direct Sound uses signed 8-bit samples. `-128` is the negative peak, `0` is silence, and `127` is the positive peak.

The demo stores one sine-wave period as 32 samples. `make_sample_buffer()` repeats that period to produce the data consumed during each video frame.

## Sample rate

The GBA takes 280896 CPU cycles to draw one frame. Dividing that by 96 samples gives an exact timer period of 2926 cycles:

```text
sample rate = 16777216 / 2926 = 5734 Hz
timer reload = 65536 - 2926 = 62610
```

Timer 0 reloads automatically after every overflow, producing a steady stream of sample requests without CPU timing loops.

## Looping the sample

FIFO DMA's repeat flag keeps the channel active, but it does not reset the source address. Without the VBlank handler, DMA1 would continue past the end of the sample buffer and play unrelated ROM data.

The 32-sample waveform repeats three times in each 96-sample frame. Rewinding DMA at VBlank therefore returns to the same point in the waveform and avoids a click at the loop boundary.

The buffer contains one extra 32-sample period because FIFO DMA reads ahead of playback. This padding prevents the final DMA burst from reading beyond the array.

## Using other PCM data

Replace `sine_period` with other signed 8-bit PCM data and adjust the timer reload value for its sample rate. The source must remain valid and readable while DMA1 is enabled.

For longer or generated audio, place the samples in RAM and restart DMA at known buffer boundaries.

## Next steps

- Read [DMA Transfers](../utilities/dma.md) for the other DMA helpers.
- Read [Timers](../concepts/timers.md) for compile-time and raw timer configuration.
- See the [Sound Peripheral Reference](../reference/peripherals/sound.md) for the Direct Sound register layout.
