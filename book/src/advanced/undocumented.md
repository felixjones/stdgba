# Undocumented Namespace

stdgba exposes a small set of BIOS calls and hardware registers through `gba::undocumented`. These are real features of the hardware, but they sit outside the better-traveled part of the public GBA programming model.

Use them when you know exactly why you need them. For everyday game code, prefer the documented BIOS wrappers and peripheral registers first.

## What lives in `gba::undocumented`

Three public headers contribute to the namespace:

- `<gba/peripherals>` for undocumented memory-mapped registers
- `<gba/video>` for undocumented OAM values (`mode_stereo`)
- `<gba/bios>` for undocumented BIOS SWIs

## Why these APIs are separate

The namespace is a warning label as much as an API grouping:

- behaviour is less commonly documented in community references
- emulator support can be uneven
- some features are useful mostly for diagnostics, boot-state inspection, or hardware experiments
- some settings can break assumptions if changed casually

## BIOS: `GetBiosChecksum()`

`<gba/bios>` exposes one undocumented BIOS helper:

```cpp
#include <gba/bios>

auto checksum = gba::undocumented::GetBiosChecksum();
if (checksum == 0xBAAE187F) {
	// Official GBA BIOS checksum
}
```

This is mainly useful for:

- sanity-checking the BIOS on real hardware
- emulator/debug diagnostics
- research tools that want to distinguish known BIOS images

## Undocumented registers

`<gba/peripherals>` exposes these registers:

| Address     | API                   | Type                           | Typical use                                                        |
|-------------|-----------------------|--------------------------------|--------------------------------------------------------------------|
| `0x4000002` | `reg_stereo_3d`       | `bool`                         | Stereo 3D enable (historically `GREENSWAP`)                        |
| `0x4000008` | `reg_bgcnt_stereo[4]` | `background_control_stereo[4]` | Per-background stereo eye selection                                |
| `0x4000300` | `reg_postflg`         | `bool`                         | Check whether the system has already passed the BIOS boot sequence |
| `0x4000301` | `reg_haltcnt`         | `halt_control`                 | Low-power mode control                                             |
| `0x4000410` | `reg_obj_center`      | `volatile char`                | Rare OBJ-centre hardware experiment register                       |
| `0x4000800` | `reg_memcnt`          | `memory_control`               | BIOS/EWRAM control                                                 |

The [Undocumented Registers](../reference/peripherals/undocumented.md) reference page lists the raw addresses. This page focuses on when they are practically useful.

## `reg_stereo_3d`

```cpp
#include <gba/peripherals>
#include <gba/video>

gba::undocumented::reg_stereo_3d = true;

// BG0 only in the left eye, BG1 only in the right eye
gba::undocumented::reg_bgcnt_stereo[0] = { .stereo = gba::undocumented::stereo_left, .screenblock = 30 };
gba::undocumented::reg_bgcnt_stereo[1] = { .stereo = gba::undocumented::stereo_right, .screenblock = 31 };

// Stereo objects: even OAM index = left eye, odd OAM index = right eye
gba::obj_mem[0] = { .y = 80, .mode = gba::undocumented::mode_stereo, .x = 120 };
gba::obj_mem[1] = { .y = 80, .mode = gba::undocumented::mode_stereo, .x = 120 };
```

This is bit 16 of `DISPCNT`, historically known as `GREENSWAP`. It enables a stereoscopic display mode in which output
pixels alternate between the left and right eye across each scanline.

Each layer then chooses which eye it draws to:

- backgrounds use `reg_bgcnt_stereo`, an alias of `reg_bgcnt` that exposes bits 4-5 as a stereo mode
- objects use OBJ mode 3, `mode_stereo`, where even OAM indices draw to the left eye and odd indices to the right eye

| `stereo`                | Effect                                    |
|-------------------------|-------------------------------------------|
| `stereo_both`           | Both eyes visible (rendered as-is)        |
| `stereo_right`          | Right eye only                            |
| `stereo_left`           | Left eye only                             |
| `stereo_duplicate_left` | Duplicate the left eye into the right eye |

Because the eyes are interleaved after scrolling and object placement, move backgrounds and objects in steps of 2
pixels. Odd offsets put graphics into the wrong eye.

It is best treated as a research feature, not a usable peripheral. Emulator support is required.

## `reg_postflg`

```cpp
#include <gba/peripherals>

bool booted_via_bios = gba::undocumented::reg_postflg;
```

`POSTFLG` is useful when you need to know whether the machine has already passed the BIOS startup path. That mostly comes up in:

- diagnostics
- boot-time experiments
- research around soft reset or alternate loaders

Most games never need to read it.

## `reg_haltcnt`

```cpp
#include <gba/peripherals>

gba::undocumented::reg_haltcnt = { .low_power_mode = true };
```

This directly controls low-power behaviour. In normal code, prefer the documented BIOS wrappers from `<gba/bios>`:

- `gba::Halt()` to sleep until interrupt
- `gba::Stop()` to enter deeper low-power mode

Those helpers are clearer and easier to read in application code. `reg_haltcnt` is most useful when you want exact register-level control.

## `reg_obj_center`

```cpp
#include <gba/peripherals>

gba::undocumented::reg_obj_center = 0;
```

It is unknown what this register does, but no emulator supports it. Needs additional experimentation on real hardware to determine its behaviour, if any.

## `reg_memcnt`

```cpp
#include <gba/peripherals>

gba::undocumented::reg_memcnt = {
	.ewram = true,
	.ws_ewram = 0xd,
};
```

`MEMCNT` is the most practically interesting entry in the namespace. It controls:

- BIOS swap state
- whether the CGB BIOS is disabled
- whether EWRAM is enabled
- EWRAM wait-state configuration

This makes it relevant for:

- hardware experiments
- boot/loader code
- benchmarking memory timing changes

It is also one of the easiest ways to make the system unstable if you write nonsense values, so treat it carefully.

## Testing expectations

Because these APIs are outside the mainline path:

- test on real hardware when possible
- expect emulator differences
- isolate undocumented writes behind small helper functions so the rest of the codebase stays understandable

That is the main reason stdgba keeps them behind an explicit namespace instead of mixing them into the everyday API surface.
