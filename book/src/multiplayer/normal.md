# Normal Link

Normal serial mode is a direct connection between two systems. It transfers either 8-bit or 32-bit words and requires
one system to supply the transfer clock.

`gba::link_normal` exposes the same typed, framed message model as Multi-Player mode, but there is only one remote peer.

## Selecting 8-bit or 32-bit mode

The optional third template argument selects the hardware word width. It defaults to `unsigned char` for Normal 8-bit
mode; use `unsigned int` for Normal 32-bit mode.

```cpp
#include <gba/codec>
#include <gba/link>

constexpr auto message_codec = gba::codec::uint8_codec;
constexpr std::size_t link_capacity = 32;

using normal8_type = gba::link_normal<message_codec, link_capacity>;
using normal32_type = gba::link_normal<message_codec, link_capacity, unsigned int>;
```

The word width changes how framed bytes are transported, not the codec's value type. Both connected systems must select
the same width and codec.

## Creating both ends

Exactly one system must be constructed as the parent:

```cpp
auto& link = gba::link.emplace<normal8_type>(is_parent);
```

Pass `true` on the system that supplies the shift clock and `false` on the other system. The application is responsible
for assigning these roles consistently; unlike Multi-Player mode, Normal mode does not assign player IDs.

## Interrupt and polling contract

Forward serial interrupts to the active link:

```cpp
gba::irq_handler = [&link](const gba::irq flags) {
    if (flags.serial) {
        link.irq();
    }
};
gba::reg_ie = {.serial = true};
gba::reg_ime = true;
```

Call `poll()` continuously from your main loop:

```cpp
for (;;) {
    link.poll();

    if (!link.sending()) {
        link.send(local_value);
    }

    while (auto incoming = link.read()) {
        handle_remote_value(*incoming);
    }
}
```

The IRQ handler captures raw completed words and stages the next word. Framing, decoding, callbacks, and starting
transfers happen in `poll()`. Polling only once per video frame unnecessarily limits throughput.

## Sending, receiving, and recovery

`send()` returns `false` while another value is in flight or when the framed value exceeds `Capacity`. `read()` returns
the next complete decoded value, or an empty `std::optional` when no complete value is available.

Normal mode exposes one sticky error state because it has one peer:

```cpp
if (link.has_error(gba::link_error::integrity_failure)) {
    link.clear_error(gba::link_error::integrity_failure);
}

link.on_error([](const std::size_t, const gba::link_error error) {
    handle_link_error(error);
});
```

Error callbacks run from `poll()`, not interrupt context. If a transfer stalls after a cable or peer failure, `reset()`
stops the current hardware transfer, clears queued transport state, and allows polling to start a fresh transfer.

## See also

- [Multi-Player Link](./multi.md)
- [Link Protocol](./protocol.md)
- [Composing Codecs](../codecs/index.md)
- [`gba::link` Reference](../reference/link.md)
