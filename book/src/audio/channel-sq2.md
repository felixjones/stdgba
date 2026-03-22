# Square 2 (CH2)

Channel 2 is a second square-wave generator without sweep. Use it for bass lines, countermelodies, and harmony support.

## Typical use

```cpp
#include <gba/music>

using namespace gba::music;

auto bass = note("c3 c3 g2 g2").channel(channel::sq2);
auto harmony = note("e4 g4 b4 g4").channel(channel::sq2);

static constexpr auto groove = compile(loop(seq(bass, harmony)));
```

## Notes

- CH2 is often paired with CH1 so leads and bass can stay independent.
- If you do not assign channels manually, CH2 is usually the second auto-assigned layer.
- CH2 is a good default for steady tonal support when CH1 handles motion.
