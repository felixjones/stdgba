# ECS Overview

`gba::ecs` is stdgba's static Entity-Component-System for fixed-capacity Game Boy Advance projects.

It exists for the same reason most of stdgba exists: many modern patterns are nice on desktop, but they only make sense on GBA if they can be made deterministic, fixed-size, and cheap to iterate.

## Why GBA needs a different ECS

Classic GBA games organise data in one of two ways:

1. **Array-per-concept**: `player_positions[]`, `player_velocities[]`, `enemy_states[]`, etc.
   - Fast to iterate
   - Easy to understand
   - Scales poorly (dozens of arrays become unwieldy)

2. **Object-heavy**: C++ objects with pointers holding player/enemy state
   - Natural to write
   - Introduces indirection and unpredictable memory access patterns
   - ARM7TDMI has no branch predictor; pointer chasing kills frame time

`gba::ecs` takes a third approach: **flat dense arrays organised by the ECS**, but with **compile-time component lists and shift-based addressing** tuned for GBA's constraints.

The result is data-oriented design without sacrificing readability.

## Core principles

`gba::ecs` is designed around:

- **zero heap allocation** -- all storage is stack-allocated or embedded in EWRAM/IWRAM structs
- **compile-time component lists** -- types are resolved at link-time, not runtime
- **predictable iteration costs** -- no sparse sets, no type-erased callbacks
- **flat dense storage** -- all-of-type component arrays in memory order
- **generation-based entity handles** -- 16-bit packed handles with stale-handle detection
- **power-of-two component sizes** -- enables shift-based pool addressing instead of multiplies
- **constexpr safety** -- invalid operations fail at compile time in constant-evaluation contexts

## The mental model

```text
entity_id     -> 16-bit handle (8-bit slot + 8-bit generation)
registry      -> owns all component arrays inline in EWRAM
group         -> compile-time logical grouping of components (zero runtime cost)
view<Cs...>   -> lightweight filtered iterator over entities matching all Cs
match<Cs...>  -> ordered per-entity conditional dispatch by component query cases
system        -> plain function operating on one or more views
```

Example: physics movement system

```cpp
void physics_system(world_type& world) {
	world.view<position, velocity>().each_arm([](position& pos, const velocity& vel) {
		pos.x += vel.vx;
		pos.y += vel.vy;
	});
}
```

Every ECS operation is deterministic and measurable -- no hidden allocation, no callback chains.

## Quick start

```cpp
#include <gba/ecs>

struct position { int x, y; };
struct velocity { int vx, vy; };
struct health   { int hp; };

using world_type = gba::ecs::registry<128, position, velocity, health>;

world_type world;

auto player = world.create();
world.emplace<position>(player, 10, 20);
world.emplace<velocity>(player, 1, 0);
world.emplace<health>(player, 100);

for (auto [pos, vel] : world.view<position, velocity>()) {
	pos.x += vel.vx;
	pos.y += vel.vy;
}
```

## Writing a system

The most important mental shift is that systems are just functions over views.

```cpp
#include <gba/ecs>
#include <gba/fixed_point>

struct position {
	gba::fixed<int, 8> x;
	gba::fixed<int, 8> y;
};

struct velocity {
	gba::fixed<int, 8> vx;
	gba::fixed<int, 8> vy;
};

struct health { int hp; };

struct sprite_id {
	std::uint8_t id;
	gba::ecs::pad<3> _;
};

using world_type = gba::ecs::registry<128, position, velocity, health, sprite_id>;

void movement_system(world_type& world) {
	world.view<position, velocity>().each_arm([](position& pos, const velocity& vel) {
		pos.x += vel.vx;
		pos.y += vel.vy;
	});
}

void damage_system(world_type& world) {
	world.view<health>().each([](health& hp) {
		if (hp.hp > 0) --hp.hp;
	});
}
```

Use `.each()` when you want the most portable, straightforward path. Use `.each_arm()` for hot loops that you have measured and want running from ARM mode + IWRAM.

## Complete API Reference

### Registry construction

```cpp
// Simple: list all components
using world = gba::ecs::registry<128, position, velocity, health>;

// With groups: organise components logically
using physics = gba::ecs::group<position, velocity, acceleration>;
using graphics = gba::ecs::group<sprite_id, palette_bank>;
using world = gba::ecs::registry<128, physics, graphics, health>;
```

