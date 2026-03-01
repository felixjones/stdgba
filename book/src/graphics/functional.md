# Functional Callbacks (`<gba/functional>`)

`<gba/functional>` provides `gba::function<Sig>` and `gba::handler<Args...>`: small, type-erased callables for callback-style code on the GBA.

Use this when you want lambda/function-pointer ergonomics without pulling in `std::function`.

## What it provides

- `gba::function<Ret(Args...)>`: generic callable wrapper
- `gba::handler<Args...>`: shorthand for `gba::function<void(Args...)>`
- Small-buffer storage (12 bytes inline)
- No dynamic allocation path in the wrapper itself

```cpp
#include <gba/functional>

gba::function<int(int, int)> add = [](int a, int b) {
    return a + b;
};

int sum = add(2, 3); // 5
```

## `gba::handler` shorthand

`gba::handler` is convenient for event callbacks where no return value is needed.

```cpp
#include <gba/functional>

gba::handler<int> on_damage = [](int amount) {
    // React to damage
    (void)amount;
};

if (on_damage) {
    on_damage(12);
}
```

## Lifetime and assignment model

`gba::function` can be empty (`nullptr`) or hold one callable.

```cpp
gba::function<void()> f;
if (!f) {
    f = [] {
        // Installed callback
    };
}

f = nullptr; // Clear callback
```

- Default construction gives an empty callback
- `operator bool()` checks whether a target is installed
- `operator=(nullptr)` clears the current target
- Copy/move construction and assignment are supported

## Embedded constraints

`gba::function` intentionally keeps a small inline buffer. This gives predictable storage and avoids allocator dependencies.

Unlike `std::function`, there is no heap fallback: the callable object itself is always stored in-place inside the `gba::function`.

That means an oversized callable is rejected at compile time (a `static_assert` in the constructor).

The target callable type must:

- fit in 12 bytes (`sizeof(callable) <= 12`), and
- be `nothrow` move constructible.

Typical small captures (for example, one pointer/reference-sized capture) fit well.

If your lambda would be too large, store the state elsewhere and capture a pointer (or reference) instead:

```cpp
struct big_state {
    int data[64];
};

big_state state{};

gba::function<void()> f = [&state] {
    // Capture is one pointer/reference, so it fits.
    state.data[0] = 1;
};
```

## Common usage pattern

A typical game-system pattern is "optional callback installed by the owner":

```cpp
struct menu {
    gba::handler<int> on_select;

    void select(int index) {
        if (on_select) {
            on_select(index);
        }
    }
};
```

This keeps callback plumbing lightweight and explicit.

## When to use what

- Use a raw function pointer for fixed, capture-free callbacks.
- Use `gba::function`/`gba::handler` when you need captures or interchangeable callable types.
- Prefer small captures; if captures grow, store data elsewhere and capture a pointer/reference.

## Related pages

- [Key Input](../concepts/key-input.md) - practical per-frame edge-driven logic
- [Interrupts](../concepts/interrupts.md) - IRQ callback pathways
- [Memory Utilities](./memory.md) - ownership and utility types for embedded code
