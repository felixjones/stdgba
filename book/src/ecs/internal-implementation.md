# Internal Implementation

This page covers the mechanics behind `gba::ecs`: how entities are recycled, why metadata is ordered the way it is, and how the iteration fast paths are selected.

## Field ordering inside `registry`

`registry.hpp` places small hot metadata first and large pools later:

| Order | Field | Why it is near the front |
|-------|-------|--------------------------|
| 1 | `m_component_count[]` | touched by view setup and component attach/remove |
| 2 | `m_free_top` | touched by `create()` and `destroy()` |
| 3 | `m_next_slot` | touched by `create()` |
| 4 | `m_alive` | touched by create/destroy/view setup |
| 5+ | masks, generations, stacks, alive lists | still hot, but larger |
| last | component pools | large bulk storage; offset cost matters less |

The comment in `registry.hpp` explains the main codegen reason: in Thumb-mode call paths such as `create()`, `destroy()`, and `emplace()`, low offsets make for cheaper loads and stores.

## How entity creation works

Creation prefers recycled slots, then falls back to a never-used slot.

```text
if free stack not empty:
	pop slot from m_free_stack
else:
	use m_next_slot and increment it

mark slot alive
append slot to m_alive_list
record reverse index in m_alive_index
increment m_alive
return entity_id(slot, generation)
```

That makes slot reuse deterministic and cheap.

## How destruction works

Destroying an entity performs four distinct jobs:

1. decrement component counts for every component present on that slot
2. clear the mask and increment the generation
3. push the slot onto `m_free_stack`
4. remove the slot from `m_alive_list` with swap-and-pop

The important bit is swap-and-pop:

```text
alive list before: [ 4, 7, 2, 9 ]
destroy slot 7
swap in last slot 9
alive list after:  [ 4, 9, 2 ]
```

That keeps removal O(1) instead of shifting a long list.

## Why there is both `m_alive_list` and `m_alive_index`

| Field | Role |
|-------|------|
| `m_alive_list[Capacity]` | dense list of alive slots in iteration order |
| `m_alive_index[Capacity]` | reverse map from slot -> index in `m_alive_list` |

You need both to delete from the dense list in O(1). Without the reverse map, destruction would have to scan the list to find the removed slot.

## Component count tracking and fast-path selection

`m_component_count[]` stores how many alive entities currently own each component type.

Before iterating, a view checks whether every requested component count equals `m_alive`.

If true, then every alive entity has every requested component, and the loop can skip per-entity mask checks.

That is the basis of the three iteration paths:

| Path | Condition | Inner-loop work |
|------|-----------|-----------------|
| Dense + all-match | `m_alive == m_next_slot` and all requested component counts equal `m_alive` | direct slot walk |
| All-match with gaps | all requested component counts equal `m_alive`, but dense-slot condition is false | walk `m_alive_list` only |
| Mixed | not all alive entities have the requested components | walk `m_alive_list` and test mask |

This is a simple but effective optimisation. Many game systems operate on worlds where almost every live entity in a layer shares the same core components.

## Iterator vs callback style

Both range-for and `.each()` are implemented on top of the same storage model, but they serve slightly different goals:

| Style | Best trait |
|-------|------------|
| range-for | ergonomic syntax with structured bindings |
| `.each()` | explicit callback, easy to specialize or switch to `.each_arm()` |
| `.each_arm()` | hottest runtime path |

The callback path also auto-detects whether your lambda wants an `entity_id` first:

```cpp
world.view<health>().each([](gba::ecs::entity_id e, health& hp) {
	// id-aware system
});
```

## `each_arm()` and why it exists

`basic_view::each_arm()` is annotated to build for ARM mode and live in IWRAM:

- `gnu::target("arm")`
- `gnu::section(".iwram._gba_ecs_each")`
- `gnu::flatten`

That combination is intended for the loops you run every frame on hardware.

### Why it can be faster

| Choice | Benefit |
|--------|---------|
| ARM mode | more registers and richer addressing modes than Thumb |
| IWRAM placement | faster instruction fetch on target hardware |
| flattened callback body | better inlining in tight loops |

In the benchmark suite, this is the path used for runtime movement and full-update loops.

## Compile-time safety behaviour

The registry uses `if consteval` checks for invalid operations such as:

- capacity overflow in `create()`
- destroying an invalid entity
- double-emplacing the same component
- removing from an invalid entity

That means a misuse inside a `static constexpr` setup produces a compiler error instead of a bad runtime state.

## The power-of-two size rule, internally

The registry enforces this with:

```cpp
static_assert(((std::has_single_bit(sizeof(Components))) && ...),
			  "all component sizes must be powers of two");
```

It is not just stylistic. The implementation is tuned around simple addressing and predictable pool layout. If you have a 3-byte or 12-byte component, pad it to 4 or 16 bytes.

```cpp
struct sprite_id {
	std::uint8_t id;
	gba::ecs::pad<3> _;
};
```

## A concrete storage example

For this registry:

```cpp
using world_type = gba::ecs::registry<128, position, velocity, health, sprite_id>;
```

with sizes from the benchmark:

| Component | Size | Pool storage |
|-----------|------|--------------|
| `position` | 8 | `128 * 8 = 1024` bytes |
| `velocity` | 8 | `128 * 8 = 1024` bytes |
| `health` | 4 | `128 * 4 = 512` bytes |
| `sprite_id` | 1 | `128 * 1 = 128` bytes |

Logical payload per entity is 21 bytes, but physical storage is split into four arrays plus the metadata arrays.

That split is what makes selective views fast.

## Related tests and benchmarks

The implementation is best understood alongside these files:

- public API: `include/gba/ecs`
- implementation: `include/gba/bits/ecs/registry.hpp`
- entity ID helpers: `include/gba/bits/ecs/entity.hpp`
- tests: `tests/ecs/test_ecs.cpp`
- runtime benchmark: `benchmarks/bench_ecs.cpp`
- debug benchmark: `benchmarks/bench_ecs_debug.cpp`

The tests exercise lifecycle, generation invalidation, view filtering, structured bindings, constexpr use, and padding rules. The benchmarks show why the implementation keeps leaning so hard into dense arrays and low-overhead iteration.
