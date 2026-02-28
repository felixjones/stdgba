# Windowing

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000040` | `reg_winh[2]` | W | `volatile unsigned char[2]` | `REG_WIN0H` |
| `0x4000044` | `reg_winv[2]` | W | `volatile unsigned char[2]` | `REG_WIN0V` |
| `0x4000048` | `reg_winin[2]` | RW | `window_control[2]` | `REG_WININ` |
| `0x400004A` | `reg_winout` | RW | `window_control` | `REG_WINOUT` |
| `0x400004B` | `reg_winobj` | RW | `window_control` | `REG_WINOUT` (hi byte) |

## `window_control`

```cpp
struct window_control {
    bool enable_bg0 : 1;
    bool enable_bg1 : 1;
    bool enable_bg2 : 1;
    bool enable_bg3 : 1;
    bool enable_obj : 1;
    bool enable_color_effect : 1;
};
```

```cpp
gba::reg_winin[0] = { .enable_bg0 = true, .enable_obj = true };
```
