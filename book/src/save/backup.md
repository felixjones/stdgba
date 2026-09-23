# Backup Storage

`gba::backup` is a codec-driven save API built on top of the raw SRAM/Flash/EEPROM access described in
[Low-Level Save Access](../advanced/save.md). Instead of hand-rolling offsets and byte layouts, you declare one codec
per value you want to persist; stdgba builds a small on-cartridge directory that maps each codec to its own record and
auto-places every field, fixed- or variable-size, in the backend.

## Creating the backup slot

Define each value's codec as a `static constexpr` member, then list every codec as a template argument to a
`backup_*` factory. Assigning the result to `gba::backup` builds the directory (reading it back if the backend
already holds one) and replaces whatever table was previously held:

```cpp
#include <gba/backup>
#include <gba/codec>

struct player_save {
    std::uint8_t x = 0;
    std::uint8_t y = 0;

    static constexpr auto codec = gba::codec::make_tuple_codec(gba::codec::member<&player_save::x>(gba::codec::uint8_codec),
                                                                gba::codec::member<&player_save::y>(gba::codec::uint8_codec))
                                      .apply<player_save>();
};

struct settings_save {
    bool invert = false;

    static constexpr auto codec = gba::codec::make_tuple_codec(gba::codec::member<&settings_save::invert>(gba::codec::bool_codec))
                                      .apply<settings_save>();
};

gba::backup = gba::backup_sram<player_save::codec, settings_save::codec>;
```

`gba::backup` represents the cartridge's one backup chip, much like `gba::link` represents the GBA's one serial port.
Unlike `gba::link`, there is only ever one physical backend shape per cartridge, so `gba::backup` does not need a
handle on any interface; reach every field directly through it by the value type its codec decodes to.

Available factories: `gba::backup_sram` (byte-addressable, no setup), `gba::backup_flash_64k` / `backup_flash_128k`
(4KB sectors, Macronix/Panasonic/Sanyo/SST chips, auto-detected on first access), `gba::backup_flash_atmel` (128-byte
pages), and `gba::backup_eeprom_512b` / `backup_eeprom_8k` (8-byte blocks). Every field in a table shares one backend.

## Loading and storing values

`get<T>()` returns `std::optional<T>`, `std::nullopt` if the field was never stored (or has been erased).
`emplace<T>(args...)`
constructs a `T` from `args`, or default-constructs it with none, and stores it, returning `false` only if the value
did not fit:

```cpp
player_save player = gba::backup.get<player_save>().value_or(player_save{});

if (!gba::backup.get<settings_save>().has_value()) {
    gba::backup.emplace<settings_save>(); // first boot: default-construct and store
}
settings_save settings = *gba::backup.get<settings_save>();

// ...later, in response to input:
gba::backup.emplace<player_save>(player);
```

`T` is looked up by decoded value type, so it must be unique across every codec passed to the `backup_*` factory.
`reset<T>()` erases a field; a subsequent `get<T>()` reports no value again.

## Fixed- vs. variable-size codecs

A codec built entirely from fixed-width components (`uint8_codec`, `bool_codec`, `make_tuple_codec` of such, etc.) is
detected at compile time and reserves its bytes once, at the front of the backend, alongside the directory itself;
this space can never fragment. A codec that can vary in size (e.g. one built with `gba::codec::vector_of()`) is placed
in a shared dynamic arena that follows the fixed region, and is only ever as large as the value currently stored.

Prefer fixed-size codecs for values with a known upper bound (positions, settings, small structs). Reserve variable-size
codecs for genuinely variable-length data; see [Redundancy & Fragmentation](./redundancy.md) for how the dynamic arena
is kept usable over the cartridge's lifetime.

## Demo: Position and settings persistence

The demo saves the ball's position on **A** and toggles (and saves) an inverted-colours setting on **SELECT**. Both
fields live in one SRAM-backed table; the settings field is default-constructed on first boot, when no prior save
exists.

```cpp
{{#include ../../demos/demo_backup_save.cpp}}
```

![Backup save demo](../img/backup_save.png)

## See also

- [Redundancy & Fragmentation](./redundancy.md)
- [Low-Level Save Access](../advanced/save.md)
- [Composing Codecs](../codecs/index.md)
