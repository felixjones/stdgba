# Memory Utilities

`<gba/memory>` collects the low-level allocation and data-layout helpers that show up repeatedly in real GBA projects:

- `bitpool` for fixed-capacity VRAM or RAM allocation
- `unique<T>` and `make_unique()` for RAII ownership
- `bitpool_buffer_resource` for `std::pmr` containers backed by a bitpool
- `plex<Ts...>` for trivially copyable tuple-like register payloads
- optimised `memcpy`, `memmove`, and `memset` wrappers tuned for ARM7TDMI

For raw VRAM addresses and palette/OAM memory maps, see [Video Memory](../reference/peripherals/video-memory.md).

## Why this module exists

The GBA gives you tight, fixed memory regions instead of a desktop-style heap:

- 32 KiB IWRAM for hot code and stack
- 256 KiB EWRAM for larger runtime data
- 32 KiB OBJ VRAM and 64 KiB BG VRAM with hardware-specific layout rules

That environment pushes you toward fixed-capacity allocators, predictable ownership, and careful copy/fill paths. `<gba/memory>` packages those patterns into APIs that stay small enough for the platform.

## API map

| API | What it does | Typical use |
|-----|--------------|-------------|
| `bitpool` | 32-chunk bitmap allocator over a caller-owned region | OBJ VRAM tiles, BG blocks, arena-style RAM |
| `bitpool::allocate()` | Raw byte allocation | Reserve tile or buffer space |
| `bitpool::allocate_unique()` | Raw allocation + RAII deallocation | Temporary VRAM ownership |
| `bitpool::make_unique()` | Placement-new object + RAII destruction | Pool-owned runtime objects |
| `bitpool::subpool()` | Carve one pool out of another | Reserve a sheet- or scene-local arena |
| `bitpool_buffer_resource` | PMR adapter over `bitpool` | `std::pmr::vector` or `std::pmr::string` |
| `unique<T>` | Small owning pointer with type-erased deleter | Resource ownership without `std::unique_ptr` |
| `plex<Ts...>` | Tuple-like object guaranteed to fit in 32 bits | Register pairs like timer reload + control |
| `memcpy` / `memmove` / `memset` | Fast wrappers over specialized AEABI back ends | Bulk transfers and clears |

## `bitpool` - a 32-chunk allocator

`bitpool` manages a contiguous region using a 32-bit mask. Each bit represents one chunk of equal size.

```text
chunk 0  chunk 1  chunk 2  ...  chunk 31
  bit0     bit1     bit2           bit31
```

That means every pool has exactly 32 allocatable chunk positions. You choose the chunk size to fit the memory region you care about.

Examples:

| Region | Total size | Sensible chunk size | Why |
|--------|------------|---------------------|-----|
| OBJ VRAM | 32 KiB | 1024 bytes | 32 chunks exactly cover the whole region |
| Small scratch arena | 4 KiB | 128 bytes | Good for many tiny fixed blocks |
| BG map staging | 8 KiB | 256 bytes | One chunk per quarter screenblock |

```cpp
#include <gba/memory>
#include <gba/video>

gba::bitpool obj_vram{gba::memory_map(gba::mem_vram_obj), 1024};

auto tiles = obj_vram.allocate_unique<unsigned char>(2048);
if (tiles) {
    std::memcpy(tiles.get(), sprite_data, 2048);
}
```

### Core queries

| Function | Meaning |
|----------|---------|
| `bitpool::capacity()` | Always 32 chunks |
| `chunk_size()` | Bytes per chunk |
| `size()` | Total bytes managed (`capacity() * chunk_size()`) |

### Raw allocation

`allocate(bytes)` rounds up to whole chunks and returns the first contiguous run that fits.

```cpp
alignas(4) unsigned char buffer[1024];
gba::bitpool pool{buffer, 32};

void* a = pool.allocate(32);  // 1 chunk
void* b = pool.allocate(64);  // 2 contiguous chunks

pool.deallocate(a, 32);
pool.deallocate(b, 64);
```

Important properties:

