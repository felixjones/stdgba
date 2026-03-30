# Animated Sprite Sheets

`gba::embed::indexed4_sheet<FrameW, FrameH>()` turns one sprite-sheet image into frame-packed OBJ tile data at compile time. It is the animation-oriented sibling to [Embedding Images](./embed.md): same file formats, same supplier-lambda pattern, but a different output shape tuned for OBJ 1D mapping.

For procedural sprite generation without source image files, see [Shapes](../utilities/shapes.md).
For type-level API details, see [Animated Sprite Sheet Type Reference](../reference/embed-sheet.md).

## When to use `indexed4_sheet`

Use `indexed4_sheet` when:

- one source image contains multiple animation frames
- every frame has the same width and height
- you want each frame's tiles laid out contiguously in OBJ VRAM
- you want compile-time flipbook helpers instead of manual tile math

Use plain `indexed4<dedup::none>()` when you only need one static sprite frame.

## Quick start

```cpp
#include <cstring>
#include <gba/embed>
#include <gba/video>

static constexpr auto actor = gba::embed::indexed4_sheet<16, 16>([] {
	return std::to_array<unsigned char>({
#embed "actor.tga"
	});
});

static constexpr auto walk = actor.ping_pong<0, 3>();

const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));
std::memcpy(gba::memory_map(gba::mem_vram_obj), actor.sprite.data(), actor.sprite.size());

unsigned int frame = walk.frame(tick / 8);

gba::obj_mem[0] = actor.frame_obj(base_tile, frame, 0);
```

The converter validates at compile time that:

- the full image width is a multiple of `FrameW`
- the full image height is a multiple of `FrameH`
- `FrameW` x `FrameH` is a valid GBA OBJ size
- the whole sheet fits a single 15-colour palette plus transparent index 0

## What `sheet4_result` gives you

| Member / helper | Purpose |
|-----------------|---------|
| `palette` | Shared OBJ palette bank for every frame |
| `sprite` | Frame-packed 4bpp tile payload ready for OBJ VRAM upload |
| `tile_offset(frame)` | Tile offset for a frame, useful with manual `tile_index` management |
| `frame_obj(base, frame, pal)` | Regular OAM helper for one frame |
| `frame_obj_aff(base, frame, pal)` | Affine OAM helper for one frame |
| `forward<Start, Count>()` | Compile-time sequential flipbook |
| `ping_pong<Start, Count>()` | Compile-time forward-then-reverse flipbook |
| `sequence<"...">()` | Explicit frame order via string literal |
| `row<R>()` | Row-scoped flipbook builder for multi-row sheets |

## How frames are laid out

The important difference from plain `indexed4()` is tile order. `indexed4_sheet()` repacks tiles frame-by-frame so the GBA can step through animation frames with simple tile offsets.

```text
Source sheet (2 rows x 4 columns, 16x16 frames)

+----+----+----+----+
| f0 | f1 | f2 | f3 |
+----+----+----+----+
| f4 | f5 | f6 | f7 |
+----+----+----+----+

OBJ tile payload emitted by indexed4_sheet

[f0 tiles][f1 tiles][f2 tiles][f3 tiles][f4 tiles][f5 tiles][f6 tiles][f7 tiles]
```

That means `tile_offset(frame)` is simply:

```text
frame * tiles_per_frame
```

No runtime repacking step is needed.

## Flipbook builders

### Sequential animation

```cpp
static constexpr auto idle = actor.forward<0, 4>();
```

Frames: `0, 1, 2, 3`

### Ping-pong animation

```cpp
static constexpr auto walk = actor.ping_pong<0, 4>();
```

Frames: `0, 1, 2, 3, 2, 1`

### Explicit frame order

```cpp
static constexpr auto attack = actor.sequence<"01232100">();
```

Each character selects a frame index. `0`-`9` map to frames 0-9, `a`-`z` continue from 10 upward.

## Row-based sheets

For RPG Maker style character sheets with one direction per row, use `row<R>()` to scope animations to a single row.

```cpp
static constexpr auto actor = gba::embed::indexed4_sheet<16, 16>([] {
	return std::to_array<unsigned char>({
#embed "hero_walk.tga"
	});
});

static constexpr auto down  = actor.row<0>().ping_pong<0, 3>();
static constexpr auto left  = actor.row<1>().ping_pong<0, 3>();
static constexpr auto right = actor.row<2>().ping_pong<0, 3>();
static constexpr auto up    = actor.row<3>().ping_pong<0, 3>();
```

Row helpers still produce sheet-global frame indices, so the result plugs directly into `frame_obj()` and `tile_offset()`.

## A practical render loop

```cpp
#include <algorithm>
#include <cstring>
#include <gba/bios>
#include <gba/embed>
#include <gba/video>

static constexpr auto actor = gba::embed::indexed4_sheet<16, 16>([] {
	return std::to_array<unsigned char>({
#embed "actor.tga"
	});
});

static constexpr auto walk = actor.ping_pong<0, 4>();

int main() {
	gba::reg_dispcnt = {
		.video_mode = 0,
		.linear_obj_tilemap = true,
		.enable_obj = true,
	};

	std::copy(actor.palette.begin(), actor.palette.end(), gba::pal_obj_bank[0]);
	std::memcpy(gba::memory_map(gba::mem_vram_obj), actor.sprite.data(), actor.sprite.size());

	unsigned int tick = 0;
	const auto base_tile = gba::tile_index(gba::memory_map(gba::mem_vram_obj));

	while (true) {
		gba::VBlankIntrWait();
		const unsigned int frame = walk.frame(tick / 8);
		auto obj = actor.frame_obj(base_tile, frame, 0);
		obj.x = 112;
		obj.y = 72;
		gba::obj_mem[0] = obj;
		++tick;
	}
}
```

## Palette and colour limits

`indexed4_sheet` builds one shared 16-entry OBJ palette:

- palette index 0 stays transparent
- the whole sheet may use at most 15 opaque colours total
- unlike background-oriented `indexed4()`, sheet conversion does not spread tiles across multiple palette banks

That trade-off keeps every frame interchangeable at one base tile and one OBJ palette bank.

## Compile-time failure modes

Typical compile-time diagnostics are:

- frame width or height not divisible into the source image
- source image not aligned to 8x8 tile boundaries
- frame dimensions not matching a legal OBJ size
- more than 15 opaque colours across the whole sheet
- invalid frame index in `forward`, `ping_pong`, `sequence`, or `row`

## Choosing between the asset paths

| Workflow | Best for |
|----------|----------|
| [Shapes](../utilities/shapes.md) | Simple geometric sprites, HUD markers, debug art, zero external assets |
| [Embedding Images](./embed.md) | Static backgrounds, portraits, logos, and one-frame sprites |
| `indexed4_sheet()` | Animated sprite sheets with compile-time frame selection |
