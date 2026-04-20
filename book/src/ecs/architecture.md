# ECS Architecture

`gba::ecs` uses a static, flat-storage architecture tuned for ARM7TDMI constraints. The design goal is straightforward: make the common operations for a small fixed-capacity game world cheap enough that you can reason about them without a profiler open all day.

## File layout and public interface

```text
include/gba/ecs              -> public facade
  +- registry<Capacity, Components...>
  +- group<Components...>
  +- entity (handle with generation)
  +- pad<N> (padding utility)

include/gba/bits/ecs/        -> internal implementation
  +- entity.hpp
  +- group.hpp
  +- group_metadata.hpp
  +- registry.hpp
```

## Why this ECS is static

Many desktop ECS libraries optimise for:

- unlimited entity counts
- runtime component registration
- dynamic archetype churn
- scheduler/tooling integration

`gba::ecs` optimises for something entirely different:

- a known maximum entity count (fits in 8 bits; max 255 entities)
- a small compile-time component set (max 31 components)
- simple arrays that can live inline inside one registry object
- predictable loops for handheld game logic

That is why the registry type specifies everything at compile time:

```cpp
using world_type = gba::ecs::registry<128, position, velocity, health>;
```

The type itself answers the architectural questions: maximum 128 live entities, exactly three component pools.

## Registry storage model

Every registry owns its storage inline -- no heap allocation, no indirection.

```text
registry<Capacity, Components...>
|
+- hot metadata (cached in Thumb mode)
|  +- m_component_count[N]      (1 byte/component)
|  +- m_free_top                (1 byte)
|  +- m_next_slot               (1 byte)
|  +- m_alive                   (1 byte)
|  +- m_dense_prefix            (1 byte)
|
+- per-slot tracking
|  +- m_mask[Capacity]          (4 bytes/slot)
|  +- m_gen[Capacity]           (1 byte/slot)
|  +- m_free_stack[Capacity]    (1 byte/slot)
|  +- m_alive_list[Capacity]    (1 byte/slot)
|  +- m_alive_index[Capacity]   (1 byte/slot)
|
+- component pools
   +- std::array<C1, Capacity>  (Capacity x sizeof(C1))
   +- std::array<C2, Capacity>  (Capacity x sizeof(C2))
   +- ...
```

No heap allocation, sparse sets, or type-erased component maps are involved.

## Memory consumption breakdown

For `gba::ecs::registry<128, position, velocity, health>`:

| Item                        | Size       | Notes                             |
| --------------------------- | ---------- | --------------------------------- |
| **Metadata overhead**       |            |                                   |
| Hot scalars (5 bytes)       | 5 B        |                                   |
| Per-slot tracking (7 × 128) | 896 B      | m_mask + m_gen + stacks + indices |
| Per-component count (3)     | 3 B        |                                   |
| **Metadata subtotal**       | **904 B**  | (26% of total)                    |
| **Component pools**         |            |                                   |
| position (8 × 128)          | 1024 B     |                                   |
| velocity (8 × 128)          | 1024 B     |                                   |
| health (4 × 128)            | 512 B      |                                   |
| **Data subtotal**           | **2560 B** | (74% of total)                    |
| **Total**                   | **3464 B** | (~3.4 KB)                         |

### General formula

For a registry with **Capacity** slots and **N** components:

```
Metadata = Capacity × 7 + N + 5

Component data = Capacity × Σ(sizeof(Component))

Total = Metadata + Component data
```

### Scaling characteristics

Metadata grows **linearly per slot** (7 bytes) but is **independent of component count**. Adding more components only adds to the pool size, not metadata.

| Config                     | Metadata | Data   | Total  | % Overhead |
| -------------------------- | -------- | ------ | ------ | ---------- |
| 64 entities, 3 components  | 453 B    | 1280 B | 1733 B | 26%        |
| 128 entities, 3 components | 904 B    | 2560 B | 3464 B | 26%        |
| 256 entities, 3 components | 1803 B   | 5120 B | 6923 B | 26%        |
| 128 entities, 6 components | 904 B    | 4608 B | 5512 B | 16%        |

Larger registries and more components both **reduce metadata percentage**, making large game worlds more efficient.

## Component groups and logical organisation

Component groups provide compile-time organisation without runtime overhead.

```cpp
// Define conceptual groups
using physics = gba::ecs::group<position, velocity, acceleration>;
using rendering = gba::ecs::group<sprite_id, palette_bank, x_offset>;

// Use groups in registry declaration
gba::ecs::registry<128, physics, rendering, health> world;

// Internally flattened to:
// gba::ecs::registry<128, position, velocity, acceleration,
//                        sprite_id, palette_bank, x_offset, health>
```

Groups are **completely erased at compile time**. They exist for code organisation and readability, not runtime behaviour.

### Why groups matter

- **Logical namespace**: Physics components stay together in the code
- **No runtime cost**: Groups are pure templates; zero overhead
- **No ambiguity**: The registry type fully specifies what exists
- **Iterating unchanged**: Use `view<position, velocity>()` regardless of groups

