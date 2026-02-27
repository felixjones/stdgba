# EWRAM & IWRAM Overlays

The GBA has two work RAM regions:

- **EWRAM** (256 KB at `0x02000000`) -- external, 16-bit bus, 2 wait states
- **IWRAM** (32 KB at `0x03000000`) -- internal, 32-bit bus, 0 wait states

Both regions are limited. Overlays let you swap different data or code into the same RAM region at runtime, effectively multiplying the usable space.

## How overlays work

The toolchain linker script defines 10 overlay slots for each region (`.ewram0`-`.ewram9` and `.iwram0`-`.iwram9`). All overlays of the same type share the same RAM address -- only one can be active at a time. The initialization data for each overlay is stored separately in ROM.

```text
ROM:   [overlay 0 data] [overlay 1 data] [overlay 2 data] ...
         |                  |
         v                  v
RAM:   [ shared region ] -- only one at a time
```

## Placing data in overlays

Use the `[[gnu::section]]` attribute:

```cpp
// Level 1 map data in EWRAM overlay 0
[[gnu::section(".ewram0")]]
int level1_map[1024] = { /* ... */ };

// Level 2 map data in EWRAM overlay 1
[[gnu::section(".ewram1")]]
int level2_map[1024] = { /* ... */ };
```

Alternatively, name source files with the overlay pattern (e.g., `level1.ewram0.cpp`) and the linker will route their `.text` sections automatically.

## Getting overlay metadata

`<gba/overlay>` provides section descriptors with ROM source, RAM destination, and size -- but does not perform the copy. You choose how to load:

```cpp
#include <gba/overlay>

auto ov = gba::overlay.ewram.section<0>();
// ov.rom   -- pointer to initialization data in ROM
// ov.ram   -- pointer to shared RAM destination
// ov.words -- size in 32-bit words
```

The template parameter provides compile-time bounds checking: `section<10>()` is a compile error.

## Loading overlays

You pick the copy method that suits your situation:

```cpp
#include <gba/overlay>
#include <gba/bios>
#include <gba/dma>
#include <cstring>

auto ov = gba::overlay.ewram.section<0>();

// Option 1: CpuSet (BIOS, always available)
gba::CpuSet(ov.rom, ov.ram, {.count = ov.words, .set_32bit = true});

// Option 2: memcpy (stdgba's optimized ARM assembly)
std::memcpy(ov.ram, ov.rom, ov.words * 4);

// Option 3: DMA (zero CPU time, good for large overlays)
gba::reg_dma[3] = gba::dma::copy(ov.rom, ov.ram, ov.words);
```

### Switching overlays

Loading a new overlay into the same region simply overwrites the previous one:

```cpp
// Load level 1 data
auto ov0 = gba::overlay.ewram.section<0>();
std::memcpy(ov0.ram, ov0.rom, ov0.words * 4);
// level1_map is now accessible

// Switch to level 2 (replaces level 1 in RAM)
auto ov1 = gba::overlay.ewram.section<1>();
std::memcpy(ov1.ram, ov1.rom, ov1.words * 4);
// level2_map is now accessible (level1_map is overwritten)
```

## IWRAM code overlays

IWRAM is fast -- ARM code runs at full speed with no wait states. Use IWRAM overlays to swap performance-critical code modules:

```cpp
// In physics.iwram0.cpp -- placed in overlay 0 automatically
void physics_update() { /* hot loop */ }

// In render.iwram1.cpp -- placed in overlay 1 automatically
void render_scene() { /* hot loop */ }
```

```cpp
// Load physics code into IWRAM and run it
auto ov = gba::overlay.iwram.section<0>();
gba::CpuSet(ov.rom, ov.ram, {.count = ov.words, .set_32bit = true});
physics_update();

// Swap in rendering code
auto ov1 = gba::overlay.iwram.section<1>();
gba::CpuSet(ov1.rom, ov1.ram, {.count = ov1.words, .set_32bit = true});
render_scene();
```

Both functions occupy the same IWRAM addresses but contain different code. Only one can be called at a time.

> **Warning:** calling a function from an overlay that is not currently loaded will execute whatever garbage is in RAM. Always load before calling.

## Region base address

Each handle also provides the shared base address:

```cpp
void* ewram_base = gba::overlay.ewram.data();  // 0x02000000 region
void* iwram_base = gba::overlay.iwram.data();  // 0x03000000 region
```
