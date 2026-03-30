# stdgba

**stdgba** is a C++23 library for Game Boy Advance development.

It keeps the hardware-first model of classic GBA development, but exposes it through strongly-typed, constexpr-friendly APIs instead of macro-heavy C interfaces.

## What stdgba is

- A zero-heap-friendly library for real GBA hardware constraints.
- A typed register/peripheral API built around `inline constexpr` objects.
- A consteval-first toolkit for things that benefit from compile-time validation.
- A practical replacement for low-level C-era patterns when writing modern C++.

## stdgba is not a game engine

You still decide your main loop, memory layout, rendering strategy, and frame budget. stdgba focuses on safer and more expressive building blocks.

## Core design goals

1. **Zero-cost abstractions** - generated code should match hand-written low-level intent.
2. **Compile-time validation** - invalid asset/pattern/config inputs should fail at compile time when possible.
3. **Typed hardware access** - peripheral use should be explicit, discoverable, and hard to misuse.
4. **Practical migration path** - where meaningful, docs map familiar tonclib-era workflows to stdgba equivalents.

## What you get

- `registral<T>` register wrappers with designated initialisers
- fixed-point and angle types with literal support
- BIOS wrappers for sync, math, memory, compression, affine setup
- compile-time image embedding and conversion (`gba/embed`)
- pattern-based PSG music composition (`gba/music`)
- static ECS (`gba/ecs`) with fixed capacity and deterministic iteration

## Quick taste

```cpp
#include <gba/peripherals>
#include <gba/keyinput>
#include <gba/bios>

int main() {
    // Initialise interrupt handler
    gba::irq_handler = {};

    // Set video mode 0, enable BG0
    gba::reg_dispcnt = { .video_mode = 0, .enable_bg0 = true };

    // Enable VBlank interrupt
    gba::reg_dispstat = { .enable_irq_vblank = true };
    gba::reg_ie = { .vblank = true };
    gba::reg_ime = true;

    gba::keypad keys;
    for (;;) {
        keys = gba::reg_keyinput;
        if (keys.pressed(gba::key_a)) {
            // ...
        }
        gba::VBlankIntrWait();
    }
}
```

## Book roadmap

- Start with [Hello VBlank](./getting-started/hello-vblank.md).
- Draw and move your first sprite in [Hello Graphics and Keypad](./getting-started/hello-graphics.md).
- Add button-triggered sound in [Hello Audio](./getting-started/hello-audio.md).
- Learn register and frame-loop basics in [Core Concepts](./concepts/registers.md).
- Get pixels on screen via [Graphics](./graphics/video-modes.md).
- Reach for transfer, BIOS, and support APIs in [Utilities](./utilities/dma.md).
- Explore [Audio](./audio/index.md), [ECS](./ecs/index.md), and [Additional Types](./concepts/fixed-point.md).

## Who this is for

- GBA developers who want modern C++ without losing hardware control
- C++ programmers learning GBA development
- Existing tonclib/libtonc users migrating to typed APIs