Both are equivalent at runtime; groups flattened to individual components at compile time.

### Entity lifecycle

| Operation    | Signature               | Notes                                 |
| ------------ | ----------------------- | ------------------------------------- |
| `create()`   | -> `entity_id`          | Allocate a new entity slot            |
| `destroy(e)` | `(entity_id)` -> `void` | Destroy entity; increment generation  |
| `valid(e)`   | `(entity_id)` -> `bool` | Check if entity handle is still alive |
| `clear()`    | `()` -> `void`          | Destroy all entities at once          |
| `size()`     | `()` -> `std::size_t`   | Current count of alive entities       |

### Component operations

| Operation                  | Signature               | Notes                                            |
| -------------------------- | ----------------------- | ------------------------------------------------ |
| `emplace<C>(e, args...)`   | -> `C&`                 | Add component C to entity e; construct with args |
| `remove<C>(e)`             | `(entity_id)` -> `void` | Remove component C from entity e                 |
| `remove_unchecked<C>(ref)` | `(C&)` -> `void`        | Remove by component reference (faster)           |
| `get<C>(e)`                | `(entity_id)` -> `C&`   | Access component (unchecked)                     |
| `try_get<C>(e)`            | `(entity_id)` -> `C*`   | Access component (returns nullptr if absent)     |

### Queries and predicates

| Operation          | Signature               | Notes                                |
| ------------------ | ----------------------- | ------------------------------------ |
| `all_of<Cs...>(e)` | `(entity_id)` -> `bool` | Entity has **all** listed components |
| `any_of<Cs...>(e)` | `(entity_id)` -> `bool` | Entity has **any** listed component  |

### Iteration APIs

| API                           | Best for                                             |
| ----------------------------- | ---------------------------------------------------- |
| `view<Cs...>()` and range-for | Ergonomic gameplay systems with structured bindings  |
| `.each(fn)`                   | Portable systems; constexpr-friendly                 |
| `.each_arm(fn)`               | Measured hot loops requiring ARM mode + IWRAM        |
| `.each(entity_id, fn)`        | Systems that need the entity ID alongside components |

### Conditional dispatch APIs

| API                                     | Best for                                                               |
| --------------------------------------- | ---------------------------------------------------------------------- |
| `with<Query...>(e, fn)`                 | Single guarded callback when all queried components are present        |
| `match<Cases...>(e, fn1, fn2, ...)`     | Ordered multi-case dispatch for one entity; all matched cases run      |
| `match_arm<Cases...>(e, fn1, fn2, ...)` | ARM/IWRAM hot-path version of `match(...)` for measured dispatch loops |

`match(...)` snapshots case matches before callbacks run, then executes matched cases in the order declared.

```cpp
// Range-for with structured bindings
for (auto [pos, vel] : world.view<position, velocity>()) {
	pos.x += vel.vx;
}

// Callback style
world.view<position, velocity>().each([](position& pos, velocity& vel) {
	pos.x += vel.vx;
});

// With entity ID
world.view<health>().each([](gba::ecs::entity_id id, health& hp) {
	if (hp.hp <= 0) world.destroy(id);
});

// ARM-mode hot loop
world.view<position, velocity>().each_arm([](position& pos, velocity& vel) {
	pos.x += vel.vx;  // Runs from ARM mode + IWRAM
});
```

### `match(...)` example

```cpp
using physics = gba::ecs::group<position, velocity>;

world.match<physics, health>(player,
	[](position& pos, velocity& vel) {
		pos.x += vel.vx;
		pos.y += vel.vy;
	},
	[](health& hp) {
		if (hp.hp > 0) --hp.hp;
	}
);
```

For an entity that has both `physics` and `health`, both callbacks run in order. For an entity that only has one case, only that callback runs. The return value is `true` if at least one case matched.

## Why the component list is compile-time

`gba::ecs` asks you to name every component type up front:

```cpp
using world_type = gba::ecs::registry<128, position, velocity, health>;
```

That buys the implementation several things:

- no runtime type registry
- no sparse-set hash maps
- direct type-to-bit and type-to-pool lookup
- compile-time diagnostics when you request a component the world does not own

