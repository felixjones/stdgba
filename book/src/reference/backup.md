# `gba::backup` Reference

`gba::backup` is the process-wide backup-chip slot from `<gba/backup>`. It is type-erased the same way as
`gba::link`'s slot: assigning it a table (built by a `gba::backup_*` factory) determines the concrete
`gba::bits::backup::table` type it holds, until the next assignment.

For a tutorial-style walkthrough, see [Backup Storage](../save/backup.md) and
[Redundancy & Fragmentation](../save/redundancy.md).

## Include

```cpp
#include <gba/backup>
```

## Type summary

```cpp
namespace gba {

    struct backup_slot {
    public:
        template<typename Held>
        decltype(auto) operator=(Held&& value);

        template<typename Held>
        decltype(auto) emplace(Held&& value);

        template<typename Held, typename... Args>
        Held& emplace(Args&&... args);

        template<typename Held>
        Held& get() const noexcept;

        template<typename Held>
        Held* get_if() const noexcept;

        template<typename Held>
        bool holds() const noexcept;

        void reset() noexcept;

        explicit operator bool() const noexcept;
    };

    inline backup_slot& backup;

    template<const auto&... Codecs> inline constexpr auto backup_sram = /* ... */;
    template<const auto&... Codecs> inline constexpr auto backup_flash_64k = /* ... */;
    template<const auto&... Codecs> inline constexpr auto backup_flash_128k = /* ... */;
    template<const auto&... Codecs> inline constexpr auto backup_flash_atmel = /* ... */;
    template<const auto&... Codecs> inline constexpr auto backup_eeprom_512b = /* ... */;
    template<const auto&... Codecs> inline constexpr auto backup_eeprom_8k = /* ... */;

    template<gba::codec::Codec C>
    consteval auto backup_redundant(const C& codec) noexcept;
}

namespace gba::bits::backup {

    template<typename... Records>
    class table {
    public:
        static constexpr std::size_t size = /* ... */;

        template<std::size_t I> using field_type = /* ... */;

        template<std::size_t I> decltype(auto) field();
        template<typename T> decltype(auto) field();

        template<std::size_t I> std::optional<typename field_type<I>::value_type> get() const;
        template<typename T> std::optional<T> get() const;

        template<std::size_t I> std::optional<typename field_type<I>::value_type> get_alternate() const;
        template<typename T> std::optional<T> get_alternate() const;

        template<std::size_t I, typename... Args> bool emplace(Args&&... args);
        template<typename T, typename... Args> bool emplace(Args&&... args);

        template<std::size_t I> void erase();
        template<typename T> void erase();

        template<std::size_t I> constexpr std::size_t offset() const noexcept;
        template<std::size_t I> constexpr std::pair<std::size_t, std::size_t> redundant_offsets() const noexcept;

        bool is_fragmented() const noexcept;
        void defragment();
        std::size_t free_granules() const noexcept;
        std::size_t largest_free_run() const noexcept;
    };
}
```

## `gba::backup`

### `operator=` / `emplace(value)`

Assigns a `backup_*` factory (or an already-built table), replacing whatever was previously held. A zero-argument
callable `Held` (a `backup_*` factory) is invoked to build the actual table; either form returns a reference to the
newly held table:

```cpp
gba::backup = gba::backup_sram<player_save::codec, settings_save::codec>;
// or, capturing the returned reference:
auto& saves = gba::backup.emplace(gba::backup_sram<player_save::codec, settings_save::codec>);
```

### `emplace<Held>(args...)`

Constructs a `Held` from `args` (default: none), replacing whatever was previously held, and returns a reference to
it. `Held` is the concrete table type, e.g. `decltype(gba::backup_sram<...>)::table_type`; every `gba::bits::backup::table`
is default-constructible, so `args` is rarely needed.

### `get<Held>()`

Returns a reference to the held table. `Held` must be named explicitly, exactly as `gba::link.get<link_type>()` does;
there is no way to recover it without already knowing it, since the slot itself carries no compile-time type
information about what it holds. It traps if `Held` is not the held type; use `get_if<Held>()` when the type may
differ.

### `get_if<Held>()`

Returns a pointer to the held table when its type is exactly `Held`, otherwise `nullptr`.

### `holds<Held>()`

Whether the currently held table's type is `Held`.

