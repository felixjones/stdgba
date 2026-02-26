/**
 * @file tests/string/test_strchr.cpp
 * @brief Tests for optimized strchr and strrchr implementations.
 */

#include <cstring>
#include <cstdint>

#include "../mgba_test.hpp"

extern "C" {
    char* strchr(const char*, int);
    char* strrchr(const char*, int);
}

static char* (*volatile strchr_fn)(const char*, int) = &strchr;
static char* (*volatile strrchr_fn)(const char*, int) = &strrchr;

namespace {

alignas(4) char buf[256];

void fill_str(const char* s) {
    std::size_t i = 0;
    while (s[i]) { buf[i] = s[i]; ++i; }
    buf[i] = '\0';
}

std::uintptr_t addr(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }

} // namespace

int main() {
    // strchr: found at various positions
    fill_str("Hello, World!");
    ASSERT_EQ(addr(strchr_fn(buf, 'H')), addr(&buf[0]));
    ASSERT_EQ(addr(strchr_fn(buf, 'o')), addr(&buf[4]));
    ASSERT_EQ(addr(strchr_fn(buf, '!')), addr(&buf[12]));

    // strchr: not found
    ASSERT_EQ(addr(strchr_fn(buf, 'Z')), addr(nullptr));

    // strchr: search for null terminator
    ASSERT_EQ(addr(strchr_fn(buf, '\0')), addr(&buf[13]));

    // strchr: empty string
    buf[0] = '\0';
    ASSERT_EQ(addr(strchr_fn(buf, 'A')), addr(nullptr));
    ASSERT_EQ(addr(strchr_fn(buf, '\0')), addr(&buf[0]));

    // strchr: single char string
    buf[0] = 'X'; buf[1] = '\0';
    ASSERT_EQ(addr(strchr_fn(buf, 'X')), addr(&buf[0]));
    ASSERT_EQ(addr(strchr_fn(buf, 'Y')), addr(nullptr));

    // strrchr: last occurrence
    fill_str("abcabc");
    ASSERT_EQ(addr(strrchr_fn(buf, 'a')), addr(&buf[3]));
    ASSERT_EQ(addr(strrchr_fn(buf, 'b')), addr(&buf[4]));
    ASSERT_EQ(addr(strrchr_fn(buf, 'c')), addr(&buf[5]));

    // strrchr: single occurrence
    fill_str("abcdef");
    ASSERT_EQ(addr(strrchr_fn(buf, 'a')), addr(&buf[0]));
    ASSERT_EQ(addr(strrchr_fn(buf, 'f')), addr(&buf[5]));

    // strrchr: not found
    ASSERT_EQ(addr(strrchr_fn(buf, 'z')), addr(nullptr));

    // strrchr: search for null terminator
    ASSERT_EQ(addr(strrchr_fn(buf, '\0')), addr(&buf[6]));

    // strrchr: empty string
    buf[0] = '\0';
    ASSERT_EQ(addr(strrchr_fn(buf, 'A')), addr(nullptr));
    ASSERT_EQ(addr(strrchr_fn(buf, '\0')), addr(&buf[0]));

    // Various alignments
    for (int off = 0; off < 4; ++off) {
        for (int i = 0; i < 10; ++i) {
            buf[off + i] = static_cast<char>('a' + i);
        }
        buf[off + 10] = '\0';
        ASSERT_EQ(addr(strchr_fn(&buf[off], 'a')), addr(&buf[off]));
        ASSERT_EQ(addr(strchr_fn(&buf[off], 'j')), addr(&buf[off + 9]));
        ASSERT_EQ(addr(strrchr_fn(&buf[off], 'a')), addr(&buf[off]));
        ASSERT_EQ(addr(strrchr_fn(&buf[off], 'j')), addr(&buf[off + 9]));
    }

    test::exit(test::failures());
    __builtin_unreachable();
}
