# Keypad

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000130` | `reg_keyinput` | R | `const key_control` | `REG_KEYINPUT` |
| `0x4000132` | `reg_keycnt` | RW | `key_control` | `REG_KEYCNT` |

## `key_control`

```cpp
struct key_control {
    bool a : 1;
    bool b : 1;
    bool select : 1;
    bool start : 1;
    bool right : 1;
    bool left : 1;
    bool up : 1;
    bool down : 1;
    bool r : 1;
    bool l : 1;
    short : 4;
    bool irq_enabled : 1;
    bool irq_all : 1; // IRQ when ALL selected keys pressed
};
```

`reg_keyinput` is active low - a button reads `false` when pressed.

```cpp
if (!gba::reg_keyinput.a) { /* A is held */ }
```

For the high-level input helper (`gba::keypad`) with `held()`/`pressed()`/`released()` and axis helpers, see `book/src/reference/keypad.md`.
