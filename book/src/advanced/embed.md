# Embedding Images

The `<gba/embed>` header converts image files into GBA-ready data entirely at compile time. Combined with C23's `#embed` directive, this replaces external asset pipelines like grit with a single `#include` and a constexpr variable.

For procedural sprite generation without source image files, see [Shapes](../graphics/shapes.md).

It covers static backgrounds/sprites and animated sprite sheets, so one API handles both still and frame-based assets.

## Supported formats

| Format | Variants | Transparency |
|--------|----------|--------------|
| **PPM** (P6) | 24-bit RGB | Palette index 0 |
| **TGA** | Uncompressed, RLE, true-color (15/16/24/32bpp), color-mapped, grayscale | Alpha channel (< 128) |

Format is auto-detected from the file header.

## Conversion functions

| Function | Output | Use case |
|----------|--------|----------|
| `bitmap15` | Flat `gba::color` array | Mode 3 framebuffer |
| `indexed4` | 4bpp tiles + 16-color palette + tilemap | Backgrounds and sprites |
| `indexed8` | 8bpp tiles + 256-color palette + tilemap | High-color backgrounds |
| `indexed4_sheet<FrameW, FrameH>` | `sheet4_result` (frame-packed OBJ tiles + palette + flipbook helpers) | Animated 4bpp sprites |

All three take a supplier lambda that returns a `std::array<unsigned char, N>`.

`indexed4_sheet` uses the same supplier pattern, but interprets the source image as a grid of fixed-size sprite frames.

## Animated sprite sheets (`indexed4_sheet`)

Use `indexed4_sheet<FrameW, FrameH>` when one source image contains multiple animation frames. The converter validates frame dimensions at compile time, packs each frame contiguously for OBJ 1D mapping, and returns helpers for runtime frame selection.

```cpp
#include <gba/embed>

static constexpr auto actor = gba::embed::indexed4_sheet<16, 16>([] {
    return std::to_array<unsigned char>({
#embed "actor.tga"
    });
});

// 4-frame walk cycle: 0,1,2,1
static constexpr auto walk = actor.ping_pong<0, 3>();

// Upload actor.sprite once, then pick frame tile offset each tick
unsigned short base_tile = upload_to_obj_vram(actor.sprite);
unsigned int frame = walk.frame(tick / 8);
gba::obj_mem[0] = actor.frame_obj(base_tile, frame, 0);
```

### What `sheet4_result` gives you

- `palette`: one OBJ palette bank shared by all frames (index 0 remains transparent)
- `sprite`: frame-packed `sprite4` tile payload ready for VRAM upload
- `frame_obj(base, frame, pal)` / `frame_obj_aff(...)`: OAM helpers for regular and affine objects
- `tile_offset(frame)`: direct tile offset when you want to manage `tile_index` yourself
- compile-time flipbook builders: `forward<...>()`, `ping_pong<...>()`, `sequence<"...">()`, plus row-scoped `row<R>()`

### Row-based animation helpers

For multi-row sheets (for example, one direction per row), use `row<R>()` and build row-local animations:

```cpp
static constexpr auto down = actor.row<0>().ping_pong<0, 3>();
static constexpr auto left = actor.row<1>().ping_pong<0, 3>();
```

Frame indices from row helpers are sheet-global, so they work directly with `frame_obj()` and `tile_offset()`.

## Example: scrollable background with sprite

This demo embeds a 512x256 background image and a 16x16 character sprite, both as TGA files. The D-pad scrolls the background, and holding A + D-pad moves the sprite:

```cpp
{{#include ../../demos/demo_embed_sprite.cpp:4:}}
```

![Scrollable background with sprite](../img/embed_sprite.png)

### How it works

The background uses a 2x1 screenblock layout (`size = 1` in `reg_bgcnt`), giving 64x32 tiles (512x256 pixels). The `indexed4` map is stored in GBA screenblock order, so the entire map can be written to VRAM with a single `std::memcpy`.

The sprite uses `dedup::none` so its tiles are stored sequentially - exactly what the GBA expects for 1D-mapped sprites. Without this, tile deduplication could merge symmetric tiles and the sprite would render with missing pieces.

The sprite's transparent pixels (alpha < 128 in the TGA source) become palette index 0, which the hardware renders as see-through - the background shows through automatically.

### Constexpr evaluation limits

All image conversion happens at compile time. For large images, GCC may hit its default constexpr operation limit. If you see an error like `constexpr evaluation operation count exceeds limit`, increase the limit:

```cmake
target_compile_options(my_target PRIVATE -fconstexpr-ops-limit=335544320)
```

Small images (8x8 tiles, 16x16 sprites) work within the default limit. A 512x256 background like the one above requires the increased limit.

## Tile deduplication

The `indexed4` and `indexed8` converters accept a `dedup` mode as a template parameter:

| Mode | Behavior | Use case |
|------|----------|----------|
| `dedup::flip` (default) | Matches identity, horizontal flip, vertical flip, and both | Background tilemaps |
| `dedup::identity` | Matches exact duplicates only | Tilemaps without flip support |
| `dedup::none` | No deduplication - tiles stored sequentially | Sprites (1D mapping) |

```cpp
using gba::embed::dedup;

// Background: deduplicate with flip detection (default)
constexpr auto bg = gba::embed::indexed4(supplier);

// Sprite: sequential tiles for 1D object mapping
constexpr auto sprite = gba::embed::indexed4<dedup::none>(supplier);
```

When `dedup::flip` is active, matching tiles reuse existing indices with the appropriate `flip_horizontal` and `flip_vertical` flags set in the `screen_entry`. This can significantly reduce tile memory for images with symmetry.

With `dedup::none`, tiles are stored in the exact order they appear in the image. This is required for sprites, where the GBA reads tiles sequentially from the `tile_index` in 1D mapping mode.

## Sprite OAM attributes

When the image dimensions match a valid GBA sprite size, the result provides `obj()` and `obj_aff()` methods that return pre-filled OAM attributes with the correct shape, size, and color depth:

```cpp
constexpr auto sprite = gba::embed::indexed4<gba::embed::dedup::none>([] {
    return std::to_array<unsigned char>({
#embed "sprite.tga"
    });
});

// Regular object - shape, size, and depth are set automatically
gba::obj_mem[0] = sprite.obj(0);

// Affine object
gba::obj_aff_mem[0] = sprite.obj_aff(0);
```

The valid GBA sprite sizes are:

| Shape | Sizes |
|-------|-------|
| Square | 8x8, 16x16, 32x32, 64x64 |
| Wide | 16x8, 32x8, 32x16, 64x32 |
| Tall | 8x16, 8x32, 16x32, 32x64 |

If the image dimensions do not match any of these, `obj()` and `obj_aff()` will produce a compile-time error.

`indexed8` results use `depth_8bpp` and `indexed4` results use `depth_4bpp` automatically.

## Transparency

For **PPM** images, palette index 0 is always reserved as the transparent color. The first color found in the image becomes index 1.

For **TGA** images with an alpha channel (32bpp or 16bpp), pixels with alpha below 128 are mapped to palette index 0 (transparent). Fully opaque pixels are indexed normally.

## tonclib comparison

| stdgba | tonclib / grit |
|--------|----------------|
| `gba::embed::bitmap15(supplier)` | `grit image.png -gb -gB16 -ftc` |
| `gba::embed::indexed4(supplier)` | `grit image.png -gt -gB4 -mLs -ftc` |
| `gba::embed::indexed8(supplier)` | `grit image.png -gt -gB8 -mLs -ftc` |
| Compile-time C++ | External build tool |
| `#embed` or `std::to_array` | Generated `.c` / `.s` files |
