/// @file wire_word.hpp
/// @brief Packing framed stream bytes into GBA SIO hardware words.
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace gba::bits::link {

    template<typename Word, typename Source>
        requires(std::is_unsigned_v<Word> && (sizeof(Word) == 1 || sizeof(Word) == 2 || sizeof(Word) == 4))
    [[nodiscard]] constexpr Word pack_wire_word(Source&& source) noexcept {
        Word word{};
        for (std::size_t lane = 0; lane < sizeof(Word); ++lane) {
            if (const std::optional<std::byte> byte = std::forward<Source>(source)()) {
                word |= static_cast<Word>(std::to_integer<unsigned char>(*byte)) << (lane * 8);
            }
        }
        return word;
    }

    template<typename Word>
        requires(std::is_unsigned_v<Word> && (sizeof(Word) == 1 || sizeof(Word) == 2 || sizeof(Word) == 4))
    [[nodiscard]] constexpr std::array<std::byte, sizeof(Word)> unpack_wire_word(const Word word) noexcept {
        std::array<std::byte, sizeof(Word)> bytes{};
        for (std::size_t lane = 0; lane < sizeof(Word); ++lane) {
            bytes[lane] = std::byte{static_cast<unsigned char>(word >> (lane * 8))};
        }
        return bytes;
    }

} // namespace gba::bits::link
