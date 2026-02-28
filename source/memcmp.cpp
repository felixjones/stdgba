/// @file memcmp.cpp
/// @brief C standard memcmp / bcmp wrappers with compile-time specialisation.
///
/// Kept in a separate translation unit from the assembly __stdgba_memcmp so
/// that link-time optimisation can see the wrapper and inline it at call
/// sites.
///
/// Each specialisation is a flat, top-level guard where every term is
/// resolved purely at compile time by __builtin_constant_p. When a guard
/// is true, the body executes directly with no runtime branches. When
/// false, the entire if-body is dead code -- nothing is emitted.
///
/// Specialisations for memcmp (checked in order):
///   1. n == 0:                             return 0 (eliminated entirely)
///   2. n == 1:                             inline single-byte compare
///   3. n % 4 == 0, n < 20, aligned(4):    inline ldr + XOR word compare
///   4. n == 2 (any alignment):             inline byte compare
///   5. fallback:                           __stdgba_memcmp
///
/// bcmp mirrors memcmp but returns 0 (equal) or 1 (not equal). GCC lowers
/// memcmp(...) == 0 to bcmp at -O2+, so the bcmp wrapper is equally
/// important.
///
/// Uses .cpp (not .c) to avoid injecting the C language into downstream
/// targets that use stdgba as an INTERFACE library.

#include <cstddef>
#include <cstdint>

extern "C" {

extern int __stdgba_memcmp(const void*, const void*, std::size_t);
extern int __stdgba_bcmp(const void*, const void*, std::size_t);

int memcmp(const void* s1, const void* s2, std::size_t n) {
    // 1. Zero-size compare: compile-time elimination.
    if (__builtin_constant_p(n) && n == 0) return 0;

    // 2. Single byte: inline compare.
    //    Saves all entry overhead (alignment check, byte loop setup).
    //    The cast + subtract produces the correct memcmp sign convention.
    if (__builtin_constant_p(n) && n == 1) {
        const auto a = *static_cast<const unsigned char*>(s1);
        const auto b = *static_cast<const unsigned char*>(s2);
        return static_cast<int>(a) - static_cast<int>(b);
    }

    // 3. Word-aligned, word-multiple (4-16 bytes): inline word XOR compare.
    //    Loads one word from each side, XORs to check equality, and if
    //    different, falls back to byte scan. Cap of 20 (exclusive):
    //    benchmarked crossover at N=16 inline is 5% faster, at N=20
    //    the __stdgba_memcmp computed-jump word path wins.
    //
    //    Each term is compile-time: __builtin_constant_p on (ptr & 3)
    //    is true only when GCC can prove alignment from the call site.
    //    When it cannot, the entire guard is false and this block is
    //    dead code.
    if (__builtin_constant_p(n) && n % 4 == 0 && n > 0 && n < 20 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(s1) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(s1) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(s2) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(s2) & 3) == 0) {
        auto p1 = static_cast<const std::uint32_t*>(s1);
        auto p2 = static_cast<const std::uint32_t*>(s2);
        for (std::size_t i = 0; i < n / 4; ++i) {
            const auto w1 = p1[i];
            const auto w2 = p2[i];
            if (w1 != w2) {
                // Byte-scan the differing word (little-endian).
                auto a = reinterpret_cast<const unsigned char*>(&p1[i]);
                auto b = reinterpret_cast<const unsigned char*>(&p2[i]);
                for (int j = 0; j < 4; ++j) {
                    if (a[j] != b[j]) return static_cast<int>(a[j]) - static_cast<int>(b[j]);
                }
            }
        }
        return 0;
    }

    // 4. Small constant compare (n == 2, any alignment): inline bytes.
    //    Benchmarked crossover: inline ldrb pairs win at N=1 (30%) and
    //    N=2 (13%); at N=3 the __stdgba_memcmp call is faster. N=1 is
    //    already handled above, so this only catches N=2.
    if (__builtin_constant_p(n) && n == 2) {
        auto a = static_cast<const unsigned char*>(s1);
        auto b = static_cast<const unsigned char*>(s2);
        for (std::size_t i = 0; i < n; ++i) {
            if (a[i] != b[i]) return static_cast<int>(a[i]) - static_cast<int>(b[i]);
        }
        return 0;
    }

    // 5. Fallback: dynamic size or unproven alignment.
    return __stdgba_memcmp(s1, s2, n);
}

int bcmp(const void* s1, const void* s2, std::size_t n) {
    // 1. Zero-size: always equal.
    if (__builtin_constant_p(n) && n == 0) return 0;

    // 2. Single byte: inline compare.
    if (__builtin_constant_p(n) && n == 1) {
        return *static_cast<const unsigned char*>(s1) != *static_cast<const unsigned char*>(s2);
    }

    // 3. Word-aligned, word-multiple (4-16 bytes): inline word XOR.
    //    bcmp only needs equal/not-equal, so a single OR-reduce of all
    //    XOR results suffices -- no byte scan on mismatch.
    //    Same cap as memcmp: inline wins up to N=16, call wins at N=20.
    if (__builtin_constant_p(n) && n % 4 == 0 && n > 0 && n < 20 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(s1) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(s1) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(s2) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(s2) & 3) == 0) {
        auto p1 = static_cast<const std::uint32_t*>(s1);
        auto p2 = static_cast<const std::uint32_t*>(s2);
        std::uint32_t diff = 0;
        for (std::size_t i = 0; i < n / 4; ++i) diff |= p1[i] ^ p2[i];
        return diff != 0;
    }

    // 4. Small constant compare (n == 2, any alignment): inline bytes.
    //    Same crossover as memcmp: inline wins at N=1-2, call wins at N=3+.
    if (__builtin_constant_p(n) && n == 2) {
        auto a = static_cast<const unsigned char*>(s1);
        auto b = static_cast<const unsigned char*>(s2);
        for (std::size_t i = 0; i < n; ++i) {
            if (a[i] != b[i]) return 1;
        }
        return 0;
    }

    // 5. Fallback.
    return __stdgba_bcmp(s1, s2, n);
}

} // extern "C"
