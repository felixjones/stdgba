#include <gba/fixed_point>

#include <mgba_test.hpp>

int main() {
    using namespace gba;
    using namespace gba::literals; // Import _fx literals
    using fix8 = fixed<int, 8>;
    using fix16 = fixed<int, 16>;

    // Test: Basic _fx literal usage
    {
        // Integer literal
        constexpr auto a = 5_fx;
        constexpr fix8 b = a;
        EXPECT_EQ(b, fix8(5));
    }

    {
        // Floating-point literal
        constexpr auto a = 3.5_fx;
        constexpr fix8 b = a;
        EXPECT_EQ(b, fix8(3.5));
    }

    // Test: Arithmetic with _fx literals
    {
        // _fx + _fx
        constexpr auto result = 1_fx + 2.3_fx;
        constexpr fix8 a = result;
        EXPECT_EQ(a, fix8(3.3));
    }

    {
        // Complex expression
        constexpr auto result = 1_fx + 2.3_fx - 0.5_fx * 2_fx;
        constexpr fix16 a = result;
        EXPECT_EQ(a, fix16(2.3)); // 1 + 2.3 - 1.0 = 2.3
    }

    // Test: Mixing _fx with numeric types
    {
        // _fx + int
        constexpr auto result = 1_fx + 2;
        constexpr fix8 a = result;
        EXPECT_EQ(a, fix8(3));
    }

    {
        // float + _fx
        constexpr auto result = 2.5f + 1.5_fx;
        constexpr fix8 a = result;
        EXPECT_EQ(a, fix8(4.0));
    }

    {
        // _fx + long long
        constexpr auto result = 3_fx + 4LL;
        constexpr fix8 a = result;
        EXPECT_EQ(a, fix8(7));
    }

    // Test: Direct assignment to different fixed types
    {
        constexpr auto lit = 3.625_fx;

        // Can assign to any fixed type
        constexpr fix8 a = lit;
        constexpr fix16 b = lit;

        EXPECT_EQ(a, fix8(3.625));
        EXPECT_EQ(b, fix16(3.625));
    }

    // Test: One-liner complex expressions
    {
        // Everything in one expression
        constexpr fix8 result = 1_fx + 2.3f + 4LL - 0.5_fx;
        EXPECT_EQ(result, fix8(6.8)); // 1 + 2.3 + 4 - 0.5 = 6.8
    }

    {
        // Division
        constexpr fix16 result = 10_fx / 4_fx;
        EXPECT_EQ(result, fix16(2.5));
    }

    // Test: Unary operators
    {
        constexpr auto pos = +3.5_fx;
        constexpr fix8 a = pos;
        EXPECT_EQ(a, fix8(3.5));
    }

    {
        constexpr auto neg = -3.5_fx;
        constexpr fix8 a = neg;
        EXPECT_EQ(a, fix8(-3.5));
    }

    {
        constexpr auto result = -(1_fx + 2_fx);
        constexpr fix8 a = result;
        EXPECT_EQ(a, fix8(-3));
    }
}
