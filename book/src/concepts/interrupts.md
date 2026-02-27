# Interrupts

The GBA uses interrupts to notify the CPU about hardware events: VBlank, HBlank, timer overflow, DMA completion, serial communication, and keypad input.

For the raw register bitfields, see [Interrupt Peripheral Reference](../reference/peripherals/interrupts.md).

## Setting up interrupts

Before any BIOS wait function will work, you must install an IRQ handler. The normal stdgba path is the high-level dispatcher exposed as `gba::irq_handler`:

```cpp
#include <gba/bios>
#include <gba/interrupt>
#include <gba/peripherals>

// Install the default dispatcher / empty stdgba IRQ stub
gba::irq_handler = {};

// Enable specific interrupt sources
gba::reg_dispstat = { .enable_irq_vblank = true };
gba::reg_ie = { .vblank = true };
gba::reg_ime = true;

// Now VBlankIntrWait() works
gba::VBlankIntrWait();
```

### The three switches

Interrupts require three things to be enabled:

1. **Source** - the hardware peripheral must be configured to fire an interrupt (for example `reg_dispstat.enable_irq_vblank`)
2. **`reg_ie`** - the Interrupt Enable register must have the corresponding bit set
3. **`reg_ime`** - the Interrupt Master Enable must be `true`

All three must be set for the interrupt to reach the handler.

## High-level custom handlers

You can provide a callable (lambda, function pointer, etc.) to `gba::irq_handler`:

```cpp
volatile int vblank_count = 0;

gba::irq_handler = [](gba::irq irq) {
    if (irq.vblank) {
        ++vblank_count;
    }
};
```

The handler receives a `gba::irq` bitfield with named boolean fields for each interrupt source. stdgba's internal IRQ wrapper acknowledges `REG_IF` and the BIOS IRQ flag for you before calling the handler, so BIOS wait functions continue to work.

## Uninstalling the dispatcher

To uninstall the stdgba user handler and restore the built-in empty acknowledgement stub, use either of these:

```cpp
gba::irq_handler = gba::nullisr;
// or
gba::irq_handler.reset();
```

This removes the current callable, but still leaves a valid low-level IRQ stub installed so BIOS wait functions remain usable.

## Installing a low-level custom handler

For advanced use, you can replace the IRQ vector directly instead of going through `gba::irq_handler`.

The BIOS IRQ function pointer lives at `0x3007FFC`, so a raw handler can be installed with a register write:

```cpp
#include <gba/peripherals>

extern "C" void my_raw_irq();

gba::registral<void(*)()>{0x3007FFC} = my_raw_irq;
```

This bypasses the stdgba dispatcher entirely. Use it only when you need complete control over IRQ entry/exit.

### What a raw handler must do itself

If you install a low-level handler directly, you are responsible for the work normally done by stdgba's internal wrapper:

- acknowledge `REG_IF`
- acknowledge the BIOS IRQ flag (`0x03FFFFF8`)
- preserve the registers and CPU state your handler clobbers
- restore any IRQ masking state you change
- keep BIOS wait functions (`VBlankIntrWait()`, `IntrWait()`) working correctly

If you skip the acknowledgements, the interrupt may immediately retrigger or BIOS wait functions may stop working.

## Uninstalling a low-level custom handler

If you want to remove a raw handler and go back to stdgba's safe empty stub, use:

```cpp
gba::irq_handler.reset();
```

If instead you want to return to the normal high-level dispatcher path, assign a callable again:

```cpp
gba::irq_handler = [](gba::irq irq) {
    if (irq.vblank) {
        // ...
    }
};
```

### Important note about `irq_handler` state queries

`gba::irq_handler.has_value()` reports whether the low-level vector currently points at *something other than* stdgba's empty handler. That means it will also report `true` for a raw handler installed directly.

However, `gba::irq_handler.value()` only returns your callable when the vector points at stdgba's own dispatcher wrapper. If you install a raw handler directly, `value()` behaves as if no user callable is installed.

## Available interrupt sources

| Field | Source |
|-------|--------|
| `.vblank` | Vertical blank |
| `.hblank` | Horizontal blank |
| `.vcounter` | V-counter match |
| `.timer0` | Timer 0 overflow |
| `.timer1` | Timer 1 overflow |
| `.timer2` | Timer 2 overflow |
| `.timer3` | Timer 3 overflow |
| `.serial` | Serial communication |
| `.dma0`-`.dma3` | DMA channel completion |
| `.keypad` | Keypad interrupt |
| `.gamepak` | Game Pak interrupt |

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::irq_handler = {};` | `irq_init(NULL);` |
| `gba::irq_handler = my_fn;` | `irq_set(II_VBLANK, my_fn);` |
| `gba::irq_handler = gba::nullisr;` | (no direct equivalent) |
| `gba::irq_handler.reset();` | (no direct equivalent) |
| `gba::registral<void(*)()>{0x3007FFC} = my_raw_irq;` | direct IRQ vector write |
| `gba::reg_ie = { .vblank = true };` | `irq_enable(II_VBLANK);` |
