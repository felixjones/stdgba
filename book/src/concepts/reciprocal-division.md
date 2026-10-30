# Reciprocal Division

The GBA ARM7TDMI has no divide instruction. Fixed-point division is implemented
in software, so repeated divisions can be expensive in camera transforms,
sprite scaling and per-scanline effects.

The `<gba/reciprocal>` header provides division strategies that multiply by an
approximation of `1 / denominator`. Each strategy makes a different trade-off
between precision, execution time and ROM usage. Ordinary `gba::fixed`
division is unchanged.

## Basic usage

```cpp
#include <gba/fixed_point>
#include <gba/reciprocal>

using q16 = gba::fixed<int, 16>;

constexpr auto divide = gba::reciprocal::normalized<8, 2>;

q16 x = 12;
q16 y = 8;
q16 z = 3;

auto quotient = divide(x, z);

// Prepare one reciprocal for multiple numerators
auto depth = divide(z);
auto px = depth(x);
auto py = depth(y);
```

Calling a strategy with one denominator returns a *divider*. The divider stores
the prepared reciprocal without keeping a reference to the original value.
Reusing it avoids repeating the denominator setup.

Strategies also work at compile time, with the same results as runtime calls:

```cpp
using namespace gba::reciprocal;

static_assert(corrected<normalized<8, 2>>(q16{1}, q16{2}) == q16{0.5});
static_assert(normalized<8, 2>(q16{1}, q16{2}) != q16{0.5}); // One raw bit low
```

## Division strategies

| Strategy | ROM table | Denominator | Typical use |
|----------|-----------|-------------|-------------|
| `normalized<8, 1>` | 512 bytes | Nonzero `fixed` | Projection and scaling |
| `normalized<8, 2>` | 512 bytes | Nonzero `fixed` | Higher-precision approximation |
| `affine<3>` | None | Nonzero `fixed` | Division without a lookup table |
| `direct<Min, Max>` | 4 bytes per entry | Integer magnitude in `[Min, Max]` | Scanline scales and bounded divisors |
| `direct<Min, Max, entry_format::packed_u16>` | 2 bytes per entry | Integer magnitude in `[Min, Max]` | Smaller tables with lower precision |
| `corrected<Strategy>` | Same as wrapped strategy | Same as wrapped strategy | Exact truncating division |

### Normalised division

`normalized<IndexBits, Refinements>` shifts the denominator into `[1, 2)`,
looks up a 16-bit reciprocal seed, then applies Newton-Raphson refinement.
Each refinement roughly squares the relative error:

| Configuration | Seed table | Relative error bound |
|---------------|-----------:|---------------------:|
| `normalized<6, 1>` | 128 bytes | ~6.1e-5 |
| `normalized<8, 1>` | 512 bytes | ~3.9e-6 |
| `normalized<8, 2>` | 512 bytes | ~7.5e-9 |
| `normalized<10, 1>` | 2048 bytes | ~2.6e-7 |

`IndexBits` accepts 1-10 and `Refinements` accepts 0-3. The bound is available
as `relative_error_bound`; strategies with the same `IndexBits` share a seed
table.

For perspective projection, prepare one divider per vertex and use it for
both screen coordinates:

```cpp
struct vec3 { q16 x, y, z; };
struct screen_point { q16 x, y; };

constexpr auto project = gba::reciprocal::normalized<8, 1>;
constexpr q16 focal = 160;

screen_point to_screen(const vec3& v) {
    auto depth = project(v.z);
    return {
        q16{120} + depth(v.x * focal),
        q16{80} - depth(v.y * focal)
    };
}
```

Clip vertices with zero or negative camera-space depth before projection.
The intermediate products and final coordinates must fit their fixed-point
types.

### Table-free division

`affine<Refinements>` uses the same normalisation and refinement, but starts
with a straight-line approximation instead of a table lookup:

```cpp
constexpr auto divide = gba::reciprocal::affine<3>;

q16 time_to_impact(q16 distance, q16 closing_speed) {
    return divide(distance, closing_speed);
}
```

| Configuration | Table | Relative error bound |
|---------------|------:|---------------------:|
| `affine<2>` | 0 bytes | ~1.2e-5 |
| `affine<3>` | 0 bytes | ~7.6e-9 |

The initial estimate is less accurate than a table seed. `affine<3>` provides
similar precision to `normalized<8, 2>`, with no table and slightly more
one-shot work.

### Integer denominators

`direct<Min, Max, Format>` builds a reciprocal table at compile time. The
numerator is fixed-point, the denominator is an integer, and the result keeps
the numerator's type:

```cpp
constexpr auto rows = gba::reciprocal::direct<1, 160>;

auto scale = rows(q16{120}, 20); // Approximately 6.0
```

The default format stores a 32-bit reciprocal for each denominator. Its
estimate can differ from the exact truncated quotient by one raw bit.
`entry_format::packed_u16` halves the table size, but increases the error:

| Configuration | Table |
|---------------|------:|
| `direct<1, 160>` | 640 bytes |
| `direct<1, 160, entry_format::packed_u16>` | 320 bytes |
| `direct<1, 1024>` | 4096 bytes |

Table size is `(Max - Min + 1)` times the entry size. Keep the range close to
the denominators used by the application. Negative denominators are supported
when the resulting quotient is representable.

### Exact results

`corrected<Strategy>` adjusts an approximate result to the exact quotient
truncated towards zero, matching integer `/` rounding:

```cpp
constexpr auto exact = gba::reciprocal::corrected<
    gba::reciprocal::normalized<8, 2>>;

auto quotient = exact(q16{12}, q16{3}); // Exactly 4.0
```

