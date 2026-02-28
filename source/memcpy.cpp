/// @file memcpy.cpp
/// @brief C standard memcpy wrapper with compile-time specialisation.
///
/// Kept in a separate translation unit from the assembly __aeabi_memcpy so
/// that link-time optimisation can see the wrapper and inline it at call
/// sites.
///
/// Each specialisation is a flat, top-level guard where every term is
/// resolved purely at compile time by __builtin_constant_p. When a guard
/// is true, the body executes directly with no runtime branches. When
/// false, the entire if-body is dead code -- nothing is emitted.
///
/// Specialisations (checked in order):
///   1. n == 0:                            eliminated entirely
///   2. n % 4 == 0, n < 64, aligned(4):   inline ldr/str pairs
///   3. n <= 6 (any alignment):            inline byte copies
///   4. aligned(4), any n:                 __aeabi_memcpy4 (skip alignment check)
///   5. fallback:                          __aeabi_memcpy
///
/// Uses .cpp (not .c) to avoid injecting the C language into downstream
/// targets that use stdgba as an INTERFACE library -- a .c INTERFACE source
/// would cause CMake to generate a C precompiled header for targets that
/// only expect C++.

#include <cstddef>
#include <cstdint>

extern "C" {

extern void __aeabi_memcpy(void*, const void*, std::size_t);
extern void __aeabi_memcpy4(void*, const void*, std::size_t);

void* memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) {
    // 1. Zero-size copy: compile-time elimination, no code emitted.
    if (__builtin_constant_p(n) && n == 0) return dest;

    // 2. Word-aligned, word-multiple (4-60 bytes): inline ldr/str pairs.
    //    Eliminates ~25+ cycles of call overhead + alignment detection.
    //    Cap of 64 (exclusive) is the ROM/Thumb crossover point: at 60
    //    bytes inline is still 12% faster, at 64 the bulk ldmia/stmia
    //    in __aeabi_memcpy wins. From IWRAM/ARM inline wins up to ~256.
    //    Each term is compile-time: __builtin_constant_p on (ptr & 3)
    //    is true only when GCC can prove alignment from the call site
    //    (struct copies, alignas buffers, stack variables). When it
    //    cannot, the entire guard is false and this block is dead code.
    //
    //    Uses plain `if`, not `if constexpr`: __builtin_constant_p is
    //    resolved by the optimizer after inlining, not by the compiler's
    //    constexpr evaluator. `if constexpr` would always see false and
    //    discard these specialisations entirely.
    if (__builtin_constant_p(n) && n % 4 == 0 && n > 0 && n < 64 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(src) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(src) & 3) == 0) {
        std::uint32_t tmp;
        asm volatile(".set i, 0\n"
                     ".rept %c[words]\n"
                     "ldr %[tmp], [%[s], #i]\n"
                     "str %[tmp], [%[d], #i]\n"
                     ".set i, i + 4\n"
                     ".endr"
                     : [tmp] "=&l"(tmp)
                     : [d] "l"(dest), [s] "l"(src), [words] "i"(n / 4)
                     : "memory");
        return dest;
    }

    // 3. Small constant copy (1-6 bytes, any alignment): inline byte copies.
    //    Benchmarked crossover: inline ldrb/strb wins up to 6 bytes from
    //    ROM/Thumb (-O3). At 7 bytes the __aeabi_memcpy call is faster.
    if (__builtin_constant_p(n) && n > 0 && n <= 6) {
        auto d = static_cast<unsigned char*>(dest);
        auto s = static_cast<const unsigned char*>(src);
        d[0] = s[0];
        if (n >= 2) d[1] = s[1];
        if (n >= 3) d[2] = s[2];
        if (n >= 4) d[3] = s[3];
        if (n >= 5) d[4] = s[4];
        if (n >= 6) d[5] = s[5];
        return dest;
    }

    // 4. Alignment-proven fast path: skip alignment check in the asm entry.
    //    Saves 8-16% by avoiding the eor/tst/rsbs/movs alignment logic.
    //    Both pointers must be provably word-aligned (struct copies,
    //    alignas buffers, stack variables).
    if (__builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(src) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(src) & 3) == 0) {
        __aeabi_memcpy4(dest, src, n);
        return dest;
    }

    // 5. Fallback: dynamic size or unproven alignment.
    __aeabi_memcpy(dest, src, n);
    return dest;
}

} // extern "C"
