# Math

The BIOS provides integer math helpers with zero floating-point dependency.

## Square root

```cpp
#include <gba/bios>

constexpr auto root = gba::Sqrt(256u); // 16
```

With constant inputs, wrappers can fold at compile time.

## Angle helpers

```cpp
#include <gba/bios>

int dx = 100;
int dy = 50;
auto theta = gba::ArcTan2(dx, dy); // packed 16-bit angle domain
```

- `gba::ArcTan(x)` computes arctangent from one input.
- `gba::ArcTan2(x, y)` follows BIOS argument order.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::Sqrt(n)` | `Sqrt(n)` |
| `gba::ArcTan(x)` | `ArcTan(x)` |
| `gba::ArcTan2(x, y)` | `ArcTan2(x, y)` |
