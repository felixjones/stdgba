# Timers

The GBA has four hardware timers (0-3). Each is a 16-bit counter that increments at a configurable rate and can trigger an interrupt on overflow. Timers can cascade -- timer N+1 increments when timer N overflows -- enabling periods far longer than a single 16-bit counter allows.

## Compile-time timer configuration

stdgba configures timers at compile time using `std::chrono` durations. The compiler selects the best prescaler and cascade chain automatically:

```cpp
#include <gba/timer>
using namespace std::chrono_literals;

// A 1-second timer with overflow IRQ
constexpr auto timer = gba::compile_timer(1s, true);

// Write the cascade chain to hardware
std::copy(timer.begin(), timer.end(), gba::reg_tmcnt.begin());
```

`compile_timer` returns a `std::array` of timer register values. A simple duration might need only one timer; a long duration might cascade two or three. The array size is determined at compile time.

### Supported durations

Any `std::chrono::duration` works:

```cpp
constexpr auto fast = gba::compile_timer(16ms);
constexpr auto slow = gba::compile_timer(30s, true);
constexpr auto precise = gba::compile_timer(100us);
```

If the duration cannot be represented exactly, `compile_timer` picks the closest possible configuration. Use `compile_timer_exact` if you need an exact match (compile error if impossible).

### Disabling timers

```cpp
for (std::size_t i = 0; i < timer.size(); ++i)
    gba::reg_tmcnt_h[i] = {};
```

## Raw timer registers

For manual control:

```cpp
// Timer 0: 1024-cycle prescaler, IRQ on overflow
gba::reg_tmcnt_l[0] = 0;  // Reload value
gba::reg_tmcnt_h[0] = { .prescaler = 3, .irq = true, .enable = true };

// Timer 1: cascade from timer 0
gba::reg_tmcnt_l[1] = 0;
gba::reg_tmcnt_h[1] = { .cascade = true, .irq = true, .enable = true };
```

## Prescaler values

| Value | Divider | Frequency |
|-------|---------|-----------|
| 0 | 1 | 16.78 MHz |
| 1 | 64 | 262.2 kHz |
| 2 | 256 | 65.5 kHz |
| 3 | 1024 | 16.4 kHz |

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `compile_timer(1s)` | Manual prescaler + reload calculation |
| `gba::reg_tmcnt_h[0] = { ... };` | `REG_TM0CNT = TM_FREQ_1024 \| TM_ENABLE;` |
| Automatic cascade chain | Manual multi-timer setup |
