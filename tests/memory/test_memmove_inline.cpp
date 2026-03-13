/// @file tests/memory/test_memmove_inline.cpp
/// @brief Tests for memmove.cpp inline specialisations with overlapping data.
///
/// The inline specialisations in memmove.cpp are guarded by
/// __builtin_constant_p, which only resolves to true with optimizations
/// enabled (-O1 or higher) AND when the function body is visible at the
/// call site (same TU or LTO). This test includes the memmove logic
/// directly and compiles with -O3 (via CMakeLists.txt) to exercise the
/// actual inline code paths.
///
/// The critical property being tested: the small-constant specialisation
/// (n <= 6) must load ALL source bytes into temporaries BEFORE storing
/// any of them, making it safe for overlapping regions where dest > src.
/// A naive forward copy (d[0]=s[0]; d[1]=s[1]; ...) corrupts data when
/// dest = src + 1.

#include <gba/testing>

#include <cstddef>
#include <cstdint>
#include <cstring>

// Pull in the aeabi symbols for the fallback path
extern "C" {
extern void __aeabi_memmove(void*, const void*, std::size_t);
extern void __aeabi_memmove4(void*, const void*, std::size_t);
}

// Reproduce the memmove wrapper logic in this TU so __builtin_constant_p
// can resolve with -O3 inlining. This is the EXACT logic from memmove.cpp.
// Must be always_inline so __builtin_constant_p sees the literal n from callers.
[[gnu::always_inline]]
static inline void* test_memmove(void* dest, const void* src, std::size_t n) {
    if (__builtin_constant_p(n) && n == 0) return dest;

    if (__builtin_constant_p(n) && n > 0 && n <= 6) {
        auto d = static_cast<unsigned char*>(dest);
        auto s = static_cast<const unsigned char*>(src);
        const auto t0 = s[0];
        const auto t1 = (n >= 2) ? s[1] : static_cast<unsigned char>(0);
        const auto t2 = (n >= 3) ? s[2] : static_cast<unsigned char>(0);
        const auto t3 = (n >= 4) ? s[3] : static_cast<unsigned char>(0);
        const auto t4 = (n >= 5) ? s[4] : static_cast<unsigned char>(0);
        const auto t5 = (n >= 6) ? s[5] : static_cast<unsigned char>(0);
        d[0] = t0;
        if (n >= 2) d[1] = t1;
        if (n >= 3) d[2] = t2;
        if (n >= 4) d[3] = t3;
        if (n >= 5) d[4] = t4;
        if (n >= 6) d[5] = t5;
        return dest;
    }

    if (__builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(src) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(src) & 3) == 0) {
        __aeabi_memmove4(dest, src, n);
        return dest;
    }

    __aeabi_memmove(dest, src, n);
    return dest;
}

namespace {

    alignas(8) unsigned char buf[64];

    void fill_pattern() {
        for (std::size_t i = 0; i < sizeof(buf); ++i) buf[i] = static_cast<unsigned char>(i);
    }

    // Reference memmove
    void ref_memmove(unsigned char* dst, const unsigned char* src, std::size_t n) {
        if (dst <= src) {
            for (std::size_t i = 0; i < n; ++i) dst[i] = src[i];
        } else {
            for (std::size_t i = n; i > 0; --i) dst[i - 1] = src[i - 1];
        }
    }

    // Each test_N function calls test_memmove with a LITERAL constant N so
    // __builtin_constant_p(n) resolves to true. The call is NOT through a
    // volatile function pointer -- the compiler MUST see the constant size.

    void test_backward_n1() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 1);
        test_memmove(buf + 1, buf + 0, 1);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_backward_n2() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 2);
        test_memmove(buf + 1, buf + 0, 2);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_backward_n3() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 3);
        test_memmove(buf + 1, buf + 0, 3);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_backward_n4() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 4);
        test_memmove(buf + 1, buf + 0, 4);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_backward_n5() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 5);
        test_memmove(buf + 1, buf + 0, 5);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_backward_n6() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 1, ref + 0, 6);
        test_memmove(buf + 1, buf + 0, 6);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    // Forward overlap: dest < src (should also be safe)
    void test_forward_n4() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 0, ref + 1, 4);
        test_memmove(buf + 0, buf + 1, 4);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    void test_forward_n6() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        ref_memmove(ref + 0, ref + 1, 6);
        test_memmove(buf + 0, buf + 1, 6);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    // Zero size
    void test_n0() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        test_memmove(buf + 1, buf + 0, 0);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

    // dest == src
    void test_same_ptr() {
        fill_pattern();
        unsigned char ref[64];
        std::memcpy(ref, buf, sizeof(buf));
        test_memmove(buf + 4, buf + 4, 4);
        for (std::size_t i = 0; i < sizeof(buf); ++i) {
            gba::test.eq(buf[i], ref[i]);
        }
    }

} // namespace

int main() {
    test_n0();
    test_backward_n1();
    test_backward_n2();
    test_backward_n3();
    test_backward_n4();
    test_backward_n5();
    test_backward_n6();
    test_forward_n4();
    test_forward_n6();
    test_same_ptr();

    return gba::test.finish();
}
