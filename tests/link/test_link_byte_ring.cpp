#include <gba/bits/link/byte_ring.hpp>
#include <gba/testing>

#include <array>
#include <cstddef>

int main() {
    gba::bits::link::byte_ring<3> ring;

    gba::test.is_true(ring.empty());
    gba::test.is_true(ring.push(std::byte{1}));
    gba::test.is_true(ring.push(std::byte{2}));
    gba::test.is_true(ring.push(std::byte{3}));
    gba::test.is_true(ring.full());
    gba::test.is_false(ring.push(std::byte{4}));
    gba::test.eq(ring.size(), std::size_t{3});

    std::byte byte{};
    gba::test.is_true(ring.pop(byte));
    gba::test.eq(byte, std::byte{1});
    gba::test.is_true(ring.push(std::byte{4}));

    std::array<std::byte, 3> output{};
    gba::test.eq(ring.pop(output), output.size());
    gba::test.eq(output[0], std::byte{2});
    gba::test.eq(output[1], std::byte{3});
    gba::test.eq(output[2], std::byte{4});
    gba::test.is_true(ring.empty());

    const std::array input{std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    gba::test.eq(ring.push(input), std::size_t{3});
    gba::test.is_true(ring.full());
    ring.clear();
    gba::test.is_true(ring.empty());

    return gba::test.finish();
}
