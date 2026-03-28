# ECS Architecture

`gba::ecs` uses a static, flat-storage architecture tuned for ARM7TDMI constraints.

## High-level layout

```text
include/gba/bits/ecs/
  entity.hpp    -> entity_id (slot + generation), null sentinel, pad<N>
  registry.hpp  -> registry<Capacity, Components...> + basic_view<ViewCs...>
include/gba/ecs -> public facade
```

## Storage model

- Each registry has fixed `Capacity` and a compile-time component list.
- Component pools are dense `std::array<C, Capacity>` blocks inside the registry.
- No dynamic allocation, sparse sets, or runtime type-erasure maps.
- Component lookup is direct by slot index.

## Entity identity

- `entity_id` is a 16-bit value: low byte slot, high byte generation.
- Generation invalidates stale handles after `destroy()`.
- Max entity slots per registry is 255.

## Presence tracking

- Per-slot `uint32_t` mask:
  - bit 31: alive flag
  - bits 0-30: component presence bits
- `all_of` / `any_of` and view filtering are bitmask operations.

## View dispatch strategy

Views (`view<Cs...>`) choose among three runtime paths before iterating:

1. dense + all-match: no mask check, no alive-list indirection
2. all-match with gaps: alive-list indirection, no mask check
3. mixed: alive-list + per-entity mask check

This keeps common uniform-archetype loops fast while still handling sparse worlds efficiently.

## Iteration APIs

- Range-for iteration (`for (auto [a, b] : view)`) is the primary ergonomic path.
- `.each(fn)` is portable and constexpr-friendly.
- `.each_arm(fn)` places the hot walker in ARM mode + IWRAM for runtime-critical loops.

See [Internal Implementation](./internal-implementation.md) for metadata layout, alive-list details, and optimization rationale.
