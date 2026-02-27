# Tiles & Maps

Tile modes (0-2) are the backbone of GBA graphics. The display hardware composites 8x8 pixel tiles from VRAM, using a *tilemap* to arrange them into backgrounds. This is extremely memory-efficient and the scrolling is handled entirely by hardware.

## How it works

1. **Tile data** (the pixel art) is stored in VRAM "character base blocks"
2. **Tilemap** (which tile goes where) is stored in VRAM "screen base blocks"
3. **Palette** maps pixel indices to colors
4. The hardware reads the map, looks up each tile, applies the palette, and draws the scanline -- all in zero CPU cycles

## Loading tile data

Tile graphics are usually pre-converted at build time and copied into VRAM. Each 8x8 tile in 4bpp mode is 32 bytes (4 bits per pixel, 64 pixels):

```cpp
#include <gba/peripherals>
#include <gba/dma>

// Assuming tile_data is a const array in ROM
extern const unsigned short tile_data[];
extern const unsigned int tile_data_size;

// Copy tile data to character base block 0 (0x06000000)
gba::reg_dma[3] = gba::dma::copy(
    tile_data,
    reinterpret_cast<volatile void*>(0x06000000),
    tile_data_size / 4
);
```

## Setting up a background

```cpp
// Configure BG0: 256x256, 4bpp tiles
// Character base = 0 (tile data at 0x06000000)
// Screen base = 31 (map at 0x0600F800)
gba::reg_bgcnt[0] = {
    .char_base = 0,
    .screen_base = 31,
    .size = 0,  // 256x256 (32x32 tiles)
};

// Scroll BG0
gba::reg_bg_hofs[0] = 0;
gba::reg_bg_vofs[0] = 0;
```

## Background sizes

| Size value | Dimensions (pixels) | Dimensions (tiles) |
|------------|---------------------|---------------------|
| 0 | 256x256 | 32x32 |
| 1 | 512x256 | 64x32 |
| 2 | 256x512 | 32x64 |
| 3 | 512x512 | 64x64 |

## Scrolling

Scrolling is a single register write per axis:

```cpp
gba::reg_bg_hofs[0] = scroll_x;
gba::reg_bg_vofs[0] = scroll_y;
```

The hardware wraps seamlessly at the background boundaries. A 256x256 background scrolled past x=255 wraps back to x=0 -- perfect for side-scrolling games.
