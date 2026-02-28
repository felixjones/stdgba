/// @file tests/string/test_strlen.cpp
/// @brief Tests for optimized strlen and strnlen implementations.
///
/// Tests all code paths:
/// - Empty string
/// - Short strings (1-3 bytes, alignment scan only)
/// - Strings at all 4 alignment offsets (0-3)
/// - Medium strings (crossing word boundaries)
/// - Long strings (exercising word-at-a-time loop)
/// - Null byte at each position within a word
/// - strnlen with maxlen < strlen, maxlen == strlen, maxlen > strlen
/// - strnlen with maxlen == 0

#include <cstring>

#include <mgba_test.hpp>

// Prevent compiler from replacing with builtins
static std::size_t (*volatile strlen_fn)(const char*) = &std::strlen;
static std::size_t (*volatile strnlen_fn)(const char*, std::size_t) = &strnlen;

namespace {

// Buffer large enough for all test cases, word-aligned
alignas(4) char buf[512];

void fill_pattern(std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        buf[i] = static_cast<char>('A' + (i % 26));
    }
    buf[len] = '\0';
}

} // namespace

int main() {
    // Empty string (all 4 alignment offsets)
    for (int off = 0; off < 4; ++off) {
        buf[off] = '\0';
        ASSERT_EQ(strlen_fn(&buf[off]), static_cast<std::size_t>(0));
    }

    // Single character
    for (int off = 0; off < 4; ++off) {
        buf[off] = 'X';
        buf[off + 1] = '\0';
        ASSERT_EQ(strlen_fn(&buf[off]), static_cast<std::size_t>(1));
    }

    // Short strings (1-7 bytes) at all alignments
    for (int off = 0; off < 4; ++off) {
        for (int len = 1; len <= 7; ++len) {
            for (int i = 0; i < len; ++i) {
                buf[off + i] = static_cast<char>('a' + i);
            }
            buf[off + len] = '\0';
            ASSERT_EQ(strlen_fn(&buf[off]), static_cast<std::size_t>(len));
        }
    }

    // Null byte at each position within a word (testing word-at-a-time detection)
    // Use word-aligned base
    for (int pos = 0; pos < 4; ++pos) {
        // Fill 8 bytes with non-null, then place null at position
        for (int i = 0; i < 8; ++i) buf[i] = 'Z';
        buf[pos] = '\0';
        ASSERT_EQ(strlen_fn(buf), static_cast<std::size_t>(pos));
    }

    // Medium strings (8-32 bytes) at alignment 0
    for (int len = 8; len <= 32; ++len) {
        fill_pattern(len);
        ASSERT_EQ(strlen_fn(buf), static_cast<std::size_t>(len));
    }

    // Medium strings at various alignments
    for (int off = 0; off < 4; ++off) {
        for (int len = 4; len <= 20; len += 4) {
            fill_pattern(off + len);
            buf[off + len] = '\0';
            ASSERT_EQ(strlen_fn(&buf[off]), static_cast<std::size_t>(len));
        }
    }

    // Long strings
    for (int len : {64, 100, 128, 255, 500}) {
        fill_pattern(len);
        ASSERT_EQ(strlen_fn(buf), static_cast<std::size_t>(len));
    }

    // Null at specific word positions in long string
    for (int wordpos = 0; wordpos < 4; ++wordpos) {
        fill_pattern(100);
        buf[60 + wordpos] = '\0';
        ASSERT_EQ(strlen_fn(buf), static_cast<std::size_t>(60 + wordpos));
    }

    // strnlen tests
    fill_pattern(100);

    // maxlen > strlen
    ASSERT_EQ(strnlen_fn(buf, 200), static_cast<std::size_t>(100));

    // maxlen == strlen
    ASSERT_EQ(strnlen_fn(buf, 100), static_cast<std::size_t>(100));

    // maxlen < strlen
    ASSERT_EQ(strnlen_fn(buf, 50), static_cast<std::size_t>(50));
    ASSERT_EQ(strnlen_fn(buf, 1), static_cast<std::size_t>(1));

    // maxlen == 0
    ASSERT_EQ(strnlen_fn(buf, 0), static_cast<std::size_t>(0));

    // strnlen with empty string
    buf[0] = '\0';
    ASSERT_EQ(strnlen_fn(buf, 100), static_cast<std::size_t>(0));
    ASSERT_EQ(strnlen_fn(buf, 0), static_cast<std::size_t>(0));

    // strnlen at various alignments
    for (int off = 0; off < 4; ++off) {
        for (int len = 1; len <= 16; ++len) {
            fill_pattern(off + len);
            buf[off + len] = '\0';
            // maxlen > len
            ASSERT_EQ(strnlen_fn(&buf[off], len + 10), static_cast<std::size_t>(len));
            // maxlen == len
            ASSERT_EQ(strnlen_fn(&buf[off], len), static_cast<std::size_t>(len));
            // maxlen < len (when len > 1)
            if (len > 1) {
                ASSERT_EQ(strnlen_fn(&buf[off], len - 1), static_cast<std::size_t>(len - 1));
            }
        }
    }

    test::exit(test::failures());
    __builtin_unreachable();
}
