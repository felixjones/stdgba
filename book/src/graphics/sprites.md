# Sprites (Objects)

The GBA calls sprites "objects" (OBJ). Up to 128 sprites can be displayed simultaneously, each with independent position, size, palette, flipping, and priority. The hardware composites sprites automatically -- no CPU time required during rendering.

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

Sprite tiles live in the lower portion of VRAM (starting at `0x06010000` in tile modes). Like background tiles, they can be 4bpp (16 colors) or 8bpp (256 colors) and use the object palette (`mem_pal_obj`).

## 1D vs 2D mapping

The `.obj_mapping_1d` field in `reg_dispcnt` controls how multi-tile sprites index their tile data:

- **1D mapping:** tiles are laid out sequentially in memory. A 16x16 sprite (4 tiles) uses tiles N, N+1, N+2, N+3.
- **2D mapping:** tiles are laid out in a 32-tile-wide grid. A 16x16 sprite uses tiles at grid positions.

Most games use 1D mapping -- it is simpler and wastes less VRAM:

```cpp
gba::reg_dispcnt = {
    .video_mode = 0,
    .obj_mapping_1d = true,
    .enable_bg0 = true,
    .enable_obj = true,
};
```

## Hiding a sprite

Set the object mode to "hidden" to remove a sprite from the display without deleting its data:

```cpp
gba::obj_mem[0] = { .obj_mode = 2 }; // 2 = hidden
```

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::obj_mem[0] = { .y = 80, .x = 120, ... };` | `obj_set_attr(&oam_mem[0], ...)` |
| `gba::mem_pal_obj[n] = color;` | `pal_obj_mem[n] = color;` |
