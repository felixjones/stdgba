# `gba::object_affine` Reference

`gba::object_affine` is the affine OAM object entry type from `<gba/video>`.

Use it with `gba::obj_aff_mem` when sprite rotation/scaling (OBJ affine transform) is required.

For regular objects with flip bits, see [`gba::object`](./object.md).

## Include

```cpp
#include <gba/video>
```

## Type summary

```cpp
struct object_affine {
    // Attribute 0
    unsigned short y : 8;
    bool affine : 1 = true;
    bool double_size : 1;
    gba::mode mode : 2;
    bool mosaic : 1;
    gba::depth depth : 1;
    gba::shape shape : 2;

    // Attribute 1
    unsigned short x : 9;
    unsigned short affine_index : 5;
    unsigned short size : 2;

    // Attribute 2
    unsigned short tile_index : 10;
    unsigned short background : 2;
    unsigned short palette_index : 4;
};
```

`sizeof(gba::object_affine) == 6` bytes.

## Typical usage

```cpp
gba::obj_aff_mem[0] = {
    .y = 80,
    .x = 120,
    .affine_index = 0,
    .shape = gba::shape_square,
    .size = 1,
    .depth = gba::depth_4bpp,
    .tile_index = 0,
};

// Configure affine matrix 0 through mem_obj_affa/b/c/d as needed.
```

## Field notes

- `affine`: set for affine rendering mode (enabled by default in the struct).
- `double_size`: doubles the render box so rotated/scaled sprites are less likely to clip.
- `affine_index`: selects one of 32 affine parameter sets (`0..31`).
- `shape` + `size`: still determine the base dimensions before affine transform.
- `flip_x`/`flip_y` do not exist on affine entries; transform comes from the affine matrix.

## Affine parameter memory

`<gba/video>` provides these typed views over OAM affine parameters:

- `gba::mem_obj_affa` (`pa`)
- `gba::mem_obj_affb` (`pb`)
- `gba::mem_obj_affc` (`pc`)
- `gba::mem_obj_affd` (`pd`)

## Related pages

- [Sprites (Objects)](../graphics/sprites.md)
- [Video Memory](./peripherals/video-memory.md)
- [`gba::object` Reference](./object.md)
