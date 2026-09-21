# Composing Codecs

`<gba/codec>` lets you describe binary formats as small `constexpr` values. A codec owns no message state: it creates an
encoder for a value or a fresh decoder for an incoming byte stream.

This design is inspired by the [Muesli codec library](https://github.com/felixjones/muesli). Codecs can be used directly
with byte buffers and also define the payload type and representation transported by [
`gba::link`](../multiplayer/protocol.md).

## The codec model

Every codec provides:

- `value_type`, the C++ value represented by the codec
- `make_encoder(value)`, which creates an encoder whose `next()` method yields one byte at a time
- `make_decoder()`, which creates a decoder that accepts one byte at a time through `feed()`

Decoder operations return `gba::codec::step`:

| Step    | Meaning                                          |
|---------|--------------------------------------------------|
| `more`  | The byte was accepted and more input is required |
| `done`  | A complete value is available from `value()`     |
| `error` | The bytes do not form a valid value              |

The incremental interface does not allocate by itself or require the complete input to be buffered. The same codec can
decode a span, a link-cable frame, or another byte-at-a-time source.

## Encoding and decoding buffers

Use `encode()` with a fixed-capacity output span:

```cpp
#include <gba/codec>

#include <array>
#include <cstddef>
#include <cstdint>

constexpr auto codec = gba::codec::uint32_codec;

std::array<std::byte, codec.encoded_size> bytes{};
const auto encoded = gba::codec::encode(codec, std::uint32_t{0x12345678}, bytes);
```

`encode()` returns the number of bytes written, or `gba::codec::error::overflow` if the destination is too small.

`decode()` returns both a `std::expected`-like result and the number of bytes consumed:

```cpp
const auto decoded = gba::codec::decode(codec, bytes);
if (decoded.value) {
    const std::uint32_t value = *decoded.value;
}
```

The possible errors are:

| Error        | Meaning                                                |
|--------------|--------------------------------------------------------|
| `incomplete` | The input ended before the value was complete          |
| `invalid`    | The input is not a valid representation                |
| `overflow`   | A bounded codec or output buffer exceeded its capacity |

`encode_to()` appends to a sink supporting `push_back(std::byte)`, such as `std::vector<std::byte>`.

## Fundamental codecs

The fixed-width codecs use the target's object representation and are little-endian on GBA:

```cpp
gba::codec::uint8_codec
gba::codec::int16_codec
gba::codec::uint32_codec
gba::codec::float_codec
gba::codec::byte_codec
```

Corresponding signed and unsigned codecs are provided for widths from 8 to 64 bits. `identity_codec<T>` provides the
same byte-for-byte representation for a suitable trivially copyable type.

Use `bool_codec` for booleans. It emits `0` or `1` and rejects every other input rather than accepting an invalid `bool`
object representation.

`constant_codec<Value>`, `monostate_codec`, and `nullptr_codec` represent values that require no payload bytes.

## stdgba value types

Codecs are provided for stdgba's compact value types:

```cpp
gba::codec::color_codec
gba::codec::keypad_codec
gba::codec::angle_codec
gba::codec::packed_angle_codec<16>
gba::codec::fixed_point_codec<gba::fixed<short>>
```

These codecs preserve the type's raw representation. `color_codec` uses the native 16-bit GBA palette format, including
`grn_lo`. `keypad_codec` preserves both samples held by `gba::keypad`, so the receiver can query held, pressed, and
released keys. `packed_angle_codec<Bits>` uses the compact storage type selected for that precision, while
`fixed_point_codec<T>` preserves the exact fractional-bit representation of `T`.

## Tuples and user-defined types

`make_tuple_codec()` concatenates heterogeneous component formats:

```cpp
constexpr auto position_codec =
    gba::codec::make_tuple_codec(
        gba::codec::uint8_codec,
        gba::codec::uint8_codec);
```

Its value type is the corresponding `std::tuple`. Use `member()` and `apply<T>()` to project that format onto a
user-defined type.

The preferred style is to make the codec a static member of the type it represents. This keeps the value and its wire
format together:

```cpp
struct position {
    unsigned char x;
    unsigned char y;

    constexpr position(unsigned char x, unsigned char y)
        : x(x), y(y) {}

    static constexpr auto codec =
        gba::codec::make_tuple_codec(
            gba::codec::member<&position::x>(gba::codec::uint8_codec),
            gba::codec::member<&position::y>(gba::codec::uint8_codec))
            .apply<position>();
};
```

Encoding reads the selected members. Decoding constructs `position` from the decoded values in codec order. Members
omitted from the codec remain local state and are not serialised.

`pair_codec(a, b)` is the corresponding convenience builder for `std::pair`.

## Collections and optional values

Use the collection combinators to build larger formats:

| Builder                      | Value type             | Representation                                   |
|------------------------------|------------------------|--------------------------------------------------|
| `array_of<N>(element)`       | `std::array<T, N>`     | Exactly `N` consecutive elements                 |
| `vector_of(element)`         | `std::vector<T>`       | Varint length followed by the elements           |
| `basic_string_of(character)` | `std::basic_string<T>` | Varint length followed by characters             |
| `string_codec`               | `std::string`          | Varint length followed by bytes                  |
| `optional_of(inner)`         | `std::optional<T>`     | Presence byte followed by the value when present |

`vector_of` and string codecs allocate because their decoded sizes are dynamic. Prefer fixed arrays or bounded codecs in
memory-constrained and latency-sensitive paths.

## Variants

`make_variant_codec()` creates a tagged union. It emits a zero-based alternative index followed by the selected
alternative:

```cpp
constexpr auto message_codec =
    gba::codec::make_variant_codec(
        position::codec,
        gba::codec::uint8_codec);

using message = decltype(message_codec)::value_type;
```

The value type is `std::variant<position, std::uint8_t>`. A discriminant outside the available alternatives is rejected
as invalid.

Variants are useful for link protocols because one framed stream can carry several message types while retaining
compile-time type checking.

## Variable-length integers

`unsigned_varint_codec` uses an unsigned LEB128-style encoding. Values below 128 occupy one byte. `signed_varint_codec`
applies zigzag encoding first, keeping small positive and negative values compact.

Both codecs are bounded to at most ten bytes and do not allocate.

## Transforming representations

`transform(inner, encode_fn, decode_fn)` exposes a different logical type while retaining an existing wire
representation:

```cpp
struct percentage {
    unsigned char value;

    static constexpr auto codec = gba::codec::transform(
        gba::codec::uint8_codec,
        [](percentage value) { return value.value; },
        [](unsigned char value) { return percentage{value}; });
};
```

The encode function maps the public value to the inner codec's value. The decode function maps the inner value back to
the public type.

## Fixed-size codecs

A codec satisfying `gba::codec::FixedSizeCodec` exposes a compile-time `encoded_size`. Fundamental codecs, fixed arrays,
and composites made entirely from fixed-size codecs preserve this property:

```cpp
constexpr auto codec =
    gba::codec::make_tuple_codec(
        gba::codec::uint8_codec,
        gba::codec::uint32_codec);

static_assert(gba::codec::FixedSizeCodec<decltype(codec)>);
static_assert(codec.encoded_size == 5);
```

Fixed-size codecs make buffer sizing straightforward. A variable-size codec can still be used with a bounded
`gba::link`, but the encoded and escaped frame must fit the link's configured capacity.

## Defining a custom codec

A custom codec only needs to satisfy the `gba::codec::Codec` concept:

```cpp
struct example_codec {
    using value_type = example;

    struct decoder {
        gba::codec::step state() const;
        gba::codec::step feed(std::byte byte);
        value_type value() const;
    };

    struct encoder {
        std::optional<std::byte> next();
    };

    decoder make_decoder() const;
    encoder make_encoder(const value_type& value) const;
};
```

Add `static constexpr std::size_t encoded_size` when every encoded value always has the same size. Decoders may also
expose `error_reason()` to report a specific `gba::codec::error`; otherwise a failed decoder is reported as `invalid`.

## See also

- [Link Protocol](../multiplayer/protocol.md)
- [Multi-Player Link](../multiplayer/multi.md)
- [Normal Link](../multiplayer/normal.md)
