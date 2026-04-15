# Fixed-Point Math

The GBA ARM7TDMI has no floating-point unit. Floating-point is emulated in software,
so fixed-point arithmetic is the usual choice for gameplay math, camera transforms,
and register-facing values.

## The `fixed<>` type

```cpp
#include <gba/fixed_point>
using namespace gba::literals;

// 8.8 format (good for small ranges, fine sub-pixel steps)
gba::fixed<short> position = 3.5_fx;

// 16.16 format (high precision for world-space values)
gba::fixed<int> velocity = 0.125_fx;

// 24.8 format (common GBA-friendly choice, tonclib-style)
gba::fixed<int, 8> angle = 1.5_fx;
```

`fixed<Rep, FracBits, IntermediateRep>` stores a scaled integer in `Rep`.

- `Rep` controls storage width and sign.
- `FracBits` controls precision (`step = 1 / 2^FracBits`).
- `IntermediateRep` controls multiply/divide intermediate width.

## Precision and range

For a signed representation:

- precision step: `1 / (1 << FracBits)`
- minimum: `-2^(integer_bits)`
- maximum: `2^(integer_bits) - step`

where `integer_bits = numeric_limits<Rep>::digits - FracBits`
(`digits` excludes the sign bit).

For unsigned representations, minimum is `0`.

### Common formats

| Type | Format | Approx range | Precision step |
|------|--------|--------------|----------------|
| `fixed<short>` | 8.8 | `-128` to `127.99609375` | `1/256` |
| `fixed<int>` | 16.16 | `-32768` to `32767.9999847412` | `1/65536` |
| `fixed<int, 8>` | 24.8 | `-8388608` to `8388607.99609375` | `1/256` |
| `fixed<short, 4>` | 12.4 | `-2048` to `2047.9375` | `1/16` |

### Introspecting format traits

```cpp
using fx = gba::fixed<int, 8>;
using traits = gba::fixed_point_traits<fx>;

static_assert(traits::frac_bits == 8);
static_assert(std::is_same_v<traits::rep, int>);
```

## The `_fx` literal

The `_fx` suffix creates fixed-point literals at compile time:

```cpp
using namespace gba::literals;

gba::fixed<short> a = 3.14_fx;
gba::fixed<short> b = 2_fx;

auto c = a + b;
auto d = a * b;
```

`_fx` is format-agnostic until assignment, then converts to the destination
`fixed<>` type.

## Arithmetic and overflow behaviour

Standard operators are supported:

```cpp
gba::fixed<short> a = 10.5_fx;
gba::fixed<short> b = 3.25_fx;

auto sum = a + b;
auto diff = a - b;
auto prod = a * b;
auto quot = a / b;

auto neg = -a;
bool gt = a > b;
```

Multiplication and division use `IntermediateRep` internally.

- `fixed<short>` uses a 32-bit intermediate by default.
- `fixed<int>` defaults to `int` intermediate (faster on ARM, lower headroom).

If you need safer large products/quotients, use `precise<>`, which switches to a
64-bit intermediate:

```cpp
using fast = gba::fixed<int, 16>;
using safe = gba::precise<int, 16>;

fast a = 100.0_fx;
fast b = 400.0_fx;
auto fast_prod = a * b;   // may overflow in edge cases

safe x = 100.0_fx;
safe y = 400.0_fx;
auto safe_prod = x * y;   // wider intermediate
```

## Mixed-type arithmetic and promotion API

Operations require compatible types. For different `fixed<>` formats, use the
promotion wrappers in `<gba/fixed_point>` to make intent explicit.

### Why wrappers exist

```cpp
using fix8 = gba::fixed<int, 8>;
using fix4 = gba::fixed<int, 4>;

fix8 a = 3.5_fx;
fix4 b = 1.25_fx;

// auto bad = a + b; // incompatible formats
auto ok  = gba::as_lhs(a) + b;
```

### Promotion wrappers

| Wrapper | Result steering | Typical use |
|---------|-----------------|-------------|
| `as_lhs(x)` | convert other operand to wrapped type | keep left-hand format |
| `as_rhs(x)` | convert wrapped operand to other type | match right-hand format |
| `as_widening(x)` | keep higher fractional precision | avoid precision loss |
| `as_narrowing(x)` | match the narrower side | intentional truncation |
| `as_average_frac(x)` | average fractional bits | balanced precision |
| `as_average_int(x)` | average integer-range bits | balanced range |
| `as_next_container(x)` | promote storage to next wider container | headroom for mixed small types |
| `as_word_storage(x)` | use `int`/`unsigned int` storage | ARM-friendly word math |
| `as_signed(x)` | force signed storage type | sign-aware operations |
| `as_unsigned(x)` | force unsigned storage type | non-negative domains only |
| `with_rounding(wrapper)` | rounding meta-wrapper for conversions | explicit rounding policy path |

### Practical examples

```cpp
using fix8 = gba::fixed<int, 8>;
using fix4 = gba::fixed<int, 4>;

fix8 hi = 3.53125_fx;
fix4 lo = 1.25_fx;

auto keep_hi = gba::as_lhs(hi) + lo;        // fix8 result
auto keep_lo = gba::as_rhs(hi) + lo;        // fix4 result
auto wide    = gba::as_widening(lo) + hi;   // fix8 result
auto narrow  = gba::as_narrowing(hi) + lo;  // fix4 result (truncating conversion)
```

Container promotion example:

```cpp
using small = gba::fixed<char, 4>;
using med   = gba::fixed<short, 4>;

small a = 3.5_fx;
med   b = 2.0_fx;

auto r1 = gba::as_next_container(a) + b;
auto r2 = gba::as_word_storage(a) + b;
```

## Converting to and from integers

```cpp
gba::fixed<short> pos = 3.75_fx;

int whole = static_cast<int>(pos);   // truncates toward zero
short raw = gba::bit_cast(pos);      // raw scaled storage bits
```

`bit_cast` is useful for register writes that expect fixed-point bit patterns.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `fixed<int, 8> x = 3.5_fx;` | `FIXED x = float2fx(3.5f);` |
| `auto y = x * z;` | `FIXED y = fxmul(x, z);` |
| `auto q = x / z;` | `FIXED q = fxdiv(x, z);` |
| `int i = static_cast<int>(x);` | `int i = fx2int(x);` |

stdgba uses operators plus explicit promotion wrappers, so expressions stay readable
while still making precision/range trade-offs visible in code.
