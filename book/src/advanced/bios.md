# BIOS Functions

The GBA BIOS provides built-in routines accessible through software interrupts (SWI). stdgba wraps these in C++ functions, some of which are `constexpr` - the compiler evaluates them at compile time when possible and falls back to the BIOS call at runtime.

## Common functions

### Halting and waiting

```cpp
#include <gba/bios>

// Wait for VBlank interrupt (most common - used every frame)
gba::VBlankIntrWait();

// Halt CPU until any interrupt
gba::Halt();

// Halt CPU until a specific interrupt
gba::IntrWait(true, { .vblank = true });
```

### Math

```cpp
// Square root (constexpr when argument is known at compile time)
auto root = gba::Sqrt(144u);  // 12

// Arc tangent
auto angle = gba::ArcTan2(dx, dy);

// Division (avoid - the compiler's division is usually better)
auto [quot, rem] = gba::Div(100, 7);
```

### Memory copy

```cpp
// CpuSet: 32-bit word copy/fill via BIOS
gba::CpuSet(src, dst, { .count = 256, .set_32bit = true });

// CpuFastSet: 32-bit copy in 8-word chunks (must be aligned, count multiple of 8)
gba::CpuFastSet(src, dst, { .count = 256 });
```

> **Note:** For general memory copying, prefer standard `memcpy`/`memset` - stdgba's optimised ARM assembly implementations are faster than the BIOS routines in most cases.

### Decompression

```cpp
// Decompress LZ77 data to work RAM (byte writes)
gba::LZ77UnCompWram(compressed_data, dest);

// Decompress LZ77 data to video RAM (halfword writes)
gba::LZ77UnCompVram(compressed_data, dest);

// Huffman decompression
gba::HuffUnCompReadNormal(compressed_data, dest);

// Run-length decompression
gba::RLUnCompReadNormalWrite8bit(compressed_data, dest);
```

### Reset

```cpp
// Soft reset (restart the ROM)
gba::SoftReset();  // [[noreturn]]

// Clear specific memory regions
gba::RegisterRamReset({
    .ewram = true,
    .iwram = true,
    .palette = true,
    .vram = true,
    .oam = true,
});
```

## Constexpr BIOS functions

Several BIOS math functions are `constexpr` in stdgba. When called with compile-time arguments, the compiler evaluates them directly and embeds the result:

```cpp
// Evaluated at compile time - no SWI at runtime
constexpr auto root = gba::Sqrt(256u);  // 16

// Evaluated at runtime - SWI 0x08
volatile unsigned int x = 256;
auto root2 = gba::Sqrt(x);  // BIOS call
```

This is possible because stdgba provides `constexpr` implementations of the algorithms alongside the SWI wrappers. The compiler chooses the appropriate path automatically.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::VBlankIntrWait()` | `VBlankIntrWait()` |
| `gba::Sqrt(n)` | `Sqrt(n)` |
| `gba::CpuSet(s, d, cfg)` | `CpuSet(s, d, mode)` |
| `gba::SoftReset()` | `SoftReset()` |
| `gba::ArcTan2(x, y)` | `ArcTan2(x, y)` |

The API names match the BIOS function names from the community documentation. The main difference is type safety: stdgba uses structs with named fields for configuration instead of raw integers with magic bit patterns.
