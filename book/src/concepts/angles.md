# Angles

stdgba provides type-safe angle types optimised for GBA hardware. Angles use binary representation where the full range of an integer maps to one full revolution (360 degrees).

## Angle types

### `angle` - intermediate type

The `angle` type is a 32-bit unsigned integer where the full `0` to `2^32` range represents 0 to 360 degrees. Natural integer overflow handles wraparound:

```cpp
#include <gba/angle>
using namespace gba::literals;

gba::angle heading = 90_deg;
heading += 45_deg;    // 135 degrees
heading = heading * 2; // 270 degrees
heading += 180_deg;   // 90 degrees (wraps around)
```

### `packed_angle<Bits>` - storage type

For memory-efficient storage, use `packed_angle` with a specific bit width. These convert implicitly to `angle` for arithmetic:

```cpp
gba::packed_angle<16> stored_heading;  // 2 bytes
gba::packed_angle<8> coarse_dir;       // 1 byte

// Promote to angle for arithmetic
gba::angle heading = stored_heading;
heading += 45_deg;

// Store back (truncates to precision)
stored_heading = heading;
```

Common aliases:
- `packed_angle8` - 8-bit (256 steps, ~1.4 degree resolution)
- `packed_angle16` - 16-bit (65536 steps, ~0.005 degree resolution)

## Literals

The `gba::literals` namespace provides degree and radian literals:

```cpp
using namespace gba::literals;

gba::angle a = 90_deg;
gba::angle b = 1.5708_rad;  // ~90 degrees
```

## BIOS integration

The GBA BIOS angle functions use 16-bit angles where `0x10000 = 360` degrees. Use `packed_angle16` for BIOS results:

```cpp
gba::packed_angle16 dir = gba::ArcTan2(dx, dy);

// Or keep full precision for further arithmetic
gba::angle precise_dir = gba::ArcTan2(dx, dy);
```

## `bit_cast` - raw access

`gba::bit_cast` extracts the underlying integer from an angle without any computation. The full `0..2^32` range represents one complete revolution.

```cpp
using namespace gba::literals;

gba::angle a = 90_deg;
unsigned int raw = gba::bit_cast(a);  // 0x40000000

gba::packed_angle16 pa = 90_deg;
uint16_t raw16 = gba::bit_cast(pa);  // 0x4000
```

This is useful when interacting with hardware registers or lookup tables that expect raw integer angles.

## Utility functions

### `lut_index<TableBits>` - lookup table index

Converts an angle to an index into a power-of-two-sized lookup table. The full `0..360` degree range maps uniformly onto `[0, 2^TableBits)` with no gaps.

```cpp
using namespace gba::literals;

// 256-entry sine table (8-bit indexing)
gba::angle theta = 45_deg;
auto idx = gba::lut_index<8>(theta);  // 0..255

// 512-entry table (9-bit indexing)
auto idx9 = gba::lut_index<9>(theta);  // 0..511
```

### `as_signed` - signed range view

Reinterprets the angle as a signed integer, treating the range as `[-180, +180)` degrees rather than `[0, 360)`. Useful for comparisons and threshold tests.

```cpp
using namespace gba::literals;

gba::angle facing_left = 270_deg;
int s = gba::as_signed(facing_left);  // negative (left of centre)

gba::angle facing_right = 90_deg;
int sr = gba::as_signed(facing_right); // positive (right of centre)
```

### `ccw_distance` and `cw_distance` - arc distances

Measure the angular distance between two angles travelling in a specific direction. Both return unsigned values that handle wraparound correctly.

```cpp
using namespace gba::literals;

// How far is it from 90 to 270 going counter-clockwise?
auto ccw = gba::ccw_distance(90_deg, 270_deg);  // 180 degrees

// How far is it from 270 to 90 going clockwise?
auto cw = gba::cw_distance(270_deg, 90_deg);    // 180 degrees

// Going the short way vs the long way
auto short_way = gba::ccw_distance(0_deg, 90_deg);   // 90 degrees
auto long_way  = gba::cw_distance(0_deg, 90_deg);    // 270 degrees
```

### `is_ccw_between` - arc containment test

Tests whether an angle lies within a counter-clockwise arc from `start` to `end`. Handles wraparound automatically.

```cpp
using namespace gba::literals;

// Is 90 degrees within the CCW arc from 0 to 180?
bool yes = gba::is_ccw_between(0_deg, 180_deg, 90_deg);   // true
bool no  = gba::is_ccw_between(0_deg, 180_deg, 270_deg);  // false

// Wraparound arc: from 315 to 45 degrees (passing through 0)
bool in_arc = gba::is_ccw_between(315_deg, 45_deg, 0_deg);  // true
```

## tonclib comparison

| stdgba                  | tonclib                   |
|-------------------------|---------------------------|
| `gba::angle`            | `u32` (raw integer)       |
| `gba::packed_angle<16>` | `u16` (raw integer)       |
| `90_deg`                | `0x4000` (magic constant) |
| `gba::ArcTan2(x, y)`    | `ArcTan2(x, y)`           |

stdgba wraps raw integers in type-safe wrappers. Overflow arithmetic is identical.
