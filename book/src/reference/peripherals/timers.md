# Timers

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000100` | `reg_tmcnt_l[4]` | RW | `unsigned short[4]` | `REG_TM0D`..`REG_TM3D` |
| `0x4000100` | `reg_tmcnt_l_stat[4]` | R | `const unsigned short[4]` | `REG_TM0D` (read) |
| `0x4000100` | `reg_tmcnt_l_reload[4]` | W | `volatile unsigned short[4]` | `REG_TM0D` (write) |
| `0x4000102` | `reg_tmcnt_h[4]` | RW | `timer_control[4]` | `REG_TM0CNT`..`REG_TM3CNT` |
| `0x4000100` | `reg_tmcnt[4]` | RW | `timer_config[4]` | - |

All timer arrays have a stride of 4 bytes between channels.

## `timer_control`

```cpp
struct timer_control {
    cycles cycles : 2; // cycles_1 / cycles_64 / cycles_256 / cycles_1024
    bool cascade : 1;  // Cascade from previous timer
    short : 3;
    bool overflow_irq : 1;
    bool enabled : 1;
};
```

`timer_config` is a `plex<unsigned short, timer_control>` that writes the reload value and control register as a single 32-bit store.

```cpp
gba::reg_tmcnt_h[0] = { .cycles = gba::cycles_1024, .enabled = true };
```
