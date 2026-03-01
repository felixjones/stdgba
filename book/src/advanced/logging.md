# Logging

stdgba provides a logging system with pluggable backends for emulator debug output. It auto-detects whether the game is running under mGBA or no$gba and routes log messages to the appropriate debug console.

## Setup

```cpp
#include <gba/logger>

using namespace gba::literals;

int main() {
    // Auto-detect emulator and initialise
    if (gba::log::init()) {
        gba::log::info("Game started!");
    }
}
```

`init()` returns `true` if a supported emulator was detected, `false` otherwise (a null backend is installed so logging calls are safe but do nothing).

## Log levels

Five severity levels are available:

```cpp
gba::log::fatal("Critical error");
gba::log::error("Something failed");
gba::log::warn("Potential problem");
gba::log::info("Status update");
gba::log::debug("Verbose trace");
```

### Filtering by level

```cpp
gba::log::set_level(gba::log::level::warn);
// Only fatal, error, and warn messages are output
```

### Runtime level selection

Use `write()` when the log level is determined at runtime:

```cpp
gba::log::level lvl = config.verbose ? gba::log::level::debug : gba::log::level::info;
gba::log::write(lvl, "Message");
```

## Formatted logging

Log messages support the same format string syntax as `<gba/format>`:

For full format syntax (`{x}`, `{x:X}`, named args, and generator behaviour), see [String Formatting](./formatting.md).

```cpp
using namespace gba::literals;

gba::log::info("HP: {hp}"_fmt, "hp"_arg = 42);
gba::log::warn("Sector {s} failed"_fmt, "s"_arg = 3);
```

## Custom backends

Implement the `gba::log::backend` interface to route logs anywhere:

```cpp
struct screen_logger : gba::log::backend {
    int line = 0;
    std::size_t write(gba::log::level lvl, const char* msg, std::size_t len) override {
        draw_text(0, line++, msg);
        return len;
    }
};

screen_logger my_logger;
gba::log::set_backend(&my_logger);
```

## Built-in backends

| Backend | Emulator | Detection |
|---------|----------|-----------|
| `mgba_backend` | mGBA | Writes to `0x4FFF780` debug registers |
| `nocash_backend` | no$gba | Writes to `0x4FFFA00` signature-based output |
| `null_backend` | (fallback) | Discards all output |

`init()` tries mGBA first, then no$gba, then falls back to the null backend.
