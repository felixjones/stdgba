# String Formatting

stdgba provides a compile-time string formatting library designed for GBA constraints. Format strings are parsed at compile time, and arguments are bound by name using user-defined literals.

## Basic usage

```cpp
#include <gba/format>
using namespace gba::format::literals;

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

## Format specifiers

| Syntax | Meaning |
|--------|---------|
| `{x}` | Default format |
| `{x:d}` | Decimal integer |
| `{x:x}` | Hex lowercase |
| `{x:X}` | Hex uppercase |

```cpp
constexpr auto fmt = "Addr: {a:X}"_fmt;
char buf[16];
fmt.to(buf, "a"_arg = 0xFF);
// buf contains "Addr: FF"
```

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

## Design notes

- Format strings are parsed entirely at compile time -- no runtime parsing overhead
- Arguments are bound by name, not position, making format strings self-documenting
- The generator API emits digits MSB-first, enabling typewriter effects without buffering
- No heap allocation -- all formatting uses caller-provided buffers
