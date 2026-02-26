/**
 * @file tests/string/test_strcmp.cpp
 * @brief Tests for optimized strcmp and strncmp implementations.
 *
 * Tests:
 *   - Equal strings (various lengths and alignments)
 *   - Strings differing at first byte, middle, and last byte
 *   - Strings of different lengths
 *   - Empty strings
 *   - strncmp with n < diff position, n == diff position, n > diff position
 *   - strncmp with n == 0
 *   - Word-aligned and misaligned pointer combinations
 */

#include <cstring>

#include "../mgba_test.hpp"

static int (*volatile strcmp_fn)(const char*, const char*) = &std::strcmp;
static int (*volatile strncmp_fn)(const char*, const char*, std::size_t) = &std::strncmp;

namespace {

alignas(4) char s1[512];
alignas(4) char s2[512];

void fill_same(int len) {
    for (int i = 0; i < len; ++i) {
        s1[i] = s2[i] = static_cast<char>('A' + (i % 26));
    }
    s1[len] = s2[len] = '\0';
}

int sign(int v) {
    return (v > 0) - (v < 0);
}

} // namespace

int main() {
    // Both empty
    s1[0] = s2[0] = '\0';
    ASSERT_EQ(strcmp_fn(s1, s2), 0);

    // One empty
    s1[0] = 'A'; s1[1] = '\0';
    s2[0] = '\0';
    ASSERT_EQ(sign(strcmp_fn(s1, s2)), 1);
    ASSERT_EQ(sign(strcmp_fn(s2, s1)), -1);

    // Equal strings at various lengths
    for (int len = 1; len <= 64; ++len) {
        fill_same(len);
        ASSERT_EQ(strcmp_fn(s1, s2), 0);
    }

    // Long equal strings
    for (int len : {100, 128, 255, 500}) {
        fill_same(len);
        ASSERT_EQ(strcmp_fn(s1, s2), 0);
    }

    // Differ at first byte
    fill_same(20);
    s2[0] = s1[0] + 1;
    ASSERT_EQ(sign(strcmp_fn(s1, s2)), -1);
    ASSERT_EQ(sign(strcmp_fn(s2, s1)), 1);

    // Differ at middle
    fill_same(20);
    s2[10] = s1[10] + 1;
    ASSERT_EQ(sign(strcmp_fn(s1, s2)), -1);

    // Differ at last byte
    fill_same(20);
    s2[19] = s1[19] + 1;
    ASSERT_EQ(sign(strcmp_fn(s1, s2)), -1);

    // Differ at each word boundary position (0,1,2,3 within word)
    for (int pos = 0; pos < 20; ++pos) {
        fill_same(20);
        s2[pos] = s1[pos] + 1;
        ASSERT_EQ(sign(strcmp_fn(s1, s2)), -1);
        ASSERT_EQ(sign(strcmp_fn(s2, s1)), 1);
    }

    // Different lengths (s1 shorter)
    fill_same(20);
    s1[10] = '\0';
    ASSERT_EQ(sign(strcmp_fn(s1, s2)), -1);
    ASSERT_EQ(sign(strcmp_fn(s2, s1)), 1);

    // Misaligned pointers (byte-only path)
    // Put the same content at different alignment offsets
    for (int i = 0; i < 20; ++i) {
        s1[1 + i] = s2[2 + i] = static_cast<char>('A' + (i % 26));
    }
    s1[21] = s2[22] = '\0';
    ASSERT_EQ(strcmp_fn(s1 + 1, s2 + 2), 0);  // same content, different alignment

    // Misaligned and differing
    s2[12] = s1[11] + 1;  // position 10 in both strings
    ASSERT_EQ(sign(strcmp_fn(s1 + 1, s2 + 2)), -1);

    // strncmp tests
    fill_same(20);

    // n == 0: always equal
    ASSERT_EQ(strncmp_fn(s1, s2, 0), 0);

    // n < strlen, strings equal within n
    ASSERT_EQ(strncmp_fn(s1, s2, 10), 0);

    // n == strlen
    ASSERT_EQ(strncmp_fn(s1, s2, 20), 0);

    // n > strlen
    ASSERT_EQ(strncmp_fn(s1, s2, 100), 0);

    // Differ at position 10, n > 10
    fill_same(20);
    s2[10] = s1[10] + 1;
    ASSERT_EQ(sign(strncmp_fn(s1, s2, 20)), -1);

    // Differ at position 10, n == 10 (before diff)
    ASSERT_EQ(strncmp_fn(s1, s2, 10), 0);

    // Differ at position 10, n == 11 (includes diff)
    ASSERT_EQ(sign(strncmp_fn(s1, s2, 11)), -1);

    // strncmp with n == 1
    fill_same(20);
    ASSERT_EQ(strncmp_fn(s1, s2, 1), 0);
    s2[0] = s1[0] + 1;
    ASSERT_EQ(sign(strncmp_fn(s1, s2, 1)), -1);

    // strncmp various lengths
    for (int len = 1; len <= 64; ++len) {
        fill_same(len);
        ASSERT_EQ(strncmp_fn(s1, s2, len), 0);
        ASSERT_EQ(strncmp_fn(s1, s2, len + 1), 0);
    }

    test::exit(test::failures());
    __builtin_unreachable();
}
