# Internal Implementation

This page documents the internal mechanics that make `gba::ecs` predictable on GBA hardware.

## Dense alive-list and free-slot reuse

- `m_alive_list[Capacity]` stores active slot indices densely.
- `m_alive_index[Capacity]` maps slot -> position in `m_alive_list`.
- `destroy()` uses swap-and-pop to remove a slot in O(1).
- `create()` reuses destroyed slots from a LIFO free stack (`m_free_stack`).

Result: iteration cost scales with alive entities, not maximum historical slot.

## Per-component count tracking

- `m_component_count[...]` tracks how many alive entities currently own each component type.
- Before looping, views compare these counts to `m_alive`.
- If all requested component counts equal `m_alive`, mask checks are skipped entirely.

This recovers near-dense iteration speed for uniform archetypes.

## Metadata-first struct layout

Hot metadata fields are intentionally placed at low offsets in the registry object:

- `m_component_count`
- `m_free_top`
- `m_next_slot`
- `m_alive`

In Thumb-mode call paths (`create`, `destroy`, `emplace`), this improves access codegen by enabling compact immediate-offset loads/stores.

## `each_arm()` hot-path walker

`basic_view::each_arm()` is compiled for ARM mode and placed in IWRAM. This is the runtime fast path for heavy frame loops:

- ARM mode: larger register set and richer addressing modes
- IWRAM placement: faster instruction fetch on target hardware
- callback body inlining: reduced call overhead in tight loops

Use `.each()` when portability/constexpr behavior matters more; use `.each_arm()` for measured hot loops.

## Compile-time safety behavior

In constant-evaluation contexts, invalid ECS operations fail at compile time (for example capacity overflow, stale entity use, or double emplace).

This keeps misuses visible early without runtime assertion overhead in release builds.

## Power-of-two component size rule

Component types must have power-of-two `sizeof(C)`. This is validated with `static_assert` and supports efficient addressing in pools.

If a component needs padding for alignment/size policy, use `gba::ecs::pad<N>`.

## Related files

- public API: `include/gba/ecs`
- implementation: `include/gba/bits/ecs/registry.hpp`
- entity ID helpers: `include/gba/bits/ecs/entity.hpp`
- tests: `tests/ecs/test_ecs.cpp`
- benchmarks: `benchmarks/bench_ecs.cpp`
