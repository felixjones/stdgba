# Save Data (Flash)

The GBA supports several save types. stdgba provides an API for Flash storage (64 KB or 128 KB), which is the most common save type for games that need persistent data.

## Writing and reading

```cpp
#include <gba/save>

// Write a save structure
struct SaveData {
    int level;
    int score;
    char name[16];
};

SaveData save = { .level = 5, .score = 12000 };
gba::flash::write(0, &save, sizeof(save));

// Read it back
SaveData loaded;
gba::flash::read(0, &loaded, sizeof(loaded));
```

## Flash specifics

Flash memory requires sector-level erasing before writing. The stdgba Flash API handles this automatically, but be aware:

- Writing is slow (milliseconds per byte)
- Flash has a limited number of erase cycles (~100,000)
- Always write during a safe time (not mid-frame)

## tonclib comparison

Flash access in tonclib requires manual BIOS calls or library functions. stdgba wraps these into a simpler interface.
