/**
 * @file memset.cpp
 * @brief C standard memset wrapper with compile-time specialisation.
 *
 * Kept in a separate translation unit from the assembly __aeabi_memset so
 * that link-time optimisation can see the wrapper and inline it at call
 * sites.
 *
 * Each specialisation is a flat, top-level guard where every term is
 * resolved purely at compile time by __builtin_constant_p. When a guard
 * is true, the body executes directly with no runtime branches. When
 * false, the entire if-body is dead code -- nothing is emitted.
 *
 * Specialisations (checked in order):
 *   1. n == 0:                            eliminated entirely
 *   2. n % 4 == 0, n < 64, aligned(4):   inline str sequence
 *   3. n <= 12 (any alignment):           inline byte stores
 *   4. aligned(4), any n:                 __aeabi_memset4 (skip alignment check)
 *   5. fallback:                          __aeabi_memset
 *
 * AEABI memset has a different parameter order from C memset:
 *   __aeabi_memset(void *dest, size_t n, int c)
 *   memset(void *dest, int c, size_t n)
 *
 * Uses .cpp (not .c) to avoid injecting the C language into downstream
 * targets that use stdgba as an INTERFACE library.
 */

#include <cstddef>
#include <cstdint>

extern "C" {

extern void __aeabi_memset(void *, std::size_t, int);
extern void __aeabi_memset4(void *, std::size_t, int);

void *memset(void *dest, int c, std::size_t n) {
    // 1. Zero-size fill: compile-time elimination, no code emitted.
    if (__builtin_constant_p(n) && n == 0)
        return dest;

    // 2. Word-aligned, word-multiple (4-60 bytes): inline str sequence.
    //    Eliminates ~25+ cycles of call overhead + byte broadcast + alignment.
    //    Cap of 64 (exclusive) is the ROM/Thumb crossover point: at 60
    //    bytes inline is still 5% faster, at 64 the bulk stmia in
    //    __aeabi_memset4 wins by 15% (exact 2x32-byte block, fast exit).
    //    From IWRAM/ARM inline wins up to ~316 bytes (memset only needs
    //    str, not ldr+str like memcpy, so it stays competitive longer),
    //    but the cap targets the common ROM/Thumb caller.
    //    Each term is compile-time: __builtin_constant_p on (ptr & 3)
    //    is true only when GCC can prove alignment from the call site
    //    (struct clears, alignas buffers, stack variables). When it
    //    cannot, the entire guard is false and this block is dead code.
    if (__builtin_constant_p(n) && n % 4 == 0 && n > 0 && n < 64 &&
        __builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0 &&
        __builtin_constant_p(c))
    {
        const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(c));
        const auto word = byte | (byte << 8) | (byte << 16) | (byte << 24);
        asm volatile(
            ".set i, 0\n"
            ".rept %c[words]\n"
            "str %[val], [%[d], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            :: [d]"l"(dest), [val]"l"(word), [words]"i"(n / 4)
            : "memory"
        );
        return dest;
    }

    // 3. Small constant fill (1-12 bytes, any alignment): inline byte stores.
    //    Benchmarked crossover: inline strb wins up to 12 bytes from
    //    ROM/Thumb (-O3). At 14 bytes the __aeabi_memset call is faster.
    //    memset's byte path is cheaper than memcpy's (strb vs ldrb+strb),
    //    so the threshold is higher (12 vs 6).
    if (__builtin_constant_p(n) && n > 0 && n <= 12) {
        auto d = static_cast<unsigned char*>(dest);
        const auto byte = static_cast<unsigned char>(c);
        d[0] = byte;
        if (n >= 2) d[1] = byte;
        if (n >= 3) d[2] = byte;
        if (n >= 4) d[3] = byte;
        if (n >= 5) d[4] = byte;
        if (n >= 6) d[5] = byte;
        if (n >= 7) d[6] = byte;
        if (n >= 8) d[7] = byte;
        if (n >= 9) d[8] = byte;
        if (n >= 10) d[9] = byte;
        if (n >= 11) d[10] = byte;
        if (n >= 12) d[11] = byte;
        return dest;
    }

    // 4. Alignment-proven fast path: skip alignment check + broadcast in
    //    the generic entry. Saves 8-13% by calling __aeabi_memset4 directly.
    //    Note the parameter order swap: AEABI is (dest, n, c).
    if (__builtin_constant_p((reinterpret_cast<std::uintptr_t>(dest) & 3)) &&
        (reinterpret_cast<std::uintptr_t>(dest) & 3) == 0)
    {
        __aeabi_memset4(dest, n, c);
        return dest;
    }

    // 5. Fallback: dynamic size or unproven alignment.
    //    Note the parameter order swap: AEABI is (dest, n, c).
    __aeabi_memset(dest, n, c);
    return dest;
}

} // extern "C"