- allocation is simple and deterministic: scan the 32-bit mask for a free run
- deallocation is O(1): clear the matching bits
- chunk size must be a power of two
- large requests can fail if the free space is split into non-contiguous holes

So `bitpool` is not a general heap replacement. It is best when you deliberately size chunks around your asset granularity.

### Alignment-aware allocation

`allocate(bytes, chunkAlignment)` steps the search in chunk-sized increments derived from `chunkAlignment`.

```cpp
alignas(32) unsigned char buffer[256];
gba::bitpool pool{buffer, 16};

void* aligned = pool.allocate(16, 32);
```

The alignment is effectively rounded up to chunk boundaries. If your chunks are already 1024 bytes wide, asking for 4-byte alignment changes nothing.

### VRAM workflow

`bitpool` is especially useful when OBJ tile ownership changes at runtime.

```cpp
#include <gba/memory>
#include <gba/video>

gba::bitpool obj_tiles{gba::memory_map(gba::mem_vram_obj), 1024};

auto slot = obj_tiles.allocate_unique<unsigned char>(1024);
if (!slot) {
    // No room for another sprite sheet chunk
    return;
}

std::memcpy(slot.get(), sprite_sheet, 1024);
const auto tile = gba::tile_index(slot.get());
gba::obj_mem[0] = sprite.obj(tile);
```

The same pattern works well for BG VRAM, because tile graphics (4 charblocks) and screen entries (32 screenblocks) share the same 64 KiB `mem_vram_bg` region.

A convenient chunking is "one chunk per screenblock":

- 1 screenblock = 0x800 bytes (2 KiB)
- 1 charblock = 0x4000 bytes (16 KiB) = 8 screenblocks

That makes `bitpool` a good fit for allocating both tile graphics and tilemaps from one shared pool.

```cpp
#include <gba/memory>
#include <gba/video>

// BG VRAM is 64 KiB. Using 0x800-byte chunks gives exactly 32 chunks:
// one per screenblock.
gba::bitpool bg_vram{gba::memory_map(gba::mem_vram_bg), 0x800};

auto tiles = bg_vram.allocate_unique<unsigned char>(0x4000); // 1 charblock
auto map   = bg_vram.allocate_unique<unsigned char>(0x800);  // 1 screenblock

const auto cbb = gba::char_map(tiles.get());
const auto sbb = gba::screen_map(map.get());

gba::reg_bgcnt[0] = {
    .charblock = cbb,
    .screenblock = sbb,
};
```

This pattern works well for:

- allocating BG charblocks + screenblocks for background layers
- staging background tilemap uploads
- swapping sprite sets between scenes
- reserving temporary OBJ tiles for effects
- carving a VRAM upload arena out of EWRAM or VRAM

### `allocate_unique()` - raw bytes with RAII

If you want ownership without placement-new, use `allocate_unique<T>()`.

```cpp
{
    auto sprite_tiles = obj_vram.allocate_unique<unsigned char>(512);
    if (sprite_tiles) {
        std::memcpy(sprite_tiles.get(), data, 512);
    }
} // returned to the pool here
```

`T` only controls pointer type and default alignment. No constructor runs.

### `make_unique()` - construct an object in pool memory

If you want an actual object stored inside the pool, use `make_unique()`.

```cpp
struct cache_entry {
    unsigned short tile_base;
    unsigned short frame_count;
};

auto entry = obj_vram.make_unique<cache_entry>(12, 4);
```

On destruction, the object destructor runs first, then the bytes are returned to the pool.

### `subpool()` - reserve one arena inside another

Subpools let you split a parent pool into smaller lifetime domains.

```cpp
gba::bitpool obj_vram{gba::memory_map(gba::mem_vram_obj), 1024};

auto enemy_bank = obj_vram.subpool(4096, 1024);
auto boss_bank  = obj_vram.subpool(8192, 1024);
```

This is useful when one group of assets should be freed all at once. For example, a scene can own a subpool and drop the whole reservation when unloading.

Important lifetime rule:

- the parent pool must outlive every subpool created from it

## `bitpool_buffer_resource` - PMR bridge