```cpp
// All of these work the same way:
world.view<position, velocity>().each([](position& p, velocity& v) {
	p.x += v.vx;
});
```

## Entity identity

`entity` is a 16-bit handle:

| Bits        | Meaning            |
| ----------- | ------------------ |
| low 8 bits  | slot index         |
| high 8 bits | generation counter |

```text
15            8 7             0
+---------------+---------------+
| generation | slot |
+---------------+---------------+
```

Consequences:

- maximum slots per registry: 255
- `0xFFFF` is reserved for `gba::entity_null`
- stale handles become invalid after `destroy()` increments generation

This is a very good match for GBA games, where worlds are usually dozens or low hundreds of entities, not tens of thousands.

## Presence tracking with one mask per slot

Each slot has one `std::uint32_t` mask:

- bit 31 = entity alive flag
- bits 0-30 = component presence bits

That supports cheap queries:

- `all_of<Cs...>()` -> bitwise AND against a compile-time mask
- `any_of<Cs...>()` -> bitwise AND against a compile-time mask
- view filtering -> compare the slot mask with a required mask

## Logical per-entity layout vs physical storage

One of the easiest ways to misunderstand the registry is to imagine each entity as one packed struct. That is **not** what happens.

### Logical view

For example, with these components:

| Component   | Size    |
| ----------- | ------- |
| `position`  | 8 bytes |
| `velocity`  | 8 bytes |
| `health`    | 4 bytes |
| `sprite_id` | 1 byte  |

The logical entity data is 21 bytes of component payload.

### Physical view

The registry stores them as separate arrays:

```text
position pool: [p0][p1][p2][p3] ...
velocity pool: [v0][v1][v2][v3] ...
health pool:   [h0][h1][h2][h3] ...
sprite pool:   [s0][s1][s2][s3] ...
```

That is why a `view<position, velocity>()` can iterate directly over only the pools it needs.

## Metadata arrays and what they buy you

| Field                 | Role                                             |
| --------------------- | ------------------------------------------------ |
| `m_component_count[]` | Count of alive entities owning each component    |
| `m_free_top`          | Size of the free-slot stack                      |
| `m_next_slot`         | Next never-before-used slot                      |
| `m_alive`             | Current alive entity count                       |
| `m_mask[]`            | Alive + component presence bits                  |
| `m_gen[]`             | Per-slot generation counters                     |
| `m_free_stack[]`      | Recycled slot stack                              |
| `m_alive_list[]`      | Dense list of alive slots                        |
| `m_alive_index[]`     | Reverse map for O(1) removal from `m_alive_list` |

This is the backbone of the ECS. The component pools are simple; the metadata is what makes creation, destruction, and iteration cheap.

## View dispatch strategy

`view<Cs...>()` does not use one always-generic loop. It picks among three runtime paths:

| Path                | Condition                                                                                     | Cost profile                               |
| ------------------- | --------------------------------------------------------------------------------------------- | ------------------------------------------ |
| Dense + all-match   | every alive entity has every requested component, and alive slots are still dense from 0..N-1 | no alive-list lookup, no mask check        |
| All-match with gaps | every alive entity has every requested component, but slots are no longer dense               | alive-list lookup, no mask check           |
| Mixed               | some alive entities are missing requested components                                          | alive-list lookup plus per-slot mask check |

This matters because many gameplay worlds spend most of their time in one of the first two cases.

## Iteration styles

| API                            | Best for                                                    |
| ------------------------------ | ----------------------------------------------------------- |
| range-for over `view<Cs...>()` | ergonomic gameplay code                                     |
| `.each(fn)`                    | explicit callback style, constexpr-friendly code            |
| `.each_arm(fn)`                | measured hot loops where ARM-mode + IWRAM placement matters |

Example:

```cpp
world.view<position, velocity>().each([](position& pos, const velocity& vel) {
    pos.x += vel.vx;
    pos.y += vel.vy;
});
```

## Power-of-two component sizes

Every component type must have a power-of-two `sizeof(T)`.

| Size            | Allowed? |
| --------------- | -------- |
| 1               | yes      |
| 2               | yes      |
| 4               | yes      |
| 8               | yes      |
| 3, 5, 6, 7, ... | no       |

If a type is almost right, pad it:

```cpp
struct sprite_id {
    std::uint8_t id;
    gba::ecs::pad<3> _;
};
```

This rule exists to support cheap shift-based addressing in the component pools.

## What the architecture intentionally omits

To stay small and predictable, `gba::ecs` deliberately does not include:

- runtime component registration
- dynamic archetype storage
- event buses or schedulers
- system graphs or task runners
- serialisation or reflection

The expectation is that you compose those policies at a higher layer if your project needs them.

See [Internal Implementation](./internal-implementation.md) for the field ordering, alive-list mechanics, and the fast-path details that fall out of this architecture.
