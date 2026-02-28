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

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::angle` | `u32` (raw integer) |
| `gba::packed_angle<16>` | `u16` (raw integer) |
| `90_deg` | `0x4000` (magic constant) |
| `gba::ArcTan2(x, y)` | `ArcTan2(x, y)` |

stdgba wraps raw integers in type-safe wrappers. Overflow arithmetic is identical.
