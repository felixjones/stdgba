# Link Protocol

`gba::link_multi` and `gba::link_normal` use the same byte-stream protocol above the GBA serial hardware. The
selected [codec](../codecs/index.md) produces a payload, stdgba places that payload in an integrity-checked frame, and
the frame is packed into the native transfer words of the selected serial mode.

This protocol is internal to stdgba. It is not the raw protocol used by commercial games, and a peer must implement the
same codec, framing, and transport rules to communicate with a stdgba link.

## Protocol layers

Each value passes through three layers:

1. **Codec:** Converts the C++ value to a sequence of payload bytes.
2. **Frame:** Adds delimiters and a CRC, then escapes reserved bytes.
3. **Transport:** Packs the wire bytes into 8-, 16-, or 32-bit GBA serial transfers.

The reverse process runs independently for each remote peer, so a partial or malformed frame from one peer cannot
corrupt another peer's decoder state.

## Codec payload

The codec completely defines the payload format. Both ends must use compatible codecs; the link protocol does not carry
a schema, type identifier, payload length, or protocol version. See [Composing Codecs](../codecs/index.md) for the codec
model and available combinators.

Fixed-width fundamental codecs emit the GBA object representation, which is little-endian. Composite codecs emit their
component codecs in the order specified by the codec. Applications that expect protocol evolution should include a
version or message tag in their own codec.

For user-defined types, keep the codec as a static member of the type it represents.
The [codec chapter](../codecs/index.md#tuples-and-user-defined-types) shows this pattern and explains how to build
streams containing several message types.

## Frame format

Before escaping, a frame has this logical layout:

```text
0xC0 | payload | CRC low | CRC high | 0xC0
```

`0xC0` is both the opening and closing delimiter. A receiver ignores data until it observes a delimiter, allowing it to
join an existing stream or recover synchronization after damaged data.

The two-byte CRC is CRC-16-CCITT with:

| Property           | Value         |
|--------------------|---------------|
| Polynomial         | `0x1021`      |
| Initial value      | `0xFFFF`      |
| Input reflection   | No            |
| Output reflection  | No            |
| Final XOR          | `0x0000`      |
| Trailer byte order | Little-endian |

The CRC covers the unescaped codec payload only. It does not include either delimiter or the CRC trailer itself.

An empty codec payload is valid. Its frame contains the opening delimiter, the CRC of the empty payload, and the closing
delimiter.

## Byte escaping

Four byte values are reserved on the wire:

| Payload or CRC byte | Wire encoding | Purpose                      |
|---------------------|---------------|------------------------------|
| `0x00`              | `0xDB 0xDE`   | Idle padding                 |
| `0xC0`              | `0xDB 0xDC`   | Frame delimiter              |
| `0xDB`              | `0xDB 0xDD`   | Escape prefix                |
| `0xFF`              | `0xDB 0xDF`   | Disconnected-player sentinel |

Escaping applies to both payload bytes and CRC bytes. Unescaped `0x00`, `0xC0`, `0xDB`, and `0xFF` never appear as frame
contents.

`0x00` is used when a hardware transfer word has no frame byte available. Receivers discard it as idle padding. Escaping
zero-valued data keeps this indistinguishable padding out of the codec payload.

Multi-Player hardware reports a missing participant as `0xFFFF`. Escaping every `0xFF` ensures a valid 16-bit wire word
can never equal that sentinel.

An unknown escape sequence, an incomplete escape at a delimiter, or a frame shorter than its two-byte CRC trailer is
malformed. A CRC mismatch is reported separately as an integrity failure.

## Packing into serial words

Wire bytes are packed into hardware words with the earliest byte in the least-significant lane:

```text
bytes:  12 34 56 78
word:   0x78563412
```

Unused high lanes are filled with `0x00` idle bytes.

| Link type     | Hardware word | Bytes per transfer |
|---------------|---------------|--------------------|
| Normal 8-bit  | 8 bits        | 1                  |
| Multi-Player  | 16 bits       | 2                  |
| Normal 32-bit | 32 bits       | 4                  |

Framing is independent of the word size. A frame may begin or end in any byte lane, and several transfers may be needed
for one encoded value.

In Multi-Player mode, each round yields one word from every connected player. stdgba maintains one frame parser and
codec decoder per remote player. In Normal mode there is only one peer stream.

## Sending and flow control

`send()` encodes and frames the complete value from main code. This keeps codec and framing work out of the serial IRQ.
The IRQ consumes prepared wire bytes and stages the next hardware word immediately.

Only one outgoing frame may be in flight. `send()` returns `false` when:

- the previous frame has not been fully staged, or
- the escaped frame, including delimiters and CRC, is larger than `Capacity`.

Because escaping can turn one logical byte into two wire bytes, `Capacity` must account for worst-case expansion. For a
fixed payload of `N` bytes, the largest possible frame is:

```text
2 * N + 6
```

This is two delimiters plus at most twice the payload and twice the two CRC bytes.

## Receive processing and recovery

The serial IRQ records completed raw words in a bounded queue. `poll()` drains that queue, separates peer streams,
removes idle bytes, parses frames, and dispatches errors. `read()` then feeds complete frame payloads into the selected
codec.

The receiver recovers at the next `0xC0` delimiter after malformed input. Errors are exposed as:

| Error               | Meaning                                                                |
|---------------------|------------------------------------------------------------------------|
| `receive_overflow`  | IRQ snapshots or pending peer bytes exceeded their bounded capacity    |
| `malformed_frame`   | Invalid escaping or an incomplete frame                                |
| `integrity_failure` | Received CRC did not match the payload                                 |
| `decode_failure`    | The codec rejected the payload or did not finish at the frame boundary |
| `hardware_failure`  | The GBA serial hardware reported an error                              |
| `topology_changed`  | A Multi-Player peer or local player ID changed                         |

These errors are sticky until cleared. Error callbacks and decoding run from `poll()` or `read()`, never from serial
interrupt context.

## See also

- [Composing Codecs](../codecs/index.md)
- [Multi-Player Link](./multi.md)
- [Normal Link](./normal.md)
