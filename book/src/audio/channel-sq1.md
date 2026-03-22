# Square 1 (CH1)

Channel 1 is a square-wave generator with hardware frequency sweep. Use it for lead lines, arps, and bright hooks.

## Typical use

```cpp
#include <gba/music>

using namespace gba::music;
using namespace gba::music::literals;

// CH1 via UDL
auto lead = "c5 b4 g4 e4"_sq1;

// CH1 with explicit sweep-capable instrument settings
auto lead_sweep = note("c5 e5 g5").channel(
    channel::sq1,
    sq1_instrument{.duty = 2, .sweep_shift = 2}
);

static constexpr auto song = compile(loop(stack(lead, lead_sweep)));
```

## Notes

- CH1 and CH2 are both pulse channels, but only CH1 has hardware sweep.
- Keep CH1 for lines where sweep movement is musically useful.
- `stack()` auto-assigns untagged layers in sq1 -> sq2 -> wav -> noise order.
