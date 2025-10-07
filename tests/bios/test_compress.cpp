#include <gba/bios>
#include <gba/compress>

#include <mgba_test.hpp>

int main() {
    {
        static constexpr auto expected = std::array<char, 16>{ 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5 };
        static constexpr auto compressed = gba::bit_pack([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::BitUnPack(compressed.data(), dest.data(), compressed);
        ASSERT_EQ(dest, expected);
    }

    {
        static constexpr auto expected = std::array<char, 16>{ 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5 };
        static constexpr auto compressed = gba::rle_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::RLUnCompWram(compressed, dest.data());
        ASSERT_EQ(dest, expected);
    }

    {
        static constexpr auto expected = std::string_view{ "Hello, World!!" };
        static constexpr auto compressed = gba::lz77_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::LZ77UnCompWram(compressed, dest.data());
        ASSERT_EQ(dest, expected);
    }

    {
        static constexpr auto expected = std::string_view{ "Hello, World!!" };
        static constexpr auto compressed = gba::huffman_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::HuffUnComp(compressed, dest.data());
        ASSERT_EQ(dest, expected);
    }
}
