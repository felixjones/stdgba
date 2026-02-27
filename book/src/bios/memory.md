# Memory Copy and Fill

BIOS provides two transfer helpers for setup and bulk memory operations.

## CpuSet

```cpp
#include <gba/bios>

// count is transfer units (halfwords for 16-bit, words for 32-bit).
gba::CpuSet(src, dst, {.count = 256, .fill = false, .set_32bit = true});
```

`CpuSet` supports copy and fill, in 16-bit or 32-bit mode.

## CpuFastSet

```cpp
#include <gba/bios>

// 32-bit only, source and destination must be 4-byte aligned.
gba::CpuFastSet(src32, dst32, {.count = 256, .fill = false});
```

`CpuFastSet` is word-oriented and alignment-sensitive.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::CpuSet(src, dst, cfg)` | `CpuSet(src, dst, mode)` |
| `gba::CpuFastSet(src, dst, cfg)` | `CpuFastSet(src, dst, mode)` |
