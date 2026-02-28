# Backgrounds

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000008` | `reg_bgcnt[4]` | RW | `background_control[4]` | `REG_BG0CNT`..`REG_BG3CNT` |
| `0x4000010` | `reg_bgofs[4][2]` | W | `volatile short[4][2]` | `REG_BG0HOFS` etc. |
| `0x4000020` | `reg_bgp[2][4]` | W | `volatile fixed<short>[2][4]` | `REG_BG2PA` etc. |
| `0x4000028` | `reg_bgx[2]` | W | `volatile fixed<int,8>[2]` | `REG_BG2X`, `REG_BG3X` |
| `0x400002C` | `reg_bgy[2]` | W | `volatile fixed<int,8>[2]` | `REG_BG2Y`, `REG_BG3Y` |
| `0x4000020` | `reg_bg_affine[2]` | W | `volatile background_matrix[2]` | `REG_BG_AFFINE` |

## `background_control`

```cpp
struct background_control {
    unsigned short priority : 2;    // BG priority (0 = highest)
    unsigned short charblock : 2;   // Character base block (0-3)
    short : 2;
    bool mosaic : 1;                // Enable mosaic effect
    bool bpp8 : 1;                  // 8bpp mode (false = 4bpp)
    unsigned short screenblock : 5; // Screen base block (0-31)
    bool wrap_affine_tiles : 1;     // Wrap for affine BGs
    unsigned short size : 2;        // BG size
};
```

```cpp
gba::reg_bgcnt[0] = { .screenblock = 31, .charblock = 0 };
```

## `background_matrix`

```cpp
struct background_matrix {
    fixed<short> p[4]; // pa, pb, pc, pd
    fixed<int, 8> x;   // Reference point X
    fixed<int, 8> y;   // Reference point Y
};
```

The scroll registers `reg_bgofs[bg][axis]` are indexed as `[bg_index][0=x, 1=y]`. The affine registers `reg_bgp[bg][coeff]` are indexed relative to BG2 (index 0 = BG2, index 1 = BG3).
