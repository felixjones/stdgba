/// @file tests/string/test_memchr.cpp
/// @brief Tests for optimized memchr implementation.
///
/// Tests:
/// - Found at various positions and alignments
/// - Not found (count exhausted)
/// - Count == 0
/// - Search byte at each word position (0-3)
/// - Long buffers
/// - Search for 0x00

#include <gba/testing>

#include <cstdint>
#include <cstring>

static void* (*volatile memchr_fn)(const void*, int, std::size_t) = &std::memchr;

namespace {

    alignas(4) unsigned char buf[512];

    void fill(unsigned char v, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) buf[i] = v;
    }

    std::uintptr_t addr(const void* p) {
        return reinterpret_cast<std::uintptr_t>(p);
    }

} // namespace

int main() {
    // count == 0: always NULL
    fill(0xAA, 16);
    gba::test.eq(addr(memchr_fn(buf, 0xAA, 0)), addr(nullptr));

    // Found at position 0
    fill(0xBB, 16);
    buf[0] = 0x42;
    gba::test.eq(addr(memchr_fn(buf, 0x42, 16)), addr(&buf[0]));

    // Found at various positions (word boundary testing)
    for (int pos = 0; pos < 32; ++pos) {
        fill(0xFF, 64);
        buf[pos] = 0x42;
        gba::test.eq(addr(memchr_fn(buf, 0x42, 64)), addr(&buf[pos]));
    }

    // Found at various positions with different alignment offsets
    for (int off = 0; off < 4; ++off) {
        for (int pos = 0; pos < 20; ++pos) {
            fill(0xCC, 64);
            buf[off + pos] = 0x42;
            gba::test.eq(addr(memchr_fn(&buf[off], 0x42, 40)), addr(&buf[off + pos]));
            buf[off + pos] = 0xCC; // restore
        }
    }

    // Not found
    fill(0xAA, 64);
    gba::test.eq(addr(memchr_fn(buf, 0x42, 64)), addr(nullptr));

    // Not found (byte exists beyond count)
    fill(0xAA, 64);
    buf[32] = 0x42;
    gba::test.eq(addr(memchr_fn(buf, 0x42, 32)), addr(nullptr));
    gba::test.eq(addr(memchr_fn(buf, 0x42, 33)), addr(&buf[32]));

    // Search for 0x00
    fill(0xFF, 64);
    buf[10] = 0x00;
    gba::test.eq(addr(memchr_fn(buf, 0, 64)), addr(&buf[10]));

    // Long buffer
    fill(0xDD, 500);
    buf[499] = 0x42;
    gba::test.eq(addr(memchr_fn(buf, 0x42, 500)), addr(&buf[499]));

    // Multiple occurrences: should return first
    fill(0xDD, 64);
    buf[5] = 0x42;
    buf[10] = 0x42;
    buf[20] = 0x42;
    gba::test.eq(addr(memchr_fn(buf, 0x42, 64)), addr(&buf[5]));

    // byte value > 127 (high bit set)
    fill(0x00, 64);
    buf[7] = 0x80;
    gba::test.eq(addr(memchr_fn(buf, 0x80, 64)), addr(&buf[7]));

    fill(0x00, 64);
    buf[7] = 0xFF;
    gba::test.eq(addr(memchr_fn(buf, 0xFF, 64)), addr(&buf[7]));

    return gba::test.finish();
}
