# `gba::reciprocal` Reference

`gba::reciprocal` provides explicit fixed-point division strategies. Each one
trades accuracy, speed and ROM size in a different way. All strategies are
`constexpr` callable objects defined in `<gba/reciprocal>`. For guidance on
choosing a strategy, with worked examples, see
[Reciprocal Division](../concepts/reciprocal-division.md).

## Include

```cpp
#include <gba/reciprocal>
```

## Type summary

```cpp
namespace gba::reciprocal {
    enum class entry_format { u32, packed_u16 };

    // Strategy objects
    template<unsigned IndexBits, unsigned Refinements>
    inline constexpr normalized_strategy<IndexBits, Refinements> normalized{};

    template<unsigned Refinements>
    inline constexpr affine_strategy<Refinements> affine{};

    template<unsigned Min, unsigned Max, entry_format Format = entry_format::u32>
    inline constexpr direct_strategy<Min, Max, Format> direct{};

    template<auto Strategy>
    inline constexpr corrected_strategy<Strategy> corrected{};

    // Prepared dividers returned by strategy(denominator)
    template<SupportedFixed T> struct normalized_divider;
    template<unsigned Min, unsigned Max, entry_format Format> struct direct_divider;
    template<typename Prepared, typename Denominator> struct corrected_divider;
}
```

`SupportedFixed` is any `gba::fixed`-style type whose storage is 32 bits, such
as `gba::fixed<int, 16>`, `gba::fixed<int, 8>` or `gba::fixed<unsigned, 16>`.

## Call forms

Every strategy `s` supports these forms:

| Expression | Result | Description |
|------------|--------|-------------|
| `s(n, d)` | `T` | Divide once. |
| `s(d)` | a divider | Prepare `1/d` for reuse. |
| `s(nums, dens, out)` | `void` | Element-wise batch: `out[i] = s(nums[i], dens[i])`. |
| `div(n)` | `T` | Apply a prepared divider to one numerator. |
| `div(nums, out)` | `void` | Apply a prepared divider to a range: `out[i] = div(nums[i])`. |

Requirements on the arguments:

- For `normalized` and `affine`, `n` and `d` must have the same
  `SupportedFixed` type `T`.
- For `direct` and `corrected<direct<...>>`, `n` is a `SupportedFixed` type `T`
  and `d` is an integer of 32 bits or fewer, but not `bool`.
- Batch ranges must be contiguous and sized, and all ranges in a call must be
  the same size.
  - Numerator and output ranges must have the same element type, and the output
    range must be writable.
  - Integer denominator ranges must hold 32-bit integers.

Every form can be used in constant evaluation and gives the same results there
as at runtime. At runtime, the arithmetic runs in ARM kernels placed in IWRAM.

## Strategies

### `normalized<IndexBits, Refinements>`

| Parameter | Range | Meaning |
|-----------|-------|---------|
| `IndexBits` | 1-10 | log2 of the seed table entry count |
| `Refinements` | 0-3 | number of Newton-Raphson steps |

| Member | Value |
|--------|-------|
| `table_bytes` | `2 << IndexBits` (16-bit seeds) |
| `relative_error_bound` | `E` after `Refinements` steps of `E -> E*E + 2^-27`, starting from `2^-(IndexBits+1) + 2^-16` |

`s(d)` returns `normalized_divider<T>`. All strategies with the same
`IndexBits` share one seed table.

### `affine<Refinements>`

| Parameter | Range | Meaning |
|-----------|-------|---------|
| `Refinements` | 0-3 | number of Newton-Raphson steps |

| Member | Value |
|--------|-------|
| `table_bytes` | `0` |
| `relative_error_bound` | `E` after `Refinements` steps of `E -> E*E + 2^-27`, starting from `1/17 + 2^-28` |

`s(d)` returns `normalized_divider<T>`.

### `direct<Min, Max, Format>`

| Parameter | Constraint | Meaning |
|-----------|------------|---------|
| `Min` | `>= 1` | smallest supported denominator magnitude |
| `Max` | `>= Min` | largest supported denominator magnitude |
| `Format` | `u32` (default) or `packed_u16` | table entry format |

| Member | Value |
|--------|-------|
| `table_bytes` | `(Max - Min + 1) * 4` for `u32`, `(Max - Min + 1) * 2` for `packed_u16` |

Each table entry stores `ceil(2^P / d)`, where `P` is 32 for `u32` and 16 for
`packed_u16`. The table is built at compile time and stored in ROM. Negative
denominators whose magnitude is within `[Min, Max]` are accepted. `s(d)` returns
`direct_divider<Min, Max, Format>`.

### `corrected<Strategy>`

| Wrapped strategy | Requirement |
|------------------|-------------|
| `normalized<IndexBits, Refinements>` | `IndexBits >= 8 && Refinements >= 2` |
| `affine<Refinements>` | `Refinements >= 3` |
| `direct<Min, Max, entry_format::u32>` | always |
| `direct<Min, Max, entry_format::packed_u16>` | not supported |

| Member | Value |
|--------|-------|
| `table_bytes` | `Strategy.table_bytes`; the table is shared, not duplicated |

`s(d)` returns `corrected_divider<...>`. Wrapping a strategy that is not in the
table above fails to compile. `corrected` has no `relative_error_bound`, because
its results are exact.

## Error contracts

Here `Q` is the exact quotient magnitude in raw units, before truncation. The
contracts hold whenever the preconditions are met:

| Strategy | Result |
|----------|--------|
| `normalized`, `affine` | Differs from `Q` by at most `ceil(Q * relative_error_bound)` raw units. |
| `direct` | Estimated magnitude is at least the exact truncated magnitude, exceeding it by at most `ceil(abs(numerator_raw) / 2^P)` raw units. Negative results reverse the error direction. |
| `corrected<...>` | Exactly the widened quotient truncated toward zero, the same rounding as integer `/`. It is not the possibly overflowing intermediate that `gba::fixed` `operator/` uses. |

The sign of the result follows the usual rule for division.

## Preconditions

| Condition | Applies to |
|-----------|------------|
| `d != 0` | all strategies |
| `Min <= abs(d) <= Max` | `direct`, `corrected<direct<...>>` |
| the quotient is representable in `T` | all strategies; for unsigned `T`, the quotient must not be negative and nonzero |
| batch ranges all have the same size | batch forms |

A violation asserts in debug builds, traps in release builds, and fails
constant evaluation. Results are never silently clamped.

## Example

```cpp
#include <gba/fixed_point>
#include <gba/reciprocal>
#include <array>

using q16 = gba::fixed<int, 16>;
namespace rc = gba::reciprocal;

constexpr auto projection = rc::normalized<8, 1>;
constexpr auto exact_projection = rc::corrected<rc::normalized<8, 2>>;
constexpr auto table_free = rc::affine<3>;
constexpr auto scanline = rc::corrected<rc::direct<1, 160>>;

static_assert(projection.table_bytes == 512);
static_assert(table_free.table_bytes == 0);
static_assert(scanline.table_bytes == 640);

void example(q16 x, q16 y, q16 z) {
    q16 row = scanline(q16{120}, 20);   // exactly 6

    auto depth = projection(z);
    q16 px = depth(x);
    q16 py = depth(y);

    std::array<q16, 64> nums{}, out{};
    depth(nums, out);
}
```

## Related pages

- [Reciprocal Division](../concepts/reciprocal-division.md) - choosing a
  strategy, with benchmarks and examples
- [Fixed-Point Math](../concepts/fixed-point.md) - the `gba::fixed` types and
  ordinary division
