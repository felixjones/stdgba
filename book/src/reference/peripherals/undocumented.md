# Undocumented Registers

These are functional but not part of the community-documented register set. Access via the `gba::undocumented` namespace.

| Address     | stdgba                              | Access | Type                           | Common Name                |
|-------------|-------------------------------------|--------|--------------------------------|----------------------------|
| `0x4000002` | `undocumented::reg_stereo_3d`       | RW     | `bool`                         | GREENSWAP                  |
| `0x4000008` | `undocumented::reg_bgcnt_stereo[4]` | RW     | `background_control_stereo[4]` | `REG_BG0CNT`..`REG_BG3CNT` |
| `0x4000300` | `undocumented::reg_postflg`         | RW     | `bool`                         | POSTFLG                    |
| `0x4000301` | `undocumented::reg_haltcnt`         | RW     | `halt_control`                 | HALTCNT                    |
| `0x4000410` | `undocumented::reg_obj_center`      | W      | `volatile char`                | -                          |
| `0x4000800` | `undocumented::reg_memcnt`          | RW     | `memory_control`               | Internal Memory Control    |

## `background_control_stereo`

Stereo 3D view of `background_control`. Bits 4-5 select the eye and are ignored unless `reg_stereo_3d` is enabled.

```cpp
struct background_control_stereo {
    unsigned short priority : 2;    // BG priority (0 = highest)
    unsigned short charblock : 2;   // Character base block (0-3)
    stereo_mode stereo : 2;         // stereo_both, stereo_right, stereo_left, stereo_duplicate_left
    bool mosaic : 1;                // Enable mosaic effect
    bool bpp8 : 1;                  // 8bpp mode (false = 4bpp)
    unsigned short screenblock : 5; // Screen base block (0-31)
    bool wrap_affine_tiles : 1;     // Wrap for affine BGs
    unsigned short size : 2;        // BG size
};
```

```cpp
gba::undocumented::reg_bgcnt_stereo[0] = { .stereo = gba::undocumented::stereo_left, .screenblock = 31 };
```
