# Registers & Peripherals

Every piece of GBA hardware - the display, sound, timers, DMA, buttons - is controlled through memory-mapped registers. In tonclib, these are `#define` macros to raw addresses. In stdgba, they are `inline constexpr` objects with real C++ types.

## The `registral<T>` wrapper

`registral<T>` is a zero-cost wrapper around a hardware address. It provides type-safe reads and writes through operator overloads:

```cpp
#include <gba/peripherals>

// Write a struct with designated initialisers
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };

// Read the current value
auto dispcnt = gba::reg_dispcnt.value();

// Write a raw integer directly (for non-integral register types)
gba::reg_dispcnt = 0x0403u;
```

### How it compiles

`registral<T>` stores the hardware address as a data member. Every operation compiles to a single `ldr`/`str` instruction - exactly what you would write in assembly.

```cpp
// This:
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };

// Compiles to the same code as:
*(volatile uint16_t*) 0x4000000 = 0x0403u;
```

### Writing raw integers

When a register stores a non-integral type (a struct with bitfields), you can still write a raw integer value when needed:

```cpp
// Normal: designated initialiser
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };

// Raw: write an integer directly
gba::reg_dispcnt = 0x0403u; // Same effect, but less readable
```

This allows some compatibility with tonclib and similar C libraries that treat registers as raw integers.

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

| Qualifier | Behaviour |
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

// Palette memory (256 BG colours + 256 OBJ colours)
gba::mem_pal_bg[0] = 0x001F; // Red
gba::mem_pal_obj[1] = 0x7C00; // Blue
```

These compile to indexed memory stores with no overhead.

### Using std algorithms with array registers

Array registers support range-based iteration and are compatible with `<algorithm>`:

```cpp
#include <algorithm>
#include <gba/peripherals>

// Initialise all 4 timers to zero
std::fill(gba::reg_tmcnt_l.begin(), gba::reg_tmcnt_l.end(), 0);

// Copy a preset palette from EWRAM into OBJ palette
std::copy(preset_palette.begin(), preset_palette.end(), gba::mem_pal_obj.begin());

// Check if any timer is running
bool any_running = std::any_of(gba::reg_tmcnt_h.begin(), gba::reg_tmcnt_h.end(),
    [] (auto tmcnt) { return tmcnt.enabled; });

// Initialise all background control registers at once
std::fill(gba::reg_bgcnt.begin(), gba::reg_bgcnt.end(),
          gba::background_control{.priority = 0, .screenblock = 31});
```

The array wrapper provides standard range interface: `.begin()`, `.end()`, `.size()`, and forward iterators compatible with all `<algorithm>` calls.

## `registral_cast`

When you need to access the same memory region through a different type - for example, interpreting palette RAM as typed `color` entries rather than raw `short` values - use `gba::registral_cast`.

```cpp
#include <gba/color>

// mem_pal_bg is registral<short[256]> (raw shorts)
// pal_bg_mem is the same address, reinterpreted as color[256]
inline constexpr auto pal_bg_mem = gba::registral_cast<gba::color[256]>(gba::mem_pal_bg);
```

The cast preserves the hardware address and stride. It works for all combinations:

| From      | To        | Example                                       |
|-----------|-----------|-----------------------------------------------|
| Non-array | Non-array | `registral_cast<color>(raw_short_reg)`        |
| Non-array | Array     | `registral_cast<color[4]>(raw_reg)`           |
| Array     | Array     | `registral_cast<color[256]>(short_array_reg)` |
| Array     | Non-array | `registral_cast<color>(color_array_reg)`      |

### Palette example

```cpp
using namespace gba::literals;

// Write palette entries as typed colors
gba::pal_bg_mem[0] = "#000000"_clr;  // transparent/backdrop
gba::pal_bg_mem[1] = "red"_clr;

// 4bpp: access as 16 banks of 16 colours each
gba::pal_bg_bank[0][0] = "black"_clr;
gba::pal_bg_bank[1][3] = "cornflowerblue"_clr;
```

### VRAM example

```cpp
#include <gba/video>

// VRAM as typed tile arrays
auto tile_ptr = gba::memory_map(gba::mem_tile_4bpp);
// Equivalent to registral_cast internally:
// registral<tile4bpp[4][512]> at 0x6000000
```

`registral_cast` is a zero-cost cast: it produces a new `registral<To>` at exactly the same base address, with no runtime overhead.

## Designated initialisers

The biggest ergonomic win is designated initialisers. Instead of remembering which bit is which:

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

Any field you omit will use sensible default values.
