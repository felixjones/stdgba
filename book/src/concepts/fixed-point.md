# Fixed-Point Math

The GBA's ARM7TDMI processor has no floating-point unit. Floating-point operations are emulated in software and are extremely slow. Fixed-point arithmetic gives you fractional values using only integer instructions.

## The `fixed<>` type

```cpp
#include <gba/fixed_point>
using namespace gba::literals;

// 8.8 format (8 integer bits, 8 fractional bits)
gba::fixed<short> position = 3.5_fx;

// 16.16 format
gba::fixed<int> velocity = 0.125_fx;

// 24.8 format (matches tonclib's FIXED type)
gba::fixed<int, 8> angle = 1.5_fx;
```

### Common formats

| Type | Format | Range | Precision |
|------|--------|-------|-----------|
| `fixed<short>` | 8.8 | -128 to 127 | 1/256 |
| `fixed<int>` | 16.16 | -32768 to 32767 | 1/65536 |
| `fixed<int, 8>` | 24.8 | -8388608 to 8388607 | 1/256 |
| `fixed<short, 4>` | 12.4 | -2048 to 2047 | 1/16 |

## The `_fx` literal

The `_fx` suffix creates fixed-point values at compile time:

```cpp
using namespace gba::literals;

gba::fixed<short> a = 3.14_fx;   // Compile-time conversion
gba::fixed<short> b = 2_fx;      // Integer literals work too

auto c = a + b;                   // 5.14 (fixed<short>)
auto d = a * b;                   // 6.28 (fixed<short>)
```

`_fx` uses the type of the variable it is assigned to for determining the fixed-point format.

## Arithmetic

All standard operators work:

```cpp
gba::fixed<short> a = 10.5_fx;
gba::fixed<short> b = 3.25_fx;

auto sum = a + b;       // 13.75
auto diff = a - b;      // 7.25
auto prod = a * b;      // 34.125
auto quot = a / b;      // 3.230...

auto neg = -a;           // -10.5
bool gt = a > b;         // true
```

Multiplication and division use a wider intermediate type to avoid overflow. For `fixed<short>`, the intermediate is `int`. For `fixed<int>`, the intermediate is also `int` (for ARM performance). Use `precise<int>` to get `long long` intermediates when overflow is a concern.

## Converting to/from integers

```cpp
gba::fixed<short> pos = 3.75_fx;

int whole = static_cast<int>(pos);  // 3 (truncates toward zero)
short raw = gba::bit_cast(pos);            // Raw underlying integer (960 for 3.75 in 8.8)
```

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `fixed<int, 8> x = 3.5_fx;` | `FIXED x = float2fx(3.5f);` |
| `auto y = x * z;` | `FIXED y = fxmul(x, z);` |
| `auto q = x / z;` | `FIXED q = fxdiv(x, z);` |
| `int i = static_cast<int>(x);` | `int i = fx2int(x);` |

The stdgba version uses standard C++ operators instead of function macros, so expressions like `a * b + c` work naturally without nested calls.
