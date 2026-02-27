# Key Input

The GBA has 10 buttons: A, B, L, R, Start, Select, and the 4-direction D-pad. stdgba provides edge detection (pressed/released this frame) and axis helpers.

## Reading keys

```cpp
#include <gba/keyinput>
#include <gba/peripherals>

gba::keypad keys;

// In your game loop:
for (;;) {
    gba::VBlankIntrWait();
    keys = gba::reg_keyinput;  // Sample hardware register

    if (keys.held(gba::key_a)) {
        // A is currently held down
    }

    if (keys.pressed(gba::key_b)) {
        // B was just pressed this frame (edge detection)
    }

    if (keys.released(gba::key_start)) {
        // Start was just released this frame
    }
}
```

### How it works

`gba::keypad` stores two frames of key state internally. When you assign from `gba::reg_keyinput`, it shifts the current state to "previous" and reads the new state from hardware. This enables edge detection without any manual bookkeeping.

## D-pad axes

For movement, use the axis helpers:

```cpp
int dx = keys.xaxis();  // -1 (left), 0, or 1 (right)
int dy = keys.yaxis();  // -1 (up), 0, or 1 (down)

player_x += dx;
player_y += dy;
```

These return a tri-state value based on the D-pad. If both left and right are held simultaneously, they cancel out to 0.

## Key constants

| Constant | Button |
|----------|--------|
| `gba::key_a` | A |
| `gba::key_b` | B |
| `gba::key_l` | L shoulder |
| `gba::key_r` | R shoulder |
| `gba::key_start` | Start |
| `gba::key_select` | Select |
| `gba::key_up` | D-pad up |
| `gba::key_down` | D-pad down |
| `gba::key_left` | D-pad left |
| `gba::key_right` | D-pad right |

Combine keys with `operator|`:

```cpp
auto combo = gba::key_a | gba::key_b;
if (keys.held(combo)) {
    // Both A and B are held
}
```

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `keys = gba::reg_keyinput;` | `key_poll();` |
| `keys.held(gba::key_a)` | `key_is_down(KEY_A)` |
| `keys.pressed(gba::key_a)` | `key_hit(KEY_A)` |
| `keys.released(gba::key_a)` | `key_released(KEY_A)` |
| `keys.xaxis()` | `key_tri_horz()` |
| `keys.yaxis()` | `key_tri_vert()` |
