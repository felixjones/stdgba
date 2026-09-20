# `gba::critical_section` Reference

`gba::critical_section` is a non-copyable, non-movable RAII guard that masks
maskable CPU IRQs for the lifetime of the object. It is defined in
`<gba/critical_section>`.

## Include

```cpp
#include <gba/critical_section>
```

## Type summary

```cpp
namespace gba {
    struct critical_section final {
        critical_section() noexcept;
        ~critical_section() noexcept;

        critical_section(const critical_section&) = delete;
        critical_section& operator=(const critical_section&) = delete;
        critical_section(critical_section&&) = delete;
        critical_section& operator=(critical_section&&) = delete;

        explicit operator bool() const noexcept;
    };
}
```

## Typical usage

Construction saves the current CPSR IRQ-disable bit and disables maskable IRQs.
Destruction restores the saved bit:

```cpp
{
    const gba::critical_section section;
    update_shared_state();
}
// The IRQ masking state from before the scope is restored.
```

The guard must remain in the scope that it protects. Its deleted copy and move
operations prevent transferring the saved CPU state to another object.

Use a critical section for a short update that must be atomic with respect to an
IRQ handler:

```cpp
volatile unsigned int pending_work = 0;

void add_work(unsigned int amount) {
    const gba::critical_section section;
    pending_work += amount;
}
```

Keep the protected region short. Do not wait for an interrupt, perform a long
copy, or do frame-scale work while the guard is alive.

## Nesting

Guards can be nested. Destroying an inner guard restores the state that was
active when that inner guard was constructed, so it cannot re-enable IRQs while
an outer guard is still alive:

```cpp
{
    const gba::critical_section outer;
    {
        const gba::critical_section inner;
        // IRQs are masked.
    }
    // IRQs are still masked.
}
```

## `operator bool`

`operator bool()` always returns `true` and exists to support an `if`-condition
scope:

```cpp
if (const gba::critical_section section{}) {
    update_shared_state();
}
```

It does not report whether masking succeeded and should not be used as an
interrupt-state query. A normal block is usually clearer for longer scopes.

## Register interaction

The guard masks IRQ acceptance in the CPU's CPSR. It does not modify
`gba::reg_ime` or the individual enables in `gba::reg_ie`. Use those registers
to configure interrupt delivery; use `critical_section` to make a short
sequence atomic with respect to IRQ handlers.

For example, `reg_ime` remains enabled while the CPU is masked:

```cpp
#include <gba/peripherals>

gba::reg_ime = true;
{
    const gba::critical_section section;
    // `gba::reg_ime` is still true, but maskable IRQs are not accepted.
}
```

Pending interrupts are handled after the guard is destroyed when the normal
interrupt conditions are enabled. A critical section does not synchronize DMA
memory access.

## Related pages

- [Interrupts](../concepts/interrupts.md) - configuring and handling IRQs
- [Interrupt Peripheral Reference](./peripherals/interrupts.md) - interrupt
  register layout
