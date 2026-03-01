# Embedded Sprite Type Reference

The sprite structure returned by `gba::embed::indexed4()`, `gba::embed::indexed8()`, and `gba::embed::bitmap15()` holds OAM object helpers and raw tile data.

## Include

```cpp
#include <gba/embed>
```

## Sprite type summary

```cpp
template<std::size_t TileCount, std::size_t PaletteSize>
struct sprite {
    std::array<unsigned int, TileCount> tiles;
    std::array<gba::color, PaletteSize> palette;
    
    gba::object obj(unsigned short tile_index = 0) const;
    gba::object_affine obj_aff(unsigned short tile_index = 0) const;
};
```

## Members

- `tiles` - 4bpp or 8bpp tile payload ready for OBJ VRAM upload via `std::memcpy()`
- `palette` - palette entries for the sprite (16 colours for 4bpp, 256 for 8bpp)

## OAM helpers

### `obj(tile_index)`

Returns a regular (non-affine) `gba::object` entry pre-configured with:
- sprite dimensions from the source image
- tile index set to `tile_index` (default 0)
- 4bpp/8bpp depth matching the source
- all other fields zeroed (position, flip, palette bank)

```cpp
constexpr auto sprite = gba::embed::indexed4<gba::embed::dedup::none>([] {
    return std::to_array<unsigned char>({
#embed "hero.tga"
    });
});

gba::obj_mem[0] = sprite.obj(tile_base);
gba::obj_mem[0].x = 120;
gba::obj_mem[0].y = 80;
```

### `obj_aff(tile_index)`

Returns an affine `gba::object_affine` entry pre-configured the same way as `obj()`, but with:
- `affine` flag always set
- `affine_index` zeroed (assign your affine matrix index after)

```cpp
gba::obj_aff_mem[0] = sprite.obj_aff(tile_base);
gba::obj_aff_mem[0].affine_index = 0;
gba::obj_aff_mem[0].x = 120;
gba::obj_aff_mem[0].y = 80;
```

## Valid sprite sizes

The sprite type is only created when the source image dimensions match a legal GBA OBJ size:

| Shape | Sizes |
|-------|-------|
| Square | 8x8, 16x16, 32x32, 64x64 |
| Wide | 16x8, 32x8, 32x16, 64x32 |
| Tall | 8x16, 8x32, 16x32, 32x64 |

If the source does not match, the converter rejects it at compile time.

## Upload pattern

```cpp
// Copy tile data to OBJ VRAM
const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));
std::memcpy(gba::memory_map(gba::mem_vram_obj), sprite.tiles.data(), sprite.tiles.size() * sizeof(sprite.tiles[0]));

// Copy palette to OBJ palette RAM
std::copy(sprite.palette.begin(), sprite.palette.end(), gba::pal_obj_bank[0]);

// Create OAM entry
gba::obj_mem[0] = sprite.obj(base_tile);
```

## Related pages

- [Embedding Images](../graphics/embed.md)
- [Animated Sprite Sheets](../graphics/embed-animated.md)
- [`gba::object` Reference](./object.md)
- [`gba::object_affine` Reference](./object-affine.md)
