# `gba::keypad` Reference

`gba::keypad` is the high-level input state tracker from `<gba/keyinput>`. It wraps active-low keypad hardware semantics and provides frame-based edge detection helpers.

For raw register details (`reg_keyinput`, `reg_keycnt`), see [Peripheral Registers: Keypad](./peripherals/keypad.md).

## Include

```cpp
#include <gba/keyinput>
#include <gba/peripherals>
```

## Type summary

```cpp
struct keypad {
    constexpr keypad& operator=(key_control keys) noexcept;

    template<template<typename> typename LogicalOp = std::logical_and, typename... Keys>
    constexpr bool held(Keys... keys) const noexcept;

    template<template<typename> typename LogicalOp = std::logical_and, typename... Keys>
    constexpr bool pressed(Keys... keys) const noexcept;

    template<template<typename> typename LogicalOp = std::logical_and, typename... Keys>
    constexpr bool released(Keys... keys) const noexcept;

    constexpr int xaxis() const noexcept;
    constexpr int i_xaxis() const noexcept;
    constexpr int yaxis() const noexcept;
    constexpr int i_yaxis() const noexcept;
    constexpr int lraxis() const noexcept;
    constexpr int i_lraxis() const noexcept;
};
```

## Frame update contract

`keypad` stores previous and current state internally. Update it by assigning from `gba::reg_keyinput` once per game frame:

```cpp
gba::keypad keys;

for (;;) {
    gba::VBlankIntrWait();
    keys = gba::reg_keyinput;

    // Query after exactly one sample per frame
}
```

Sampling multiple times in one frame advances history multiple times, which can make `pressed()`/`released()` behaviour appear inconsistent.

## Query methods

`Keys...` must be `gba::key` masks (`gba::key_a`, `gba::key_left`, etc.).

### `held(keys...)`

Returns whether keys are currently down.

```cpp
if (keys.held(gba::key_a)) {
    // A is down this frame
}
```

### `pressed(keys...)`

Returns whether keys transitioned up -> down on this frame.

```cpp
if (keys.pressed(gba::key_start)) {
    // Start edge this frame
}
```

### `released(keys...)`

Returns whether keys transitioned down -> up on this frame.

```cpp
if (keys.released(gba::key_b)) {
    // B release edge this frame
}
```

## Logical operators

All three query methods default to `std::logical_and` semantics for multiple keys.

```cpp
if (keys.held(gba::key_l, gba::key_r)) {
    // L and R both held
}
```

You can also select `std::logical_or` or `std::logical_not`:

```cpp
if (keys.pressed<std::logical_or>(gba::key_a, gba::key_b)) {
    // A or B was newly pressed
}
```

## Axis helpers

Axis helpers are tri-state (`-1`, `0`, `1`) from the current key sample.

- `xaxis()`: `-1` left, `+1` right
- `i_xaxis()`: inverted horizontal axis
- `yaxis()`: `-1` down, `+1` up (mathematical convention)
- `i_yaxis()`: inverted vertical axis (`+1` down for screen-space movement)
- `lraxis()`: `-1` L, `+1` R
- `i_lraxis()`: inverted shoulder axis

## Key masks and combos

Use `operator|` on `gba::key` constants to build combinations:

```cpp
auto combo = gba::key_a | gba::key_b;
if (keys.held(combo)) {
    // A+B held
}
```

`gba::reset_combo` is predefined as `A + B + Select + Start`.

## Related pages

- [Key Input](../concepts/key-input.md) - practical gameplay patterns
- [Peripheral Registers: Keypad](./peripherals/keypad.md) - raw register layout and IRQ bits