If you want STL-like dynamic containers but still want to control exactly where the bytes come from, wrap a pool as a `std::pmr::memory_resource`.

```cpp
#include <memory_resource>
#include <vector>

alignas(4) unsigned char arena[4096];
gba::bitpool pool{arena, 128};
gba::bitpool_buffer_resource resource{pool};

std::pmr::vector<int> values{&resource};
values.push_back(1);
values.push_back(2);
values.push_back(3);
```

This does not magically remove dynamic allocation costs, but it keeps them inside a bounded arena you control.

## `unique<T>` and `make_unique()`

`gba::unique<T>` is a small owning pointer with a type-erased deleter stored inline. It is useful even outside `bitpool`, because it lets you attach custom destruction behaviour without dragging in the full standard smart-pointer machinery.

```cpp
auto owned = gba::make_unique<int>(42);
if (owned) {
    *owned = 100;
}
```

Use cases:

- ownership of pool allocations
- placement-new objects in custom arenas
- temporary wrappers around manually managed resources

## `plex<Ts...>` - tuple-like data that fits registers

`plex<Ts...>` is a trivially copyable heterogeneous aggregate that is guaranteed to fit in 32 bits. Unlike `std::tuple`, it is designed to be safe for hardware-oriented use cases such as register pairs and packed configuration values.

```cpp
#include <bit>
#include <gba/memory>

gba::plex<unsigned short, unsigned short> pair{0x1234, 0x5678};
auto [lo, hi] = pair;

auto raw = std::bit_cast<unsigned int>(pair);
```

Typical uses:

- timer reload + control (`gba::timer_config` is a `plex`)
- paired register writes
- tiny aggregate values you want to destructure with structured bindings

`plex` supports:

- 1 to 4 elements
- structured bindings via `get<I>()`
- comparisons and `swap()`
- deduction guides and `make_plex(...)`

## Optimised `memcpy`, `memmove`, and `memset`

stdgba ships custom wrappers in `source/memcpy.cpp`, `source/memmove.cpp`, and `source/memset.cpp`. They let the compiler inline small constant cases and jump straight to specialized AEABI entry points when alignment is provable.

### `memcpy`

| Specialization | Trigger |
|----------------|---------|
| No-op | `n == 0` known at compile time |
| Inline word copy | aligned source + dest, `n % 4 == 0`, `0 < n < 64` |
| Inline byte copy | `1 <= n <= 6` |
| Fast aligned AEABI path | both pointers provably word-aligned |
| Generic AEABI path | everything else |

### `memmove`

| Specialization | Trigger |
|----------------|---------|
| No-op | `n == 0` known at compile time |
| Inline overlap-safe byte move | `1 <= n <= 6` |
| Fast aligned AEABI path | both pointers provably word-aligned |
| Generic AEABI path | everything else |

### `memset`

| Specialization | Trigger |
|----------------|---------|
| No-op | `n == 0` known at compile time |
| Inline word stores | aligned destination, `n % 4 == 0`, `0 < n < 64`, constant fill byte |
| Inline byte stores | `1 <= n <= 12` |
| Fast aligned AEABI path | destination provably word-aligned |
| Generic AEABI path | everything else |

These paths matter because the ARM7TDMI is sensitive to call overhead, alignment checks, and instruction fetch bandwidth. Small constant copies and clears are common in sprite/OAM/tile code, so letting the compiler collapse them early saves cycles.

In practice you usually just call `std::memcpy`, `std::memmove`, or `std::memset` as normal. The library provides the tuned implementation underneath.

## Choosing the right tool

| Problem | Recommended tool |
|---------|------------------|
| Reserve OBJ VRAM tiles for a runtime-loaded sprite sheet | `bitpool` |
| Keep a pool allocation alive until a sprite/effect is destroyed | `allocate_unique()` |
| Construct a small object inside a bounded arena | `make_unique()` |
| Give a PMR container a fixed arena | `bitpool_buffer_resource` |
| Pack <= 32 bits of heterogenous register data | `plex` |
| Copy/fill bytes quickly | `memcpy` / `memmove` / `memset` |
