# String Formatting

stdgba provides a compile-time string formatting library designed for GBA constraints. Format strings are parsed at compile time, and arguments are bound by name using user-defined literals.

## Basic usage

```cpp
#include <gba/format>
using namespace gba::literals;

// Define a format string (parsed at compile time)
constexpr auto fmt = "HP: {hp}/{max}"_fmt;

// Format into a buffer
char buf[32];
fmt.to(buf, "hp"_arg = 42, "max"_arg = 100);
// buf contains "HP: 42/100"
```

## Without literals

If you prefer not to use literal operators:

```cpp
constexpr auto fmt = gba::format::make_format<"HP: {hp}/{max}">();
constexpr auto hp = gba::format::make_arg<"hp">();
constexpr auto max_hp = gba::format::make_arg<"max">();

char buf[32];
fmt.to(buf, hp = 42, max_hp = 100);
```

## Placeholder forms

| Form | Meaning |
|------|---------|
| `{name}` | Named placeholder with default formatting |
| `{name:spec}` | Named placeholder with format spec |
| `{}` | Implicit positional placeholder |
| `{:spec}` | Implicit positional placeholder with format spec |
| `{0}` | Explicit positional placeholder |
| `{0:spec}` | Explicit positional placeholder with format spec |
| `{{` / `}}` | Escaped literal braces |

## Format spec grammar

The format spec follows a Python-style mini-language:

```text
[[fill]align][sign][#][0][width][grouping][.precision][type]
```

| Field | Syntax | Default | Applies to |
|-------|--------|---------|------------|
| fill | any ASCII character before align | `' '` | all aligned outputs |
| align | `<` left, `>` right, `^` centre, `=` sign-aware | type-dependent | all (`=` is numeric-only) |
| sign | `+`, `-`, or space | `-` behaviour | numeric types |
| `#` | alternate form | off | integral prefixes, fixed-point decimal point retention |
| `0` | zero-fill (equivalent to fill=`0` align=`=`) | off | numeric types |
| width | decimal digits | 0 | all types |
| grouping | `,` or `_` | none | integer, fixed-point, angle decimal output |
| precision | `.` followed by digits | unset | strings, fixed-point, angle degrees/radians/turns, angle hex |
| type | trailing presentation character | per value category | see tables below |

### Integer type codes

| Code | Meaning | `#` alternate form |
|------|---------|--------------------|
| (default) | decimal | - |
| `d` | decimal | - |
| `b` | binary | `0b` prefix |
| `o` | octal | `0o` prefix |
| `x` | hex lowercase | `0x` prefix |
| `X` | hex uppercase | `0X` prefix |
| `n` | grouped decimal | - |
| `c` | single character from code point | - |

Integer grouping inserts a separator every 3 digits for decimal/octal, or every 4 digits for binary/hex.

### String type codes

| Code | Meaning |
|------|---------|
| (default) | emit string as-is |
| `s` | same as default |

Precision truncates the string to at most N characters before width/alignment is applied.

### Fixed-point type codes

| Code | Meaning |
|------|---------|
| (default) | fixed decimal, trailing fractional zeros trimmed |
| `f` / `F` | fixed decimal with exactly `.N` fractional digits |
| `e` | scientific notation lowercase (`1.23e+03`) |
| `E` | scientific notation uppercase (`1.23E+03`) |
| `g` | general format -- uses fixed for small values, scientific for large |
| `G` | general format uppercase |
| `%` | multiply by 100 and append `%` |

Grouping applies to the integer part only. `#` with `.0f` retains the decimal point.

### Angle type codes

| Code | Meaning |
|------|---------|
| (default) | degrees |
| `r` | radians |
| `t` | turns (0.0 - 1.0) |
| `i` | raw integer value of the angle storage |
| `x` | raw hex lowercase |
| `X` | raw hex uppercase |

For `x`/`X`, precision controls the number of emitted hex digits (most-significant digits are kept). If omitted, the native width is used (8 for `gba::angle`, `Bits/4` for `gba::packed_angle<Bits>`). `#` adds a `0x`/`0X` prefix.

## Examples

### Integers

```cpp
constexpr auto fmt = "Addr: {a:#010x}"_fmt;
char buf[16];
fmt.to(buf, "a"_arg = 0x2A);
// buf contains "Addr: 0x0000002a"
```

```cpp
constexpr auto fmt = "Gold: {gold:_d}"_fmt;
char buf[16];
fmt.to(buf, "gold"_arg = 9999);
// buf contains "Gold: 9_999"
```

### Strings

