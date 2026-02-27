# Sync and Power

Use BIOS wait calls to pace the frame loop and reduce wasted CPU time.

## Frame pacing

```cpp
#include <gba/bios>

for (;;) {
	update_game();
	render();
	gba::VBlankIntrWait();
}
```

`VBlankIntrWait()` is the standard 60 Hz pacing primitive.

> Timing note: the GBA refresh is `2^24 / 280896` Hz (about 59.73 Hz, ~16.74 ms per frame), not exactly 60. VBlank starts after 160 visible scanlines and lasts 68 scanlines (228 total per frame), so `VBlankIntrWait()` naturally locks your loop to this hardware cadence.

## General interrupt wait

```cpp
#include <gba/bios>
#include <gba/peripherals>

// Wait for VBlank IRQ (interrupts must be configured first).
gba::IntrWait(true, {.vblank = true});
```

Use `IntrWait()` when you need a specific IRQ mask.

## Low-power halt

```cpp
#include <gba/bios>

gba::Halt();
```

`Halt()` sleeps until the next enabled interrupt fires.
