# Colors & Palettes

The GBA uses 16-bit colors: 5 bits each for red and blue, and 6 bits for green. The green channel is split across the halfword -- 5 bits in the standard position and the 6th (lowest) bit at position 15.

## Color format

```text
Bit:  15    14-10  9-5       4-0
      G_lo  Blue   Green(hi) Red
```

Most software treats bit 15 as unused and works with 15-bit color (5-5-5). This is fine for general use. The extra green bit only matters when you need the full hardware precision.

```cpp
#include <gba/video>

// Write colors to background palette
gba::pal_bg_mem[0] = { .red = 0 };                  // Black (background color)
gba::pal_bg_mem[1] = { .red = 31 };                  // Red   (5 bits max = 31)
gba::pal_bg_mem[2] = { .green = 31 };                // Green (5-bit, range 0-31)
gba::pal_bg_mem[3] = { .blue = 31 };                 // Blue
gba::pal_bg_mem[4] = { .red = 31, .green = 31, .blue = 31 }; // White

// Use the 6th green bit for extra precision
gba::pal_bg_mem[5] = { .green = 31, .green_lo = 1 }; // Brightest green (6-bit = 63)

// Hex color literals ignore the extra green bit (standard 5-5-5)
using namespace gba;
gba::pal_bg_mem[6] = "#FF8040"_clr;
```

## Palette memory layout

The GBA has 512 palette entries total (1 KB), split evenly:

| Region | Address | Entries | Used by |
|--------|---------|---------|---------|
| `mem_pal_bg` | `0x05000000` | 256 | Background tiles |
| `mem_pal_obj` | `0x05000200` | 256 | Sprites (objects) |

In 4bpp (16-color) mode, the 256 entries are organized as 16 sub-palettes of 16 colors each. Each tile chooses which sub-palette to use.

In 8bpp (256-color) mode, all 256 entries form one large palette.

## Palette index 0

Palette index 0 is special: it is the **transparent color** for both backgrounds and sprites. For the very first background palette (sub-palette 0, index 0), it also serves as the screen backdrop color -- the color you see when no background or sprite covers a pixel.

```cpp
// Set the backdrop to dark blue
gba::mem_pal_bg[0] = 0x4000;
```
