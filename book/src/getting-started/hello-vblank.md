# Hello VBlank

The simplest GBA program that actually *does something* is a VBlank loop. This is the heartbeat of every GBA game - wait for the display to finish drawing, then update your game state.

## The code

```cpp
#include <gba/interrupt>
#include <gba/peripherals>

int main() {
    // Step 1: Initialise the interrupt handler
    gba::irq_handler = {};

    // Step 2: Tell the display hardware to fire an interrupt each VBlank
    gba::reg_dispstat = { .enable_irq_vblank = true };

    // Step 3: Tell the CPU to accept VBlank interrupts
    gba::reg_ie = { .vblank = true };
    gba::reg_ime = true;

    // Step 4: Main loop
    for (;;) {
        gba::VBlankIntrWait();
        // Your game logic goes here
    }
}
```

## What is happening?

The GBA display draws 160 lines of pixels (the "active" period), then enters a 68-line "vertical blank" period where no pixels are drawn. The VBlank is your window to safely update video memory without visual tearing.

`gba::VBlankIntrWait()` puts the CPU to sleep (saving battery) until the VBlank interrupt fires. This is the BIOS SWI `0x05`.

### Step by step

1. **`gba::irq_handler = {}`** installs the default interrupt dispatcher. Without this, BIOS interrupt-wait functions will hang forever.

2. **`gba::reg_dispstat = { .enable_irq_vblank = true }`** writes to the DISPSTAT register using a designated initialiser. Only the `.enable_irq_vblank` bit is set; all other fields default to zero.

3. **`gba::reg_ie = { .vblank = true }`** enables the VBlank interrupt in the interrupt enable register. **`gba::reg_ime = true`** is the master interrupt switch.

4. **`gba::VBlankIntrWait()`** is a BIOS call that halts the CPU until a VBlank interrupt occurs.

## tonclib comparison

The equivalent tonclib code:

```c
#include <tonc.h>

int main() {
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    for (;;) {
        VBlankIntrWait();
    }
}
```

The key difference is that stdgba uses designated initialisers (`{ .vblank = true }`) instead of bitfield macros (`II_VBLANK`). Typos in field names are compile errors; typos in macro names might silently compile to wrong values.

## Putting something on screen

The VBlank loop itself produces a blank screen. To prove the program is running, here is a minimal extension that draws a white rectangle in Mode 3:

```cpp
{{#include ../../demos/demo_hello_vblank.cpp:7:}}
```

![Hello VBlank screenshot](../img/hello_vblank.png)

## Next steps

- Continue to [Hello Graphics and Keypad](./hello-graphics.md) to draw and move a consteval sprite.
- Then continue to [Hello Audio](./hello-audio.md) to play a PSG jingle on button press.
