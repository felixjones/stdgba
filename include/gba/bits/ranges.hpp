#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <ranges>
#include <vector>

namespace gba::bits {

template<std::integral T, std::ranges::input_range R> requires(std::is_trivial_v<std::ranges::range_value_t<R>> && std::is_standard_layout_v<std::ranges::range_value_t<R>>)
struct integer_view : std::ranges::view_interface<integer_view<T, R>> {
    explicit constexpr integer_view(R&& base) noexcept : m_base{&base} {}

    struct sentinel;

    class iterator {
        static constexpr auto element_size = sizeof(std::ranges::range_value_t<R>);
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        explicit constexpr iterator(R& base) noexcept : m_elementIterator(std::ranges::begin(base)), m_endIterator{std::ranges::end(base)} {}

        constexpr value_type operator*() const noexcept {
            std::array<std::byte, sizeof(value_type)> copyBuffer;

            auto offset = m_byteOffset;
            auto srcIt = m_elementIterator;
            auto dstIt = copyBuffer.begin();

            while (dstIt != copyBuffer.end() && srcIt != m_endIterator) {
                while (offset < element_size && dstIt != copyBuffer.end()) {
                    *dstIt++ = std::bit_cast<std::array<std::byte, element_size>>(*srcIt)[offset++];
                }
                ++srcIt;
                offset = 0;
            }

            std::fill(dstIt, copyBuffer.end(), std::byte{}); // Fill remaining high bytes

            return std::bit_cast<value_type>(copyBuffer);
        }

        constexpr iterator& operator++() noexcept {
            m_byteOffset += sizeof(value_type);
            while (m_byteOffset >= element_size && m_elementIterator != m_endIterator) {
                ++m_elementIterator; // Move to next element
                m_byteOffset -= element_size;
            }
            return *this;
        }

        constexpr void operator++(int) noexcept { ++*this; }

        friend constexpr bool operator==(const iterator& it, const sentinel&) noexcept {
            return it.m_elementIterator == it.m_endIterator;
        }

        friend constexpr bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
            return lhs.m_elementIterator == rhs.m_elementIterator && lhs.m_byteOffset == rhs.m_byteOffset;
        }
    private:
        std::ranges::const_iterator_t<R> m_elementIterator;
        std::ranges::const_iterator_t<R> m_endIterator;
        std::size_t m_byteOffset = 0;
    };

    struct sentinel {
        constexpr sentinel() = default;
    };

    [[nodiscard]]
    constexpr iterator begin() const {
        return iterator{*m_base};
    }

    [[nodiscard]]
    constexpr sentinel end() const {
        return sentinel{};
    }
private:
    R* m_base;
};

template<std::integral T>
constexpr auto view_integers(std::ranges::input_range auto&& r) {
    using container_type = std::remove_cvref_t<decltype(r)>;
    return integer_view<T, container_type>{std::forward<container_type>(r)};
}

template <std::ranges::view View>
constexpr auto min_max(const View& r) noexcept requires std::integral<std::ranges::range_value_t<View>> {
    using int_type = std::ranges::range_value_t<View>;

    std::optional<int_type> minValue;
    std::optional<int_type> maxValue;
    bool hasZero = false;

    for (auto&& value : r) {
        if (value == 0) {
            hasZero = true;
        } else if (!minValue || value < *minValue) {
            minValue = value;
        }

        if (!maxValue || value > *maxValue) {
            maxValue = value;
        }
    }

    struct result_type {
        int_type min;
        int_type max;
        bool has_zero;
    };

    return result_type{ minValue.value_or(0), maxValue.value_or(0), hasZero };
}

template<typename T>
concept StringViewLike = requires(T t) {
        { t.data() } -> std::convertible_to<const char*>;
        { t.size() } -> std::convertible_to<std::size_t>;
    } && requires(T t, std::size_t i) {
        { t[i] } -> std::convertible_to<char>;
    } && std::is_trivially_copyable_v<T>;

consteval auto ranges_frequency(std::ranges::input_range auto&& r) {
    using value_type = std::ranges::range_value_t<decltype(r)>;
    static constexpr auto max_size = 1 << (sizeof(value_type) * 8);

    std::array<std::pair<value_type, std::size_t>, max_size> frequencies{};
    std::size_t uniqueCount = 0;

    for (const auto& value : r) {
        auto it = std::find_if(frequencies.begin(), frequencies.begin() + uniqueCount, [&value](const auto& pair) { return pair.first == value; });
        if (it != frequencies.begin() + uniqueCount) {
            ++it->second;
        } else {
            frequencies[uniqueCount++] = {value, 1};
        }
    }

    std::ranges::sort(frequencies.begin(), frequencies.begin() + uniqueCount, [](const auto& a, const auto& b) { return a.second < b.second || (a.second == b.second && a.first < b.first); });

    return std::make_pair(frequencies, uniqueCount);
}

}
