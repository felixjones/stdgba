 # Affine Setup
Use BIOS affine helpers to build matrix coefficients for backgrounds and objects.

## Background affine matrix

```cpp
#include <gba/bios>
#include <gba/literals>

using namespace gba::literals;

gba::background_parameters bg{
	.tex_x = 128.0_fx,
	.tex_y = 128.0_fx,
	.scr_x = 120,
	.scr_y = 80,
	.sx = 1.0_fx,
	.sy = 1.0_fx,
	.alpha = 15_deg,
};

gba::background_matrix out{};
gba::BgAffineSet(&bg, &out, 1);
```

## Object affine matrix

`gba::ObjAffineSet(...)` follows the same pattern for object transforms.

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::BgAffineSet(&in, &out, n)` | `BgAffineSet(src, dst, n)` |
| `gba::ObjAffineSet(&in, &out, n, stride)` | `ObjAffineSet(src, dst, n, stride)` |
