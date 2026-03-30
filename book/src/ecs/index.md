# ECS Overview

`gba::ecs` is stdgba's static Entity-Component-System for fixed-capacity Game Boy Advance projects.

It exists for the same reason most of stdgba exists: many modern patterns are nice on desktop, but they only make sense on GBA if they can be made deterministic, fixed-size, and cheap to iterate.

`gba::ecs` is designed around:

- zero heap allocation
- compile-time component lists
- predictable iteration costs
- constexpr-friendly validation in constant-evaluation contexts
- storage layouts that map cleanly onto ARM7TDMI strengths

## Why stdgba includes an ECS at all

Classic GBA codebases often fall into one of two camps:

1. hand-written arrays for every gameplay concept
2. object-heavy designs that feel natural in C++ but end up paying in indirection and lifetime complexity

`gba::ecs` aims for a middle ground.

It gives you:

- one fixed-capacity world object
- explicit component lists at compile time
- simple entity handles with stale-handle detection
- direct iteration over the data you care about

It does **not** try to be a full engine runtime with dynamic type registration, archetype migration, scheduler graphs, reflection, or editor tooling. The goal is a small, readable ECS that fits a handheld game, not a giant framework.

## What the model looks like

```text
entity_id -> slot + generation
registry  -> owns all component arrays inline
view<Cs>  -> iterates alive entities that have every listed component
system    -> plain function that operates on one or more views
```

That means a system is usually just a free function:

```cpp
void movement_system(world_type& world) {
	world.view<position, velocity>().each([](position& pos, const velocity& vel) {
		pos.x += vel.vx;
		pos.y += vel.vy;
	});
}
```

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

## Core API

| Area | API |
|------|-----|
| Registry type | `registry<Capacity, Components...>` |
| Lifecycle | `create()`, `destroy()`, `valid()`, `clear()`, `size()` |
| Component ops | `emplace<C>()`, `remove<C>()`, `get<C>()` |
| Queries | `all_of<Cs...>()`, `any_of<Cs...>()` |
| Iteration | `view<Cs...>()`, range-for, `.each(...)`, `.each_arm(...)` |

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

## Where to go next

- [ECS Architecture](./architecture.md) explains the data layout and iteration model.
- [Internal Implementation](./internal-implementation.md) covers the metadata arrays, fast paths, and codegen-oriented design choices.
