#include <gba/bits/link/wire_word.hpp>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <optional>

int main() {
    using gba::bits::link::pack_wire_word;
    using gba::bits::link::unpack_wire_word;

    {
        const std::array input{
            std::byte{0x12},
            std::byte{0x34},
            std::byte{0x56},
            std::byte{0x78},
        };
        std::size_t index = 0;
        const auto word = pack_wire_word<unsigned int>([&]() -> std::optional<std::byte> { return input[index++]; });
        gba::test.eq(word, 0x78563412u);
        gba::test.is_true(unpack_wire_word(word) == input);
    }

    {
        const std::array input{std::byte{0xAB}};
        std::size_t index = 0;
        const auto word = pack_wire_word<unsigned short>([&]() -> std::optional<std::byte> {
            if (index == input.size()) {
                return std::nullopt;
            }
            return input[index++];
        });
        gba::test.eq(word, static_cast<unsigned short>(0x00AB));
        const auto bytes = unpack_wire_word(word);
        gba::test.eq(bytes[0], std::byte{0xAB});
        gba::test.eq(bytes[1], std::byte{0x00});
    }

    {
        const auto word = pack_wire_word<unsigned char>([]() -> std::optional<std::byte> { return std::nullopt; });
        gba::test.eq(word, static_cast<unsigned char>(0));
        gba::test.eq(unpack_wire_word(word)[0], std::byte{0x00});
    }

    return gba::test.finish();
}
