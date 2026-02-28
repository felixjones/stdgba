# Mosaic

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x400004C` | `reg_mosaicbg` | RW | `mosaic_control` | `REG_MOSAIC` (lo) |
| `0x400004D` | `reg_mosaicobj` | RW | `mosaic_control` | `REG_MOSAIC` (hi) |

## `mosaic_control`

```cpp
struct mosaic_control {
    unsigned char add_h : 4; // Horizontal stretch (0-15)
    unsigned char add_v : 4; // Vertical stretch (0-15)
};
```
