# Compression

stdgba provides `consteval` compression functions that compress data entirely at compile time. The compressed output is compatible with the GBA BIOS decompression routines, so assets can be stored compressed in ROM and decompressed at runtime with a single BIOS call.

## Supported algorithms

| Algorithm | Best for | Header format |
|-----------|----------|---------------|
| **LZ77** | Repeated patterns (tiles, maps) | BIOS-compatible |
| **Huffman** | Skewed symbol frequencies (text) | BIOS-compatible |
| **RLE** | Long runs of identical values | BIOS-compatible |
| **BitPack** | Reducing bit depth (e.g., 32-bit to 4-bit) | BIOS-compatible |

## LZ77 compression

```cpp
#include <gba/compress>
#include <gba/bios>

// Compress tilemap data at compile time
constexpr auto compressed_map = gba::lz77_compress([] {
    return std::array<unsigned short, 1024>{
        0, 0, 0, 1, 1, 1, 2, 2, 2, // ...
    };
});

// Decompress at runtime using BIOS
alignas(4) std::array<unsigned short, 1024> buffer;
gba::LZ77UnCompWram(compressed_map, buffer.data());
```

Use `LZ77UnCompWram` for general RAM targets and `LZ77UnCompVram` for video RAM (which requires halfword writes).

## Huffman compression

```cpp
constexpr auto compressed_text = gba::huffman_compress([] {
    return std::array<unsigned char, 256>{ /* text data */ };
});

alignas(4) std::array<unsigned char, 256> buffer;
gba::HuffUnCompReadNormal(compressed_text, buffer.data());
```

## RLE compression

```cpp
constexpr auto compressed_fill = gba::rle_compress([] {
    return std::array<unsigned char, 512>{ /* data with runs */ };
});

alignas(4) std::array<unsigned char, 512> buffer;
gba::RLUnCompReadNormalWrite8bit(compressed_fill, buffer.data());
```

## Bit packing

Bit packing reduces the bit depth of data elements. Useful for compacting palette indices or other small values:

```cpp
constexpr auto packed = gba::bit_pack<4>([] {
    return std::array<unsigned int, 64>{ 0, 1, 2, 3, /* 4-bit values in 32-bit containers */ };
});
```

## Combining with differential filtering

For data with gradual changes (audio waveforms, gradients), apply a differential filter before compression:

```cpp
#include <gba/filter>
#include <gba/compress>

constexpr auto filtered = gba::diff_filter<1>([] {
    return std::array<unsigned char, 512>{
        128, 130, 132, 134, 136, // ...
    };
});

constexpr auto compressed = gba::lz77_compress([] { return filtered; });
```
