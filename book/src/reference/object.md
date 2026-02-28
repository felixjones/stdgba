# `gba::object` Reference

`gba::object` is the regular (non-affine) OAM object entry type from `<gba/video>`.

Use it with `gba::obj_mem` when you want standard sprite placement with optional horizontal/vertical flipping.

For affine objects, see [`gba::object_affine`](./object-affine.md).

## Include

```cpp
#include <gba/video>
```

## Type summary

```cpp
struct object {
    // Attribute 0
    unsigned short y : 8;
    bool : 1;
    bool disable : 1;
    gba::mode mode : 2;
    bool mosaic : 1;
    gba::depth depth : 1;
    gba::shape shape : 2;

    // Attribute 1
    unsigned short x : 9;
    short : 3;
    bool flip_x : 1;
    bool flip_y : 1;
    unsigned short size : 2;

    // Attribute 2
    unsigned short tile_index : 10;
    unsigned short background : 2;
    unsigned short palette_index : 4;
};
```

`sizeof(gba::object) == 6` bytes.

## Typical usage

```cpp
gba::obj_mem[0] = {
    .y = 80,
    .x = 120,
    .shape = gba::shape_square,
    .size = 1,          // 16x16 for square sprites
    .depth = gba::depth_4bpp,
    .tile_index = 0,
    .palette_index = 0,
};
```

## Field notes

- `disable`: hide this object without clearing its other fields.
- `mode`: object blend/window mode (`mode_normal`, `mode_blend`, `mode_window`).
- `depth`: choose `depth_4bpp` (16-colour banked palette) or `depth_8bpp` (256-colour OBJ palette).
- `shape` + `size`: together determine dimensions.
- `flip_x`/`flip_y`: valid for regular objects.
- `background`: OBJ priority relative to backgrounds (`0` highest, `3` lowest).

## Regular vs affine comparison

| Aspect | `gba::object` (regular) | `gba::object_affine` |
|---|---|---|
| Typed OAM view | `gba::obj_mem` | `gba::obj_aff_mem` |
| Attr0 mode bit | `disable` hide flag | `affine` enabled, optional `double_size` |
| Attr1 control bits | `flip_x` / `flip_y` | `affine_index` (`0..31`) |
| Rotation/scaling | Not supported | Supported via affine matrix |
| Transform source | Flip bits only | `mem_obj_affa/b/c/d` entry selected by `affine_index` |
| Shared fields | `x`, `y`, `shape`, `size`, `tile_index`, `background`, `palette_index`, `depth`, `mode`, `mosaic` | Same shared fields |
| Best fit | Standard sprites, mirroring, UI, low overhead | Rotating/scaling sprites, camera-facing effects |

## Shape/size table

| Shape | Size 0 | Size 1 | Size 2 | Size 3 |
|---|---|---|---|---|
| Square | 8x8 | 16x16 | 32x32 | 64x64 |
| Wide | 16x8 | 32x8 | 32x16 | 64x32 |
| Tall | 8x16 | 8x32 | 16x32 | 32x64 |

## Related symbols

- `gba::obj_mem` - typed OAM as `object[128]`
- `gba::tile_index(ptr)` - compute OBJ tile index from an OBJ VRAM pointer
- `gba::mem_vram_obj` - raw object VRAM

## Related pages

- [Sprites (Objects)](../graphics/sprites.md)
- [Video Memory](./peripherals/video-memory.md)
- [`gba::object_affine` Reference](./object-affine.md)
