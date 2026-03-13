/// @file tests/fixed_point/test_16bit_fastpath.cpp
/// @brief Tests for 16-bit fixed-point optimization.

#include <gba/fixed_point>

#include <gba/testing>

int main() {
    using namespace gba;
    using namespace gba::literals;

    // Test 16-bit multiplication (should use fast path)
    {
        using fix8_16 = fixed<short, 8>;  // 8.8 format, fits in 16 bits

        fix8_16 a = 2.5;
        fix8_16 b = 3.0;
        fix8_16 c = a * b;

        gba::test.eq(c, fix8_16(7.5));
    }

    // Test 16-bit multiplication edge cases
    {
        using fix8_16 = fixed<short, 8>;

        fix8_16 a = 0.5;
        fix8_16 b = 0.5;
        fix8_16 c = a * b;

        gba::test.eq(c, fix8_16(0.25));
    }

    // Test 16-bit with negative values
    {
        using fix8_16 = fixed<short, 8>;

        fix8_16 a = -2.0;
        fix8_16 b = 3.0;
        fix8_16 c = a * b;

        gba::test.eq(c, fix8_16(-6.0));
    }

    // Test 16-bit division
    {
        using fix8_16 = fixed<short, 8>;

        fix8_16 a = 6.0;
        fix8_16 b = 2.0;
        fix8_16 c = a / b;

        gba::test.eq(c, fix8_16(3.0));
    }

    // Test 32-bit with fewer fractional bits (uses IntermediateRep without overflow)
    {
        using fix8_32 = fixed<int, 8>;  // 24.8 format - less overflow risk

        fix8_32 a = 2.5;
        fix8_32 b = 3.0;
        fix8_32 c = a * b;

        gba::test.eq(c, fix8_32(7.5));
    }

    // Compare 16-bit and 32-bit results for same values
    {
        using fix8_16 = fixed<short, 8>;
        using fix8_32 = fixed<int, 8>;

        fix8_16 a16 = 1.5;
        fix8_16 b16 = 2.5;
        fix8_32 a32 = 1.5;
        fix8_32 b32 = 2.5;

        fix8_16 c16 = a16 * b16;
        fix8_32 c32 = a32 * b32;

        // Results should be equivalent
        gba::test.eq(bit_cast(c16), static_cast<short>(bit_cast(c32)));
    }

    // Test unsigned 16-bit
    {
        using ufix8_16 = fixed<unsigned short, 8>;

        ufix8_16 a = 2.5;
        ufix8_16 b = 3.0;
        ufix8_16 c = a * b;

        gba::test.eq(c, ufix8_16(7.5));
    }

    // Test precise<> type alias for overflow-safe 16.16 multiplication
    // Note: 16.16 format can only represent values up to ~32767, so we use smaller values
    {
        using precise16 = precise<int, 16>;  // Uses 64-bit intermediate

        precise16 a = 100.0;
        precise16 b = 50.0;
        precise16 c = a * b;

        gba::test.eq(c, precise16(5000.0));
    }

    // Test precise<> with values that would overflow regular fixed<int, 16>
    {
        using precise16 = precise<int, 16>;

        precise16 a = 2.5;
        precise16 b = 3.0;
        precise16 c = a * b;

        gba::test.eq(c, precise16(7.5));
    }

    
    return gba::test.finish();
}