Correction requires a sufficiently accurate estimate:

| Wrapped strategy | Supported configuration |
|------------------|-------------------------|
| `normalized<IndexBits, Refinements>` | `IndexBits >= 8`, `Refinements >= 2` |
| `affine<Refinements>` | `Refinements >= 3` |
| `direct<Min, Max>` | `entry_format::u32` |

Other combinations fail to compile. Correction reuses the wrapped strategy's
table and checks the quotient without overflowing a narrow division
intermediate. It adds execution time, so use it when exact rounding matters.

## Batch division

Batch calls process contiguous ranges in one ARM/IWRAM kernel call:

```cpp
std::array<q16, 64> numerators{}, denominators{}, results{};
denominators.fill(q16{4});

constexpr auto divide = gba::reciprocal::normalized<8, 2>;
divide(numerators, denominators, results);

// A prepared divider also accepts ranges
auto shared = divide(q16{4});
shared(numerators, results);
```

The results match the corresponding scalar calls. Batching avoids a
Thumb-to-ARM call for every element.

All ranges must be contiguous, sized and equal in length. Input and output
fixed-point types must match. For `direct`, a batch denominator range uses
32-bit integers such as `int` or `unsigned int`.

## Preconditions

- Denominators must be nonzero.
- A `direct` denominator's magnitude must be within `[Min, Max]`.
- Quotients must fit the result type. Unsigned results cannot represent
  negative nonzero quotients.
- `normalized` and `affine` require matching numerator and denominator types
  with 32-bit storage, such as `gba::fixed<int, 16>`.

Invalid inputs assert in debug builds, trap in release builds and fail
constant evaluation. Results are not silently clamped.

## Performance and memory

Runtime arithmetic uses ARM code in IWRAM, including specialised normalisation,
refinement and scalar multiply kernels. Constant evaluation uses matching C++
arithmetic. Tables remain in ROM and are shared by strategies with the same
configuration, including corrected forms.

`table_bytes` reports the table size. Copying a strategy does not copy its
table. No strategy changes the CPU mode or `WAITCNT`.

The following measurements are cycles for 128 quotients in mGBA, built with
devkitARM GCC 15.1.0 in Release mode. The caller runs as Thumb code in ROM,
with `WAITCNT=0x4317`, interrupts disabled and forced blank. Reused calls share
each denominator across four numerators. The widened fixed-point division
baseline takes 36080 cycles.

| Strategy | One-shot | Reused | Batch | Shared batch (4) | Max raw error |
|----------|---------:|-------:|------:|-----------------:|--------------:|
| `normalized<8, 1>` | 27463 | 18987 | 19800 | 14697 | 39 |
| `normalized<8, 2>` | 30023 | 19723 | 22360 | 15433 | 1 |
| `corrected<normalized<8, 2>>` | 46471 | 29309 | 27382 | 26390 | 0 |
| `affine<3>` | 31420 | 19718 | 22994 | 15712 | 1 |
| `corrected<direct<1, 79>>` | 18622 | 16123 | 9072 | 11990 | 0 |
| `direct<1, 79, entry_format::packed_u16>` | 18101 | 15552 | 7708 | 10551 | 101 |

These are sample measurements from `benchmarks/bench_reciprocal_api.cpp`, not
hardware timing guarantees. Maximum raw error is the largest difference from
the exact truncated quotient in that sample set, not a general error bound.

Sharing denominators and batching reduce overhead. Bounded integer
denominators suit `direct`; one-shot corrected normalised division can be
slower than ordinary division. Measure the call form and input range used by
the application.

GCC optimises the kernels and header wrappers even in `-O0` builds. Calling
loops still follow the application's optimisation settings.

## Demo: Mode 7 Floor

This demo renders a repeating 512x512 PNG as an affine BG2 floor. The CPU
calculates one transform per scanline, and HBlank DMA transfers the transforms
to the background registers:

```cpp
{{#include ../../demos/demo_reciprocal_floor.cpp:4:}}
```

![Reciprocal floor demo screenshot](../img/reciprocal_floor.png)

| Button | Action |
|--------|--------|
| Up / Down | Drive forwards / backwards |
| Left / Right | Turn |
| L / R | Strafe |
| A / B | Raise / lower the camera |

### How it works

The horizon is eight rows above the screen, giving integer denominators
`y - horizon` in `[8, 167]`. `corrected<direct<8, 167>>` uses a 640-byte table
and prepares each row's denominator once for two Q8 numerators:

```cpp
const auto row = row_divide(y - horizon);
const auto dx = row(height_cosine);
const auto dz = row(height_sine);
```

The numerators are camera height multiplied by the heading's cosine and sine.
The resulting `dx` and `dz` become BG2's horizontal texture steps, `PA` and
`PC`. The reference point centres the view and offsets it by the camera
position. `PB` and `PD` are zero because each scanline supplies its own
reference point.

`gba::embed::affine8` converts the PNG to tiles, palette and a linear affine
map at compile time. These are copied directly to VRAM. The map and camera
position wrap together.

Two scanline buffers separate calculation from display. During VBlank, the
demo installs row 0 and arms DMA from row 1. HBlank after row `y` installs the
transform for row `y + 1`; a guard entry handles the final transfer.

## Further reading

- [Fixed-Point Math](./fixed-point.md) - storage, precision and arithmetic
- [`gba::reciprocal` reference](../reference/reciprocal.md) - types and constraints
- [Tonc: Mode 7](https://gbadev.net/tonc/mode7.html) - per-scanline affine projection
