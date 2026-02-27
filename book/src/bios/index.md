# BIOS Overview

The GBA BIOS provides SWI routines for synchronisation, math, memory transfers,
decompression, and affine setup.

In stdgba, BIOS APIs are wrapped in `#include <gba/bios>` with typed arguments.

## Categories

- [Sync and Power](./sync-and-power.md)
- [Math](./math.md)
- [Memory Copy and Fill](./memory.md)
- [Compression and Decompression](./compression.md)
- [Affine Setup](./affine.md)

## Why use BIOS calls

- They are the canonical low-level primitives on GBA.
- They match hardware behaviour and existing GBA conventions.
- stdgba wrappers remove magic mode integers where possible.

## Common pitfalls

- Use `*Vram` decompression functions for direct VRAM destinations.
- `CpuFastSet` requires 4-byte alignment and a word count.
- Keep `ArcTan2(x, y)` axis conventions consistent in your engine.
- Prefer wait SWIs (`VBlankIntrWait`, `IntrWait`) over busy-wait loops.
