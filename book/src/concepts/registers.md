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
