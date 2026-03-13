#include <gba/bios>
#include <gba/compress>
#include <gba/testing>

int main() {
    {
        static constexpr auto expected = std::array<char, 16>{1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
        static constexpr auto compressed = gba::bit_pack([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::BitUnPack(compressed.data(), dest.data(), compressed);
        gba::test.eq(dest, expected);
    }

    {
        static constexpr auto expected = std::array<char, 16>{1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};
        static constexpr auto compressed = gba::rle_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::RLUnCompWram(compressed, dest.data());
        gba::test.eq(dest, expected);
    }

    {
        static constexpr auto expected = std::string_view{"Hello, World!!"};
        static constexpr auto compressed = gba::lz77_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::LZ77UnCompWram(compressed, dest.data());
        gba::test.eq(dest, expected);
    }

    {
        static constexpr auto expected = std::string_view{"Hello, World!!"};
        static constexpr auto compressed = gba::huffman_compress([] { return expected; });

        typename decltype(compressed)::unpacked_array dest;
        gba::HuffUnComp(compressed, dest.data());
        gba::test.eq(dest, expected);
    }
    return gba::test.finish();
}
