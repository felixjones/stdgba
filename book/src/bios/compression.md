# Compression and Decompression

BIOS decompression routines read GBA headered compressed streams.

## WRAM targets

```cpp
#include <gba/bios>

gba::LZ77UnCompWram(lz_src, ewram_dst);
gba::HuffUnComp(huff_src, ewram_dst);
gba::RLUnCompWram(rl_src, ewram_dst);
```

## VRAM-safe targets

```cpp
#include <gba/bios>

gba::LZ77UnCompVram(lz_src, vram_dst);
gba::RLUnCompVram(rl_src, vram_dst);
```

Use VRAM variants for direct VRAM writes.

## Header requirement

Compressed input must begin with `gba::uncompress_header`.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::LZ77UnCompWram(src, dst)` | `LZ77UnCompWram(src, dst)` |
| `gba::LZ77UnCompVram(src, dst)` | `LZ77UnCompVram(src, dst)` |
| `gba::HuffUnComp(src, dst)` | `HuffUnComp(src, dst)` |
| `gba::RLUnCompWram(src, dst)` | `RLUnCompWram(src, dst)` |
| `gba::RLUnCompVram(src, dst)` | `RLUnCompVram(src, dst)` |
