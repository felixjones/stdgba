# Animated Sprite Sheet Type Reference

The result structure returned by `gba::embed::indexed4_sheet<FrameW, FrameH>()` holds frame-packed tile data and compile-time animation builders.

## Include

```cpp
#include <gba/embed>
```

## Sheet result type summary

```cpp
template<unsigned int FrameW, unsigned int FrameH, unsigned int Cols, unsigned int Rows, std::size_t PaletteSize>
struct sheet4_result {
    static constexpr unsigned int frame_count = Cols * Rows;
    static constexpr unsigned int tiles_per_frame = (FrameW / 8u) * (FrameH / 8u);
    static constexpr std::size_t total_tiles = frame_count * tiles_per_frame;

    std::array<gba::color, PaletteSize> palette;
    gba::sprite4<FrameW, FrameH, total_tiles> sprite;
    
    // Frame indexing
    static constexpr unsigned int tile_offset(unsigned int frame) noexcept;
    static constexpr gba::object frame_obj(unsigned short base_tile, unsigned int frame, unsigned short palette_index = 0);
    static constexpr gba::object_affine frame_obj_aff(unsigned short base_tile, unsigned int frame, unsigned short palette_index = 0);
    
    // Animation builders (return flipbook types with .frame(tick) methods)
    static consteval auto forward<Start, Count>();
    static consteval auto ping_pong<Start, Count>();
    static consteval auto sequence<"...">();
    static consteval auto row<R>();
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

Explicit frame sequence via string literal. Characters `0`-`9` map to frames 0-9; `a`-`z` continue from frame 10 upward, and `A`-`Z` map the same way as lowercase.

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
#embed "actor.png"
    });
});

// Copy tile data and palette to hardware
const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));
std::memcpy(gba::memory_map(gba::mem_vram_obj), actor.sprite.data(), actor.sprite.size());
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
