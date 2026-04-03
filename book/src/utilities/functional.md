# Functional

`<gba/functional>` provides a lightweight, heap-free type-erased callable wrapper
designed for GBA embedded development.

## Overview

The standard library's `std::function` allocates on the heap when the stored
callable is too large for its internal buffer, and its virtual-dispatch overhead
is higher than necessary for a single-core embedded target. `gba::function`
avoids both problems:

- **No heap allocation** -- callables are stored in a 12-byte inline buffer.
  Oversized callables are rejected at compile time via `static_assert`.
- **Function-pointer dispatch** -- avoids virtual-table overhead.
- **Copyable and movable** -- full value semantics, including assignment from
  `nullptr`.

## `gba::function<Sig>`

```cpp
#include <gba/functional>

gba::function<void(int)> fn = [](int x) { /* ... */ };
fn(42);
```

The template parameter `Sig` is a function signature such as `void(int)` or
`int(float, float)`.

### Construction

```cpp
// Default-construct (null / empty)
gba::function<void()> empty;

// Construct from a lambda
int counter = 0;
gba::function<void()> inc = [&counter] { ++counter; };

// Construct from a free function
void on_tick() { /* ... */ }
gba::function<void()> tick = on_tick;

// Assign null
inc = nullptr;
```

### Invocation

```cpp
if (fn) {
    fn(42);   // only call when non-null
}
```

Invoking a null `gba::function` is undefined behaviour -- guard with the `bool`
conversion operator before calling.

### Null checks and reassignment

```cpp
gba::function<void(int)> fn;

if (!fn) {
    fn = [](int x) { /* ... */ };
}

fn = nullptr;  // reset to empty
```

## `gba::handler<Args...>`

`handler` is a convenience alias for void-returning functions:

```cpp
// Equivalent to gba::function<void(int)>
gba::handler<int> h = [](int x) { process(x); };
h(42);
```

It is the idiomatic type for GBA event callbacks (VBlank handler, key-press
callback, etc.) where the return value is not needed.

## Small-buffer constraint

The inline storage is **12 bytes**. Any callable larger than 12 bytes triggers a
`static_assert` at compile time:

```cpp
int a, b, c, d;   // four ints = 16 bytes - too large
gba::function<void()> fn = [a, b, c, d] { /* ... */ };
// error: Callable too large for small buffer optimization
```

To capture more state, store it in a struct and capture a pointer or reference
to it instead:

```cpp
struct State {
    int a, b, c, d;
};

State state{1, 2, 3, 4};

// Capture a pointer - sizeof(State*) == 4 bytes, fits easily
gba::function<void()> fn = [&state] {
    state.a += state.b;
};
```

## Usage with `gba::irq_handler`

`gba::irq_handler` (from `<gba/interrupt>`) stores a `gba::handler<gba::irq>`,
so any callable that accepts a `gba::irq` can be assigned directly:

```cpp
#include <gba/interrupt>

gba::irq_handler = [](gba::irq irq) {
    if (irq.vblank) { /* frame logic */ }
};
```

For the full interrupt setup and `irq_handler` API (`has_value`, `swap`,
`reset`, `nullisr`), see [Interrupts](../concepts/interrupts.md).

## Type sizes

| Type | Size |
|---|---|
| `gba::function<void()>` | 20 bytes |
| `gba::function<void(int)>` | 20 bytes |
| `gba::handler<>` | 20 bytes (alias) |

The 20-byte total comes from: 4-byte invoke pointer + 4-byte ops-table pointer +
12-byte inline storage.

## Summary

| Feature | `gba::function` | `std::function` |
|---|---|---|
| Heap allocation | Never | When callable > SBO buffer |
| Inline storage | 12 bytes (fixed) | Implementation-defined |
| Oversized callable | `static_assert` at compile time | Heap fallback |
| Dispatch mechanism | Function pointer | Virtual dispatch |
| Null / empty state | Yes (`nullptr` / default) | Yes |
| Copy / move | Yes | Yes |
