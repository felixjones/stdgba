# Registers & Peripherals

Every piece of GBA hardware -- the display, sound, timers, DMA, buttons -- is controlled through memory-mapped registers. In tonclib, these are `#define` macros to raw addresses. In stdgba, they are `inline constexpr` objects with real C++ types.

## The `registral<T>` wrapper

`registral<T>` is a zero-cost wrapper around a hardware address. It provides type-safe reads and writes through operator overloads:

```cpp
#include <gba/peripherals>

// Write a struct with designated initializers
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };

// Read the current value
auto dispcnt = gba::reg_dispcnt.value();

// Modify specific bits with compound assignment
gba::reg_dispcnt |= { .enable_obj = true };
```

### How it compiles

`registral<T>` stores the hardware address as a data member. Every operation compiles to a single `ldr`/`str` instruction -- exactly what you would write in assembly.

```cpp
// This:
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };

// Compiles to the same code as:
*gba::memory_map(gba::reg_dispcnt) = 0x0403;
```

## The `memory_map()` helper

When you need a raw pointer (for DMA, memcpy, pointer arithmetic, or interop), use `gba::memory_map(...)` instead of hard-coded addresses.

```cpp
#include <gba/peripherals>
#include <gba/video>

// Register pointer
auto* dispcnt = gba::memory_map(gba::reg_dispcnt);

// VRAM pointer (BG tile/map region)
auto* vram_bg = gba::memory_map(gba::mem_vram_bg);
```

This keeps code tied to named hardware mappings while still compiling to direct memory access.

### Read-only and write-only registers

The GBA has registers that are read-only, write-only, or read-write. stdgba encodes this in the type:

| Qualifier | Behavior |
|-----------|----------|
| `registral<T>` | Read-write |
| `registral<const T>` | Read-only |
| `registral<volatile T>` | Write-only |

For example, `gba::reg_keyinput` is read-only (you can not write to the keypad), while `gba::reg_bg_hofs` is write-only (the hardware does not let you read back scroll values).

## Array registers

Some registers are arrays (e.g., timer control, DMA channels, palette RAM):

```cpp
// Timer 0 control
gba::reg_tmcnt_h[0] = { .prescaler = 3, .enable = true };

// BG0 horizontal scroll
gba::reg_bg_hofs[0] = 120;

// Palette memory (256 BG colors + 256 OBJ colors)
gba::mem_pal_bg[0] = 0x001F; // Red
gba::mem_pal_obj[1] = 0x7C00; // Blue
```

These compile to indexed memory stores with no overhead.

## Designated initializers

The biggest ergonomic win is designated initializers. Instead of remembering which bit is which:

```c
// tonclib: which bits are these?
REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D;
```

You write self-documenting code:

```cpp
// stdgba: every field is named
gba::reg_dispcnt = {
    .video_mode = 0,
    .linear_obj_tilemap = true,
    .enable_bg0 = true,
    .enable_bg1 = true,
    .enable_obj = true,
};
```

Any field you omit defaults to zero/false. Misspelling a field name is a compile error.