### `reset()`

Destroys the currently held table, if any.

### `explicit operator bool()`

Whether any table is currently held.

## `gba::bits::backup::table<Records...>`

The concrete type built by a `gba::backup_*` factory (e.g. `decltype(gba::backup_sram<Codecs...>)::table_type`), and
the type `gba::backup.emplace(...)` returns a reference to. Every operation below is a plain method on this type;
there is no further indirection through `gba::backup` once you hold the reference.

Fields are addressed either by declaration index `I` (position in the `Codecs...` list passed to the `backup_*`
factory) or by decoded value type `T`, exactly like `std::tuple`'s `std::get<I>`/`std::get<T>`. A type-based overload
is ill-formed unless exactly one field decodes to `T`.

| Method | Description |
|---|---|
| `size` | Number of fields (static constexpr). |
| `field_type<I>` | The declared `gba::bits::backup::record` type at index `I`. |
| `field<I>()` / `field<T>()` | The record at index `I` (or decoding to `T`): a persistent reference for a fixed-size field, a fresh snapshot for a variable-size one. |
| `get<I>()` / `get<T>()` | Load the field; `std::nullopt` if never stored, erased, or its redundant save intent is still pending after an interrupted save. |
| `get_alternate<I>()` / `get_alternate<T>()` | Load a redundant field's previous committed value when an interrupted save is pending. Fixed-size fields also expose their inactive slot after a completed save. See [Recovering the alternate copy](../save/redundancy.md#recovering-the-alternate-copy). |
| `emplace<I>(args...)` / `emplace<T>(args...)` | Construct a value from `args` (default: none) and store it. Returns `false` if it did not fit. |
| `erase<I>()` / `erase<T>()` | Erase the field; a subsequent `get` reports no value again. |
| `offset<I>()` | The granularity-aligned byte offset of a non-redundant fixed-size field. |
| `redundant_offsets<I>()` | The pair of granularity-aligned offsets reserved for a redundant fixed-size field. |
| `is_fragmented()` | Whether the dynamic (variable-size) arena's free space is split across more than one run. |
| `defragment()` | Compact every allocated variable-size field to the arena's front, consolidating free space into one trailing run. Called automatically, once, by `emplace(...)` when a value doesn't fit as-is. It is a no-op while a redundant save is pending. |
| `free_granules()` | Total unallocated space in the dynamic arena, in `Backend::granularity` units. |
| `largest_free_run()` | Size of the largest contiguous free run in the dynamic arena. |

## `gba::backup_*` factories

Each factory is a stateless tag naming a physical backend and root codecs; the actual table (and its directory read
from the backend) is only built once assigned to `gba::backup`. Each factory's `table_type` member alias names the
concrete `gba::bits::backup::table` it produces.

| Factory | Backend | Granularity |
|---|---|---|
| `backup_sram<Codecs...>` | SRAM (32KB) | 1 byte, packed byte-for-byte |
| `backup_flash_64k<Codecs...>` | Flash, standard chips (64KB) | 4KB sector |
| `backup_flash_128k<Codecs...>` | Flash, standard chips (128KB) | 4KB sector |
| `backup_flash_atmel<Codecs...>` | Flash, Atmel chips | 128-byte page |
| `backup_eeprom_512b<Codecs...>` | EEPROM (512B) | 8-byte block |
| `backup_eeprom_8k<Codecs...>` | EEPROM (8KB) | 8-byte block |

Every field in one table shares the same backend. `Codecs` must decode to mutually distinct types to use the
type-based `get<T>()`/`emplace<T>()`/`erase<T>()` overloads; there is no header, footer, or schema version, only a
CRC-checked directory of per-field offsets.

## `gba::backup_redundant(codec)`

Tags `codec`'s field so a new value is never written over the currently stored one; it always lands somewhere else
first, and the field's directory entry only flips to it once that write fully lands. Works for both fixed- and
variable-size codecs. See [Redundancy & Fragmentation](../save/redundancy.md).

## Related pages

- [Backup Storage](../save/backup.md) - creating the slot, loading and storing values
- [Redundancy & Fragmentation](../save/redundancy.md) - power-loss safety, `get_alternate`, defragmentation
- [Low-Level Save Access](../advanced/save.md) - raw SRAM/Flash/EEPROM access underneath `gba::backup`
