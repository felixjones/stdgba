/**
 * @file test_diff_filter.cpp
 * @brief Tests for compile-time differential filtering and BIOS unfilter.
 */
#include <gba/bios>
#include <gba/filter>

#include <array>

#include <mgba_test.hpp>

// Test data with gradual changes (filters well with differential)
static constexpr auto test_data = std::array<unsigned char, 64>{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
};

// Filter at compile time
static constexpr auto filtered = gba::diff_filter<1>([] { return test_data; });

int main() {
    // Verify header is correct
    EXPECT_EQ(filtered.comp_algo, gba::comp_algo_diff);
    EXPECT_EQ(filtered.src_len, 1u); // 8-bit differential
    EXPECT_EQ(filtered.dst_len, test_data.size());

    // Unfilter at runtime using BIOS
    alignas(4) std::array<unsigned char, 64> unfiltered{};
    gba::Diff8bitUnFilterWram(filtered, unfiltered.data());

    // Verify unfiltered data matches original
    for (std::size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(unfiltered[i], test_data[i]);
    }
}
