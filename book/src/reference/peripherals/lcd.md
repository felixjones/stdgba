# LCD

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000000` | `reg_dispcnt` | RW | `display_control` | `REG_DISPCNT` |
| `0x4000004` | `reg_dispstat` | RW | `display_status` | `REG_DISPSTAT` |
| `0x4000006` | `reg_vcount` | R | `const unsigned short` | `REG_VCOUNT` |

## `display_control`

```cpp
struct display_control {
    unsigned short video_mode : 3; // Video mode (0-5)
    bool cgb : 1;                  // CGB mode flag (read-only)
    unsigned short page : 1;       // Page select for mode 4/5
    bool hblank_oam_free : 1;      // Allow OAM access during HBlank
    bool linear_obj_tilemap : 1;   // OBJ VRAM 1D mapping
    bool disable : 1;              // Force blank
    bool enable_bg0 : 1;
    bool enable_bg1 : 1;
    bool enable_bg2 : 1;
    bool enable_bg3 : 1;
    bool enable_obj : 1;
    bool enable_win0 : 1;
    bool enable_win1 : 1;
    bool enable_obj_win : 1;
};
```

```cpp
gba::reg_dispcnt = { .video_mode = 3, .enable_bg2 = true };
```

## `display_status`

```cpp
struct display_status {
    const bool currently_vblank : 1;
    const bool currently_hblank : 1;
    const bool currently_vcount : 1;
    bool enable_irq_vblank : 1;
    bool enable_irq_hblank : 1;
    bool enable_irq_vcount : 1;
    short : 2;
    unsigned short vcount_setting : 8; // VCount trigger value
};
```

```cpp
gba::reg_dispstat = { .enable_irq_vblank = true };
```
