# Sprites (Objects)

The GBA calls sprites "objects" (OBJ). Up to 128 sprites can be displayed simultaneously, each with independent position, size, palette, flipping, and priority. The hardware composites sprites automatically.

For field-by-field API details, see [`gba::object`](../reference/object.md) and [`gba::object_affine`](../reference/object-affine.md).

## OAM (Object Attribute Memory)

Sprite attributes are stored in OAM at `0x07000000`. Each entry is 8 bytes with three 16-bit attribute words (plus an affine parameter slot shared across entries).

```cpp
#include <gba/video>

// Place sprite 0 at position (120, 80), using tile 0
gba::obj_mem[0] = {
    .y = 80,
    .x = 120,
    .tile_index = 0,
};
```

> **Important:** OAM should only be written during VBlank or HBlank. Writing during the active display period can cause visual glitches. Use DMA or a shadow buffer for safe updates.

## Sprite sizes

Sprites can be various sizes by combining shape and size fields:

| Shape | Size 0 | Size 1 | Size 2 | Size 3 |
|-------|--------|--------|--------|--------|
| Square | 8x8 | 16x16 | 32x32 | 64x64 |
| Wide | 16x8 | 32x8 | 32x16 | 64x32 |
| Tall | 8x16 | 8x32 | 16x32 | 32x64 |

## Sprite tile data

Sprite tiles live in the lower portion of VRAM (starting at `0x06010000` in tile modes). Like background tiles, they can be 4bpp (16 colours) or 8bpp (256 colours) and use the object palette (`mem_pal_obj`).

## 1D vs 2D mapping

The `.linear_obj_tilemap` field in `reg_dispcnt` controls how multi-tile sprites index their tile data:

- **1D mapping (`linear_obj_tilemap = true`):** tiles are laid out sequentially in memory. A 16x16 sprite (4 tiles) uses tiles N, N+1, N+2, N+3.
- **2D mapping (`linear_obj_tilemap = false`):** tiles are laid out in a 32-tile-wide grid. A 16x16 sprite uses tiles at grid positions.

Most games use 1D mapping - it is simpler and wastes less VRAM:

```cpp
gba::reg_dispcnt = {
    .video_mode = 0,
    .linear_obj_tilemap = true,
    .enable_bg0 = true,
    .enable_obj = true,
};
```

## Hiding a sprite

Set the object disable flag to remove a sprite from the display without deleting its data:

```cpp
gba::obj_mem[0] = { .disable = true };
```

Iterators and ranges can also be used to hide multiple sprites at once:

```cpp
// Hides all sprites
std::ranges::fill(gba::obj_mem, gba::object{ .disable = true });
```

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::obj_mem[0] = { .y = 80, .x = 120, ... };` | `obj_set_attr(&oam_mem[0], ...)` |
| `gba::mem_pal_obj[n] = color;` | `pal_obj_mem[n] = color;` |
