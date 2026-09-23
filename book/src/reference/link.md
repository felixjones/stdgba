# `gba::link` Reference

`gba::link` is the process-wide serial-port slot from `<gba/link>`. It holds exactly one active link transport
(`gba::link_multi` or `gba::link_normal`), constructed in static storage and reached by its own type at every call
site.

For a tutorial-style walkthrough, see [Multi-Player Link](../multiplayer/multi.md), [Normal Link](../multiplayer/normal.md),
and [Link Protocol](../multiplayer/protocol.md).

## Include

```cpp
#include <gba/link>
```

## Type summary

```cpp
namespace gba {

    struct link_slot {
        template<typename Link>
        Link& operator=(Link&& value);

        template<typename Link, typename... Args>
        Link& emplace(Args&&... args);

        template<typename Link>
        Link& get() const noexcept;

        template<typename Link>
        Link* get_if() const noexcept;

        template<typename Link>
        bool holds() const noexcept;

        void reset() noexcept;

        explicit operator bool() const noexcept;
    };

    inline link_slot& link;

    template<const auto& Codec, std::size_t Capacity, typename Word = unsigned char>
    using link_normal = /* ... */;

    template<const auto& Codec, std::size_t Capacity>
    using link_multi = /* ... */;

    enum class link_error : std::uint16_t {
        none, receive_overflow, malformed_frame, integrity_failure, decode_failure, hardware_failure, topology_changed
    };

    enum class multi_player : unsigned short {
        player_0, player_1, player_2, player_3
    };
}
```

## `gba::link`

### `emplace<Link>(args...)`

Destroys whatever transport is currently held, constructs a new `Link` in static storage from `args`, and returns a
typed reference to it. `operator=` forwards to this.

```cpp
auto& link = gba::link.emplace<link_type>(gba::sio_baud_115200);
```

### `get<Link>()`

Returns a typed reference to the currently held transport. It traps if `Link` is not the held type; use
`get_if<Link>()` when the type is not already known.

### `get_if<Link>()`

Returns a pointer to the held transport when its type is exactly `Link`, otherwise `nullptr`.

### `holds<Link>()`

Returns whether the currently held transport's type is exactly `Link`.

### `reset()`

Destroys the currently held transport, if any. `gba::link` then holds nothing until the next `emplace`/`operator=`.

### `explicit operator bool()`

Whether any transport is currently held.

## `gba::link_multi<Codec, Capacity>`

Multi-Player serial mode: two to four systems, each transfer exchanging one 16-bit word with every connected system.

| Method | Description |
|---|---|
| `send(value)` | Begin sending one encoded value to every connected peer. `false` if a value is already in flight or the framed value exceeds `Capacity`. |
| `sending()` | Whether a value is still in flight. |
| `read(player)` | Next decoded value from `player`'s stream, or `std::nullopt`. `player` is a `gba::multi_player`. |
| `pending_wire(player)` | Bytes currently buffered, undecoded, for `player`. |
| `player()` | The local `gba::multi_player` identifier. |
| `is_parent()` | Whether the local player is player `0`. |
| `connected()` | Current four-player connection mask. |
| `connected_count()` | Number of currently connected peers, including self. |
| `poll()` | Process completed transfers, update topology, dispatch errors, start new rounds. Call continuously. |
| `irq()` | Forward the serial interrupt here; captures the completed hardware transfer only. |
| `errors(player)` / `has_error(player, error)` | Query `player`'s sticky error state. |
| `clear_error(player, error)` / `clear_errors(player)` | Clear one or all of `player`'s sticky errors. |
| `on_error(handler)` | `handler(std::size_t player, link_error error)`, called from `poll()`. |
| `reset_peer(player)` | Reset `player`'s receive stream state. |
| `busy()` | Whether a hardware transfer is currently in flight. |

`configure(baud)` (static) reconfigures Multi-Player mode at the given `gba::sio_baud`; the constructor calls this
with `Capacity`'s codec and `sio_baud_115200` by default. Available rates: `sio_baud_9600`, `sio_baud_38400`,
`sio_baud_57600`, `sio_baud_115200`.

`gba::multi_player` is the bounded peer identifier: `player_0`, `player_1`, `player_2`, or `player_3`.
Iterate all four with `gba::multi_players`, and use `gba::multi_player_index(player)` when indexing your own
per-player array.

## `gba::link_normal<Codec, Capacity, Word>`

Normal serial mode: a direct two-system connection. `Word` selects the hardware transfer width, `unsigned char` for
8-bit mode (default) or `unsigned int` for 32-bit mode.

| Method | Description |
|---|---|
| `send(value)` | Begin sending one encoded value. `false` if a value is already in flight or the framed value exceeds `Capacity`. |
| `sending()` | Whether a value is still in flight. |
| `read()` | Next decoded value from the one peer, or `std::nullopt`. |
| `pending_wire()` | Bytes currently buffered, undecoded. |
| `poll()` | Process completed transfers, dispatch errors, start new rounds. Call continuously. |
| `irq()` | Forward the serial interrupt here. |
| `errors()` / `has_error(error)` | Query the one peer's sticky error state. |
| `clear_error(error)` / `clear_errors()` | Clear one or all sticky errors. |
| `on_error(handler)` | `handler(std::size_t, link_error error)`, called from `poll()`; `std::size_t` is always `0`. |
| `reset_peer()` | Reset the receive stream state. |
| `reset()` | Stop the current hardware transfer and clear queued transport state, so polling starts a fresh transfer. |
| `busy()` | Whether a hardware transfer is currently in flight. |

The constructor takes one `bool`: `true` on the system supplying the shift clock (the parent), `false` on the other.

## `gba::link_error`

A bitmask of sticky error flags, combinable with `operator|`:

| Flag | Meaning |
|---|---|
| `receive_overflow` | Incoming wire data exceeded `Capacity` before a full value decoded. |
| `malformed_frame` | Framing/escaping was invalid. |
| `integrity_failure` | The frame's integrity check failed. |
| `decode_failure` | The codec failed to decode a well-formed frame. |
| `hardware_failure` | The serial hardware reported an error. |
| `topology_changed` | (Multi-Player only) A peer connected, disconnected, or the local player ID changed. |

## Related pages

- [Multi-Player Link](../multiplayer/multi.md) - creating and polling a Multi-Player link
- [Normal Link](../multiplayer/normal.md) - two-system connections, 8-bit and 32-bit modes
- [Link Protocol](../multiplayer/protocol.md) - framing, escaping, and integrity check details
- [Composing Codecs](../codecs/index.md) - defining the message codec
