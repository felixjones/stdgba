/// @file memmove.cpp
/// @brief C standard memmove wrapper with compile-time specialisation.
///
/// Kept in a separate translation unit from the assembly __aeabi_memmove so
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
///   2. n <= 6 (any alignment):            inline load-all then store-all
///   3. aligned(4), any n:                 __aeabi_memmove4 (skip alignment)
///   4. fallback:                          __aeabi_memmove
///
/// Unlike memcpy.cpp, there is no inline ldr/str word-pair specialisation.
/// Forward ldr/str pairs are unsafe when dest > src (overlap), and proving
/// non-overlap at compile time is not reliable -- __builtin_constant_p on
/// pointer relationships resolves at the optimizer level, not as a sound
/// alias analysis.
///
/// Specialisation 2 loads ALL source bytes into temporaries before storing
/// any of them, making it safe for overlapping regions in either direction.
///
/// Uses .cpp (not .c) to avoid injecting the C language into downstream
/// targets that use stdgba as an INTERFACE library.

#include <cstddef>
#include <cstdint>

extern "C" {

extern void __aeabi_memmove(void*, const void*, std::size_t);
extern void __aeabi_memmove4(void*, const void*, std::size_t);

void* memmove(void* dest, const void* src, std::size_t n) {
    // 1. Zero-size move: compile-time elimination, no code emitted.
    if (__builtin_constant_p(n) && n == 0) return dest;

    // 2. Small constant move (1-6 bytes, any alignment): overlap-safe.
    //    Loads ALL source bytes into temporaries first, then stores them
    //    all. The compiler emits N ldrb's followed by N strb's -- safe
    //    regardless of overlap direction. Threshold of 6 bytes matches
    //    memcpy.cpp's ROM/Thumb crossover.
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

    // 3. Alignment-proven fast path: skip alignment check in the asm entry.
    //    Saves 8-16% by avoiding the eor/tst alignment logic.
    //    __aeabi_memmove4 handles overlap correctly (backward copy when
    //    needed).
    if (__builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(src) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(src) & 3) == 0) {
        __aeabi_memmove4(dest, src, n);
        return dest;
    }

    // 4. Fallback: dynamic size or unproven alignment.
    __aeabi_memmove(dest, src, n);
    return dest;
}

} // extern "C"
