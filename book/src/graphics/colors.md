# Colours & Palettes

The GBA uses 16-bit colours: 5 bits each for red, green, and blue in bits 0-14.

`"..."_clr` lives in `gba::literals` and accepts both hex (`"#RRGGBB"`) and CSS web colour names (for example `"cornflowerblue"`).

Named-colour list: [MDN CSS named colors](https://developer.mozilla.org/en-US/docs/Web/CSS/named-color).

## Colour format

```text
Bit:  15      14-10  9-5    4-0
      grn_lo  Blue   Green  Red
```

Most software treats bit 15 as unused and works with 15-bit colour (5-5-5). This is perfectly fine for general use.

```cpp
#include <gba/video>

// Write colours to background palette
gba::pal_bg_mem[0] = { .red = 0 };                  // Black (background colour)
gba::pal_bg_mem[1] = { .red = 31 };                  // Red   (5 bits max = 31)
gba::pal_bg_mem[2] = { .green = 31 };                // Green (5-bit, range 0-31)
gba::pal_bg_mem[3] = { .blue = 31 };                 // Blue
gba::pal_bg_mem[4] = { .red = 31, .green = 31, .blue = 31 }; // White

// Hex colour literals (grn_lo is derived from the green channel)
using namespace gba::literals;
gba::pal_bg_mem[5] = "#FF8040"_clr;
gba::pal_bg_mem[6] = "cornflowerblue"_clr;
```

Here are several colours displayed as palette swatches using Mode 0 tiles:

![Colour swatches](../img/colors.png)

```cpp
{{#include ../../demos/demo_colors.cpp:7:}}
```

## Palette memory layout

The GBA has 512 palette entries total (1 KB), split evenly:

| Region | Address | Entries | Used by |
|--------|---------|---------|---------|
| `mem_pal_bg` | `0x05000000` | 256 | Background tiles |
| `mem_pal_obj` | `0x05000200` | 256 | Sprites (objects) |

In 4bpp (16-colour) mode, the 256 entries are organised as 16 sub-palettes of 16 colours each. Each tile chooses which sub-palette to use.

In 8bpp (256-colour) mode, all 256 entries form one large palette.

## Palette index 0

Palette index 0 is special: it is the **transparent colour** for both backgrounds and sprites. For the very first background palette (sub-palette 0, index 0), it also serves as the screen backdrop colour - the colour you see when no background or sprite covers a pixel.

```cpp
// Set the backdrop to dark blue
gba::mem_pal_bg[0] = 0x4000;
```

## Bit 15 and hardware blending

Bit 15 (`grn_lo`) is usually safe to ignore for everyday palette work.

When colour effects are enabled (brighten, darken, or alpha blend), hardware treats green as an internal 6-bit value and may use `grn_lo`. This can create hardware-visible differences that many emulators do not reproduce.

For full details, demo code, and emulator-vs-hardware screenshots, see [Advanced: Green Low Bit (`grn_lo`)](../advanced/green-low-bit.md).
