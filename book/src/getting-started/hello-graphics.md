# Hello Graphics and Keypad

Now that you have a stable VBlank loop, the next step is drawing a visible shape and moving it.

This page pairs two tiny demos that share the same consteval circle sprite:

- `demo_hello_graphics.cpp`: draw the sprite in the centre.
- `demo_hello_keypad.cpp`: move the same sprite with the D-pad.

## Part 1: draw a shape

```cpp
{{#include ../../demos/demo_hello_graphics.cpp:7:}}
```

## What is happening?

1. The setup is the same as Hello VBlank: initialise interrupts and wait on `gba::VBlankIntrWait()` in the main loop.
2. `sprite_16x16(circle(...))` creates the sprite tile data at compile time (consteval).
3. We copy that tile data into OBJ VRAM, then place it with `obj_mem[0]`.
4. The display runs in Mode 0 with objects enabled (`.enable_obj = true`).
5. Colours use `_clr` literals for readability (`"#102040"_clr`, `"white"_clr`).

![Hello Graphics screenshot](../img/hello_graphics.png)

## Part 2: move it with keypad

```cpp
{{#include ../../demos/demo_hello_keypad.cpp:7:}}
```

- `keys.xaxis()` handles left/right.
- `keys.i_yaxis()` handles up/down in screen-space coordinates.
- Position is clamped to keep the sprite inside the 240x160 screen.

## Next step

Continue to [Hello Audio](./hello-audio.md) to trigger a PSG jingle on button press.
