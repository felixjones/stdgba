# Noise (CH4)

Channel 4 is the noise generator. In stdgba, `s()` targets this channel and validates drum tokens at compile time.

## Typical use

```cpp
#include <gba/music>

using namespace gba::music;
using namespace gba::music::literals;

auto hats = s("hh ~ hh ~");
auto beat = s("bd sd [rim bd] sd");

static constexpr auto drums = compile(loop(stack(hats, beat)));

// UDL sugar for s()
auto fill = "bd sd hh sd"_s;
```

## Notes

- Prefer `s("...")` for percussion; it rejects pitched notes on noise at compile time.
- 20 Strudel drum names are supported (`bd`, `sd`, `hh`, `rim`, etc.).
- You can still use `.channel(channel::noise)` when you need explicit routing.
