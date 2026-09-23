# Redundancy & Fragmentation

Interrupting a save mid-write, most commonly by removing power, must never corrupt the *previous* value while the
new one is only partially written. `gba::backup_redundant()` tags an individual field's codec so its value is never
overwritten in place: before writing the new value, the table persistently records its intended location. Only after
the write completes does it promote that location to the current value. A write cut short at any point therefore
leaves the previous value untouched and signals that the most recent save attempt was interrupted.

The directory itself uses two CRC-checked generations in separate backend write units, so recording an intent cannot
destroy the last committed directory if power fails during that update.

## Marking a field redundant

Wrap the codec, not the table:

```cpp
struct settings_save {
    bool invert = false;

    static constexpr auto codec = gba::backup_redundant(
        gba::codec::make_tuple_codec(gba::codec::member<&settings_save::invert>(gba::codec::bool_codec))
            .apply<settings_save>());
};

auto& saves = gba::backup.emplace(gba::backup_sram<settings_save::codec>);
```

`emplace<T>(...)` behaves exactly as for a non-redundant field: the alternation is internal. A redundant field whose
last write was interrupted is the exception: `get<T>()` returns `std::nullopt` rather than silently loading the
previous value, so the game can inform the player that their latest save did not complete.

For a fixed-size codec, the table reserves two granularity-aligned slots up front (twice the space of a
non-redundant fixed field) and alternates between them on every `emplace<T>(...)`. For a variable-size codec, each
`emplace<T>(...)` is placed in a free run of the dynamic arena that excludes the field's own current allocation,
defragmenting first if nothing else is currently big enough. The old allocation remains reserved until the new value
is promoted, so it remains available if the save is interrupted.

## Recovering the alternate copy

If a redundant save was interrupted, `get<T>()` returns `std::nullopt` and `get_alternate<T>()` loads the previous
committed value. This lets the game tell the player exactly what happened before loading the recoverable value:

```cpp
if (auto settings = saves.get<settings_save>()) {
    apply(*settings);
} else if (auto previous = saves.get_alternate<settings_save>()) {
    show_save_interrupted_message();
    apply(*previous);
}
```

`get_alternate<T>()` also supports a redundant fixed-size field's inactive slot after a completed save. That remains
useful when your value embeds its own integrity check and the active value fails it. For a variable-size field, the
alternate is available only while an interrupted save is pending: after a successful save its former allocation is
released for reuse.

For example, a fixed-size value with its own CRC can recover from an otherwise valid, completed save whose active
payload fails that application-level check:

```cpp
if (auto settings = saves.get<settings_save>(); settings && checksum_ok(*settings)) {
    apply(*settings);
} else if (auto previous = saves.get_alternate<settings_save>()) {
    apply(*previous);
}
```

## Fragmentation in the dynamic arena

Every variable-size field, redundant or not, shares one dynamic arena following the fixed-size region. As fields
grow, shrink, or are erased, the arena's free space can end up split across more than one run: a large enough hole
for some new or grown value may not exist even though the *total* free space would be enough.

`is_fragmented()` and `defragment()` operate on the whole table rather than one field, so, unlike `get<T>()` or
`emplace<T>(...)`, they take no type argument:

```cpp
if (saves.is_fragmented()) {
    saves.defragment();
}
```

`is_fragmented()` reports whether free space is currently split; `defragment()` compacts every allocated field to the
arena's front, in ascending order of current position, consolidating the free space into one trailing run.
`emplace<T>(...)` already calls `defragment()` automatically, once, whenever a variable-size field doesn't fit as-is,
so most code never needs to call it directly. Call it explicitly to recover space proactively (e.g. after
deleting a large record) or to retry a save that failed because a single grown value could not yet be relocated.
`defragment()` is a no-op while a redundant field has a pending interrupted save, preserving that field's recoverable
previous allocation until the game handles it.

Fixed-size fields never fragment: their bytes are reserved once, at the front of the backend, and never move.

## See also

- [Backup Storage](./backup.md)
- [Low-Level Save Access](../advanced/save.md)
- [`gba::backup` Reference](../reference/backup.md)
