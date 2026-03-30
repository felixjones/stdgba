# Hello Audio

Now that you can draw and move a sprite, the next step is sound.

This demo plays a short PSG jingle when you press `A`.

## The code

```cpp
{{#include ../../demos/demo_hello_audio.cpp:4:}}
```

## What is happening?

1. We set up VBlank + interrupts as in earlier chapters.
2. We enable PSG output with `reg_soundcnt_x`, `reg_soundcnt_l`, and `reg_soundcnt_h`.
3. `note("c5 e5 g5 c6").channel(channel::sq1).press()` builds a staccato pattern (each note plays half duration, rests half), ensuring the jingle ends in silence naturally.
4. `compile<2_cps>(...)` compiles at 2 cycles per second (4x faster than the default 0.5 cps), making the jingle snappy and brief.
5. `music_player<jingle>` advances once per frame, dispatching note events.
6. Pressing `A` resets the player with `player = {}`, restarting the jingle from the beginning.

## Next step

Move on to [Registers & Peripherals](../concepts/registers.md), then dive deeper into [Music Composition](../audio/music.md).