It is a strong fit for GBA projects, where the total set of gameplay component types is usually small and stable.

## Power-of-two component sizes

Each component type must have a power-of-two `sizeof(T)`.

```cpp
struct sprite_id {
	std::uint8_t id;
	gba::ecs::pad<3> _;
};

static_assert(sizeof(sprite_id) == 4);
```

This is not just a style rule - it supports the simple shift-based pool addressing the implementation is built around.

## Constexpr-friendly behaviour

All core registry operations are `constexpr`. In constant-evaluation contexts, invalid operations produce compile-time failures instead of silent bad state.

```cpp
static constexpr auto result = [] {
	gba::ecs::registry<8, int, short> reg;
	auto e = reg.create();
	reg.emplace<int>(e, 42);
	reg.emplace<short>(e, short{7});
	return reg.get<int>(e) * 100 + reg.get<short>(e);
}();

static_assert(result == 4207);
```

## Memory consumption in EWRAM

Registry memory is all inline -- no heap allocation or indirection. For a typical game setup:

```cpp
gba::ecs::registry<128, position, velocity, health> world;
```

| Category            | Size         | Notes                            |
| ------------------- | ------------ | -------------------------------- |
| **Metadata**        | ~900 bytes   | Per-entity tracking + free stack |
| **Component pools** | ~2,560 bytes | 128 × (8 + 8 + 4) bytes          |
| **Total**           | ~3.5 KB      | ~26% overhead, 74% actual data   |

**Key insight**: Metadata grows **linearly per entity slot** (7 bytes/slot) regardless of component count. Adding more components adds component-pool storage, not metadata overhead.

### Scaling examples

- **64 entities, 3 components**: ~1.7 KB
- **128 entities, 3 components**: ~3.5 KB (typical action game)
- **256 entities, 6 components**: ~8.8 KB (large world)

For context: GBA has 256 KB EWRAM and 32 KB IWRAM. A 128-entity registry uses ~1.4% of EWRAM, leaving room for graphics buffers, tilemaps, and multiple registries if needed.

### Optimising EWRAM usage

If registry memory is tight:

1. **Reduce capacity**: Each entity slot = 7 bytes overhead
   - 64 entities instead of 128 saves 448 bytes metadata

2. **Combine sparse components**: If only 10% of entities need a component, you still allocate space for 100%
   - Consider whether to split into separate registries

3. **Careful padding**: Power-of-two sizes are required but not wasteful
   - 1-byte component -> 1 byte (pad to 1, not 4)
   - 3-byte component -> needs padding to 4

## Why ECS benefits GBA game architecture

### Predictable memory access patterns

Arrays-of-components means systems iterate only the memory regions they need, reducing bus traffic:

```
View iteration over position + velocity:
  Read sequential position array
  Read sequential velocity array
  
  vs

Array-of-structs (without ECS):
  Read interleaved position/velocity/health data
  Fetch unused health values into memory bus
```

Without ECS, every sprite iteration would pull extra data into the memory bus even if only position is needed. Arrays keep access patterns linear and predictable.

### No hidden allocations during gameplay

- Registry is pre-allocated at startup
- All memory lives in EWRAM or IWRAM
- Zero dynamic allocation in the game loop
- Deterministic frame time (no GC pauses, no allocation failures)

### Flexible game architecture

- Physics system operates on `<position, velocity>`
- Rendering system operates on `<sprite_id, depth>`
- Destruction system operates on `<health>` (with entity IDs)

Each system only touches the data it needs, keeping working set small and predictable on GBA's 32 KB IWRAM.

### Small learning curve

If you know how to write `for (auto& entity : entities)`, you can write an ECS system. The mental model is straightforward: **views are filtered arrays, systems operate on views**.

## Where to go next

- [ECS Architecture](./architecture.md) explains the data layout, memory model, and iteration strategies.
- [Internal Implementation](./internal-implementation.md) covers the metadata arrays, fast-path selection, and why power-of-two sizes matter.
- [`tests/ecs/test_ecs.cpp`](https://github.com/felixjones/stdgba/tree/main/tests/ecs) -- comprehensive runtime examples of all APIs.
