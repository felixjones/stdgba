/**
 * @file tests/memory/test_plex.cpp
 * @brief Unit tests for plex type.
 */

#include <gba/memory>

#include "../mgba_test.hpp"

int main() {
    using namespace gba;

    // Test basic plex construction
    {
        plex<short, short> p{short{10}, short{20}};
        ASSERT_EQ(p.first, static_cast<short>(10));
        ASSERT_EQ(p.second, static_cast<short>(20));
    }

    // Test plex swap (1-element)
    {
        plex<int> a{42};
        plex<int> b{99};
        a.swap(b);
        ASSERT_EQ(a.first, 99);
        ASSERT_EQ(b.first, 42);
    }

    // Test plex swap (2-element)
    {
        plex<short, short> a{short{10}, short{20}};
        plex<short, short> b{short{30}, short{40}};
        a.swap(b);
        ASSERT_EQ(a.first, static_cast<short>(30));
        ASSERT_EQ(a.second, static_cast<short>(40));
        ASSERT_EQ(b.first, static_cast<short>(10));
        ASSERT_EQ(b.second, static_cast<short>(20));
    }

    // Test plex swap (3-element)
    {
        plex<char, char, short> a{char{1}, char{2}, short{300}};
        plex<char, char, short> b{char{3}, char{4}, short{500}};
        a.swap(b);
        ASSERT_EQ(a.first, static_cast<char>(3));
        ASSERT_EQ(a.second, static_cast<char>(4));
        ASSERT_EQ(a.third, static_cast<short>(500));
        ASSERT_EQ(b.first, static_cast<char>(1));
        ASSERT_EQ(b.second, static_cast<char>(2));
        ASSERT_EQ(b.third, static_cast<short>(300));
    }

    // Test plex swap (4-element)
    {
        plex<char, char, char, char> a{char{1}, char{2}, char{3}, char{4}};
        plex<char, char, char, char> b{char{5}, char{6}, char{7}, char{8}};
        a.swap(b);
        ASSERT_EQ(a.first, static_cast<char>(5));
        ASSERT_EQ(a.second, static_cast<char>(6));
        ASSERT_EQ(a.third, static_cast<char>(7));
        ASSERT_EQ(a.fourth, static_cast<char>(8));
        ASSERT_EQ(b.first, static_cast<char>(1));
        ASSERT_EQ(b.second, static_cast<char>(2));
        ASSERT_EQ(b.third, static_cast<char>(3));
        ASSERT_EQ(b.fourth, static_cast<char>(4));
    }

    // Test that plex fits in 32 bits (can be bit_cast to uint32_t)
    {
        plex<short, short> p{short{0x1234}, short{0x5678}};
        auto raw = __builtin_bit_cast(unsigned int, p);
        // Little-endian: first is low, second is high
        ASSERT_EQ(raw, 0x56781234u);
    }

    // Test constexpr swap
    {
        constexpr auto test_swap = [] {
            plex<short, short> a{short{10}, short{20}};
            plex<short, short> b{short{30}, short{40}};
            a.swap(b);
            return a.first == 30 && a.second == 40 && b.first == 10 && b.second == 20;
        };
        static_assert(test_swap());
        ASSERT_TRUE(test_swap());
    }

    test::finalize();
}