```cpp
constexpr auto fmt = "{name:*^7.3}"_fmt;
char buf[16];
fmt.to(buf, "name"_arg = "Hello");
// buf contains "**Hel**"
```

### Fixed-point

```cpp
#include <gba/fixed_point>
using fix8 = gba::fixed<int, 8>;

constexpr auto fmt = "X: {x:,.2f}"_fmt;
char buf[32];
fmt.to(buf, "x"_arg = fix8(1234.5));
// buf contains "X: 1,234.50"
```

Scientific notation:

```cpp
constexpr auto fmt = "X: {x:.2e}"_fmt;
char buf[32];
fmt.to(buf, "x"_arg = fix8(1234.5));
// buf contains "X: 1.23e+03"
```

Percent formatting:

```cpp
constexpr auto fmt = "HP: {x:%}"_fmt;
char buf[32];
fmt.to(buf, "x"_arg = fix8(0.5));
// buf contains "HP: 50%"
```

### Angles

```cpp
#include <gba/angle>
using namespace gba::literals;

constexpr auto fmt = "Angle: {a:.4r}"_fmt;
char buf[32];
fmt.to(buf, "a"_arg = 90_deg);
// buf contains "Angle: 1.5708"
```

Compact raw hex view of a packed angle:

```cpp
constexpr auto fmt = "Rot: {a:#.4X}"_fmt;
char buf[16];
fmt.to(buf, "a"_arg = gba::packed_angle16{0x4000});
// buf contains "Rot: 0X4000"
```

### Compile-time formatting

```cpp
constexpr auto result = "HP: {hp}"_fmt.to_static("hp"_arg = 42);
// result is a compile-time array containing "HP: 42"
```

`to_static` also accepts `gba::literals::fixed_literal` values (e.g. `3.14_fx`), which are compile-time-only and cannot be used with runtime output paths.

## Typewriter generator

The generator API emits one character at a time, perfect for RPG-style text rendering:

```cpp
constexpr auto fmt = "You found {item}!"_fmt;

auto gen = fmt.generator("item"_arg = "Sword");
while (auto ch = gen()) {
    draw_char(*ch);
    wait_frames(2);  // Typewriter delay
}
```

## Lazy (lambda) arguments

Arguments can also be bound to a callable (for example, a lambda). The callable is invoked when formatting reaches that placeholder.

This is useful for typewriter-style output: you can defer looking up a value until the moment the generator starts emitting that argument.

```cpp
constexpr auto fmt = "HP: {hp}/{max}"_fmt;

// player.hp is read when the generator reaches {hp}, not when it is created.
auto gen = fmt.generator(
    "hp"_arg = [&] { return player.hp; },
    "max"_arg = [&] { return player.max_hp; }
);

while (auto ch = gen()) {
    draw_char(*ch);
    wait_frames(2);
}
```

For string arguments, the supplier should return a stable pointer (for example, a string stored in memory) rather than a temporary buffer.

## Word boundary lookahead

The generator provides `until_break()` to check how many characters remain until the next word boundary. Use this for line wrapping:

```cpp
auto gen = fmt.generator("hp"_arg = 42);
int col = 0;
while (auto ch = gen()) {
    if (col + gen.until_break() > 30) {
        newline();
        col = 0;
    }
    draw_char(*ch);
    ++col;
}
```

## Output paths

All output paths share the same rendering semantics and produce identical results for the same inputs:

| Path | Description |
|------|-------------|
| `generator()` | Streaming character-by-character emission |
| `to(buf, ...)` | Render into a caller-provided buffer |
| `to_array(...)` | Render into a `std::array` |
| `to_static(...)` | Compile-time render into a constexpr array |

## Invalid spec rejection

Invalid format spec combinations are rejected at compile time. Examples of rejected specs:

| Spec | Reason |
|------|--------|
| `+s` | sign on string type |
| `,s` | grouping on string type |
| `=s` | sign-aware alignment on string type |
| `.2i` | precision on raw integer angle type |
| `#c` | alternate form on character type |

## Deferred features

The following features are not supported in the current implementation:

- `!s` / `!r` conversion flags
- Dynamic width / precision (`{x:{w}.{p}f}`)
- Nested replacement fields inside format specs
- Runtime-parsed format strings
- Built-in `float` / `double` formatting

## Design notes

- Format strings are parsed entirely at compile time - no runtime parsing overhead
- Arguments are bound by name, not position, making format strings self-documenting
- Arguments may be bound to callables (lambdas) for lazy evaluation at placeholder time
- The generator API emits digits MSB-first, enabling typewriter effects without buffering
- No heap allocation - all formatting uses caller-provided buffers
- The generator uses a deterministic phase/state machine with category-specialised emission states
