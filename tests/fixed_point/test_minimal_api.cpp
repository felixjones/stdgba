#include <gba/fixed_point>
#include <mgba_test.hpp>

int main() {
    using namespace gba;
    using fix8 = fixed<int, 8>;
    using fix4 = fixed<int, 4>;
    using ufix8 = fixed<unsigned int, 8>;

    // ========================================
    // Test: fixed<> has minimal API like fundamental types
    // ========================================

    // Construction
    {
        fix8 a;           // Default constructor
        fix8 b = 3;       // From integer
        fix8 c = 3.5;     // From float (consteval only)

        EXPECT_EQ(b, fix8(3));
        EXPECT_EQ(c, fix8(3.5));
    }

    // Conversion to integral (explicit)
    {
        fix8 a = 3.75;
        int i = static_cast<int>(a);  // Explicit cast required
        EXPECT_EQ(i, 3);
    }

    // Operators
    {
        fix8 a = 3.5;
        fix8 b = 2.0;

        auto c = +a;  // Unary +
        auto d = -a;  // Unary -

        EXPECT_EQ(c, fix8(3.5));
        EXPECT_EQ(d, fix8(-3.5));

        auto e = a + b;  // Binary +
        auto f = a - b;  // Binary -
        auto g = a * b;  // Binary *
        auto h = a / b;  // Binary /

        EXPECT_EQ(e, fix8(5.5));
        EXPECT_EQ(f, fix8(1.5));
        EXPECT_EQ(g, fix8(7.0));
        EXPECT_EQ(h, fix8(1.75));
    }

    // Compound assignment
    {
        fix8 a = 3.5;

        a += fix8(1.0);
        EXPECT_EQ(a, fix8(4.5));

        a -= fix8(0.5);
        EXPECT_EQ(a, fix8(4.0));

        a *= fix8(2.0);
        EXPECT_EQ(a, fix8(8.0));

        a /= fix8(2.0);
        EXPECT_EQ(a, fix8(4.0));
    }

    // Compound assignment with integers
    {
        fix8 a = 3.5;

        a += 2;
        EXPECT_EQ(a, fix8(5.5));

        a -= 1;
        EXPECT_EQ(a, fix8(4.5));

        a *= 2;
        EXPECT_EQ(a, fix8(9.0));

        a /= 3;
        EXPECT_EQ(a, fix8(3.0));
    }

    // Bit shifts
    {
        fix8 a = 3.5;

        auto b = a << 1;  // Left shift
        auto c = a >> 1;  // Right shift

        // Shifting by 1 doubles/halves the underlying representation
        EXPECT_EQ(b, fix8(7.0));
        EXPECT_EQ(c, fix8(1.75));
    }

    // ========================================
    // Test: Access to bits ONLY via __builtin_bit_cast
    // ========================================
    {
        fix8 a = 3.5;

        // The ONLY way to get the raw bits
        auto bits = __builtin_bit_cast(int, a);

        // 3.5 with 8 fractional bits = 3.5 * 256 = 896
        EXPECT_EQ(bits, 896);

        // Can reconstruct from bits
        auto reconstructed = __builtin_bit_cast(fix8, bits);
        EXPECT_EQ(reconstructed, a);
    }
}
