# Animated Sprite Sheet Type Reference

The result structure returned by `gba::embed::indexed4_sheet<FrameW, FrameH>()` holds frame-packed tile data and compile-time animation builders.

## Include

```cpp
#include <gba/embed>
```

## Sheet result type summary

```cpp
template<std::size_t FrameCount, std::size_t TileCount>
struct sheet4_result {
    std::array<gba::color, 16> palette;
    std::array<unsigned int, TileCount> sprite;
    
    // Frame indexing
    constexpr std::size_t tile_offset(std::size_t frame) const;
    gba::object frame_obj(unsigned short base_tile, std::size_t frame, unsigned short palette_index = 0) const;
    gba::object_affine frame_obj_aff(unsigned short base_tile, std::size_t frame, unsigned short palette_index = 0) const;
    
    // Animation builders (return flipbook types with .frame(tick) methods)
    constexpr auto forward<Start, Count>() const;
    constexpr auto ping_pong<Start, Count>() const;
    constexpr auto sequence<"...">() const;
    constexpr auto row<R>() const;
};
```

## Members

- `palette` - 16-colour OBJ palette shared across all frames
- `sprite` - frame-packed 4bpp tile payload ready for OBJ VRAM upload

## Frame access

### `tile_offset(frame)`

Returns the tile offset (in tiles, not bytes) for a given frame. Used when manually managing OBJ VRAM layout.

```cpp
const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));
auto offset = actor.tile_offset(frame_index);
gba::obj_mem[0].tile_index = base_tile + offset;
```

### `frame_obj(base_tile, frame, palette_index)`

Returns a regular (non-affine) `gba::object` entry for a specific frame.

```cpp
gba::obj_mem[0] = actor.frame_obj(base_tile, current_frame, 0);
gba::obj_mem[0].x = 120;
gba::obj_mem[0].y = 80;
```

### `frame_obj_aff(base_tile, frame, palette_index)`

Returns an affine `gba::object_affine` entry for a specific frame.

```cpp
gba::obj_aff_mem[0] = actor.frame_obj_aff(base_tile, current_frame, 0);
gba::obj_aff_mem[0].affine_index = 0;
```

## Animation builders

All animation builders are compile-time helpers that return a flipbook type with a `.frame(tick)` method.

### `forward<Start, Count>()`

Compile-time sequential flipbook: frames play in order once.

```cpp
static constexpr auto idle = actor.forward<0, 4>();

unsigned int frame = idle.frame(tick / 8);  // Cycles: 0, 1, 2, 3, 0, 1, 2, 3, ...
```

### `ping_pong<Start, Count>()`

Compile-time forward-then-reverse flipbook: frames play forward, then reverse (excluding the endpoints to avoid doubling them).

```cpp
static constexpr auto walk = actor.ping_pong<0, 4>();

unsigned int frame = walk.frame(tick / 8);  // Cycles: 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, ...
```

### `sequence<"...">()`

Explicit frame sequence via string literal. Characters `0`-`9` map to frames 0-9; `a`-`z` continue from frame 10 upward.

```cpp
static constexpr auto attack = actor.sequence<"01232100">();

unsigned int frame = attack.frame(tick / 10);  // Cycles through the specified sequence
```

### `row<R>()`

Returns a row-scoped builder for multi-row sprite sheets (e.g., one direction per row).

```cpp
static constexpr auto down  = actor.row<0>().ping_pong<0, 3>();
static constexpr auto left  = actor.row<1>().ping_pong<0, 3>();
static constexpr auto right = actor.row<2>().ping_pong<0, 3>();
static constexpr auto up    = actor.row<3>().ping_pong<0, 3>();
```

The result is still a sheet-global frame index, so it plugs directly into `frame_obj()`.

## Flipbook `.frame(tick)` method

All animation builders return a flipbook type with:

```cpp
constexpr std::size_t frame(std::size_t tick) const;
```

This maps a monotonically-increasing tick value to a frame index within the animation sequence.

```cpp
unsigned int tick = 0;
const auto walk = actor.ping_pong<0, 4>();

while (true) {
    gba::VBlankIntrWait();
    unsigned int frame = walk.frame(tick / 8);  // Update every 8 ticks
    gba::obj_mem[0] = actor.frame_obj(base_tile, frame, 0);
    ++tick;
}
```

## Sheet layout

Frames are laid out contiguously in OBJ VRAM. The converter ensures:

- whole sheet uses one shared 15-colour palette + transparent index 0
- frames are tile-aligned for simple `base_tile + tile_offset(frame)` indexing
- no runtime repacking is needed

## Upload pattern

```cpp
#include <algorithm>
#include <cstring>
#include <gba/embed>

static constexpr auto actor = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "actor.tga"
    });
});

// Copy tile data and palette to hardware
const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));
std::memcpy(gba::memory_map(gba::mem_vram_obj), actor.sprite.data(), actor.sprite.size() * sizeof(actor.sprite[0]));
std::copy(actor.palette.begin(), actor.palette.end(), gba::pal_obj_bank[0]);

// Use frame_obj() to create OAM entries
auto walk = actor.ping_pong<0, 4>();
gba::obj_mem[0] = actor.frame_obj(base_tile, walk.frame(tick / 8), 0);
```

## Constraints

- all frames must fit within one 15-colour palette (index 0 always transparent)
- frame dimensions must match a legal GBA OBJ size
- frame width x height must divide the source image evenly

Violations are rejected at compile time.

## Related pages

- [Embedding Images](../graphics/embed.md)
- [Animated Sprite Sheets](../graphics/embed-animated.md)
- [Embedded Sprite Type Reference](./embed-sprite.md)
- [`gba::object` Reference](./object.md)
