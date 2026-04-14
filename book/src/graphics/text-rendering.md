# Text Rendering

stdgba provides a text renderer targeting 4bpp BG tile modes.

The core goal is to render formatted strings efficiently (including typewriter effects) without a full-screen redraw each frame.

## Features

- Bitmap fonts embedded from BDF files at compile time via `<gba/embed>`.
- Character streams:
  - C-string streams.
  - Format generator streams from `<gba/format>`.
- Word wrapping using a lookahead to the next break character.
- Incremental rendering with `draw_cursor` for typewriter effects.
- Optional drop shadow.

## Quick start

The demo below embeds `9x18.bdf`, configures the bitplane palette, and draws one visible glyph per frame.

```cpp
{{#include ../../demos/demo_text_render.cpp:4:}}
```

![Text rendering demo](../img/text_render.png)

## Notes

- Word wrapping only occurs at word starts (after a break character). Long tokens are allowed to overflow rather than wrapping one character per line.
- The bitplane renderer uses mixed-radix encoding so multiple planes can share a 4bpp tile while selecting different palette banks.
