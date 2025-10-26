#include <gba/fixed_point>
#include <mgba_test.hpp>

int main() {
    using namespace gba;
    using fix8 = fixed<int, 8>;
    using fix4 = fixed<int, 4>;
    using ufix8 = fixed<unsigned int, 8>;

    // ========================================
    // Test: Implicit conversions (safe, no precision loss)
    // ========================================
    {
        fix4 low = 1.25;   // 4 fractional bits

        // Widening conversion - safe, no precision loss
        fix8 high = low;  // OK - implicit conversion
        EXPECT_EQ(high, fix8(1.25));
    }

    {
        fixed<char, 3> tiny = 2.5;  // 3 fractional bits, char storage

        // Safe conversion: more fractional bits AND larger/equal storage
        fixed<int, 3> word = tiny;  // OK - same frac bits, larger storage
        EXPECT_EQ(word, fixed<int, 3>(2.5));

        // Safe conversion: more fractional bits
        fixed<int, 6> wider = tiny;  // OK - more fractional bits
        EXPECT_EQ(wider, fixed<int, 6>(2.5));
    }

    // ========================================
    // Test: Incompatible conversions require converters
    // ========================================
    {
        fix8 high = 3.75;  // 8 fractional bits

        // ERROR: Would lose precision - must use converter
        // fix4 low = high;  // Compile error!

        // OK: Use converter in operation
        auto low = as_narrowing(high) + fix4(0);
        EXPECT_EQ(low, fix4(3.75));
    }

    {
        fix8 a = 3.625;  // Has fine fractional detail

        // Must use converter in operation
        auto b = as_narrowing(a) + fix4(0);
        // 0.625 is exactly representable with 4 frac bits (10/16)
        EXPECT_EQ(b, fix4(3.625));
    }

    {
        // Test actual truncation - value not representable with 4 frac bits
        fix8 precise = 3.53125;  // 3 + 136/256 (8 frac bits: 1/256 precision)

        auto truncated = as_narrowing(precise) + fix4(0);
        // With 4 frac bits (1/16 precision), 3.53125 truncates to 3.5 (3 + 8/16)
        EXPECT_EQ(truncated, fix4(3.5));
    }

    // ========================================
    // Test: Converters work in operations
    // ========================================
    {
        fix8 source = 5.25;

        // Narrowing
        auto narrow = as_narrowing(source) + fix4(0);
        EXPECT_EQ(narrow, fix4(5.25));

        // Widening (though implicit would work here)
        fix4 low_val = 2.75;
        auto wide = as_widening(low_val) + fix8(0);
        EXPECT_EQ(wide, fix8(2.75));
    }

    // ========================================
    // Test: Storage type conversions via operations
    // ========================================
    {
        fixed<char, 4> small = 3.5;

        // Word storage for performance
        auto word = as_word_storage(small) + fix4(0);
        EXPECT_EQ(word, fix4(3.5));
    }

    {
        fixed<unsigned char, 3> u_val = 2.5;

        // To signed (promotes to short for safety)
        auto s_val = as_signed(u_val) + fixed<short, 3>(0);
        EXPECT_EQ(s_val, fixed<short, 3>(2.5));
    }
}
