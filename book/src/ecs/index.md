# ECS Overview

`gba::ecs` is stdgba's static Entity-Component-System for fixed-capacity Game Boy Advance projects.

It is designed for:

- zero heap allocation
- compile-time component lists
- predictable iteration costs
- constexpr-friendly validation in constant-evaluation contexts

## Quick start

```cpp
#include <gba/ecs>

struct position { int x, y; };
struct velocity { int vx, vy; };

gba::ecs::registry<128, position, velocity> world;

auto e = world.create();
world.emplace<position>(e, 10, 20);
world.emplace<velocity>(e, 1, 0);

for (auto [pos, vel] : world.view<position, velocity>()) {
	pos.x += vel.vx;
	pos.y += vel.vy;
}
```

## Core API

- `registry<Capacity, Components...>`: fixed-capacity ECS world
- lifecycle: `create()`, `destroy()`, `valid()`, `clear()`, `size()`
- component ops: `emplace<C>()`, `remove<C>()`, `get<C>()`
- queries: `all_of<Cs...>()`, `any_of<Cs...>()`
- iteration: `view<Cs...>()`, range-for, `.each(...)`, `.each_arm(...)`

For deeper details, continue to [ECS Architecture](./architecture.md) and [Internal Implementation](./internal-implementation.md).
