# Colors & Palettes

The GBA uses 15-bit colors: 5 bits each for red, green, and blue, packed into a `short` (the top bit is unused).

## Color format

```text
Bit:  15  14-10  9-5  4-0
       X  Blue  Green  Red
```

```cpp
#include <gba/video>

// Write colors to background palette
gba::mem_pal_bg[0] = 0x0000;  // Black (background color)
gba::mem_pal_bg[1] = 0x001F;  // Red   (5 bits max = 31)
gba::mem_pal_bg[2] = 0x03E0;  // Green
gba::mem_pal_bg[3] = 0x7C00;  // Blue
gba::mem_pal_bg[4] = 0x7FFF;  // White
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
