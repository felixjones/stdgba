#include <gba/fixed_point>

#include <mgba_test.hpp>

int main() {
    using namespace gba;
    using fix8 = fixed<int, 8>;
    using fix4 = fixed<int, 4>;

    // Basic test - same types
    {
        fix8 a = 3.5;
        fix8 b = 2.0;
        auto result = a + b;
        EXPECT_EQ(result, fix8(5.5));
    }

    // LHS converter
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_lhs(a) + b;
        EXPECT_EQ(result, fix8(5.0));
    }

    // RHS converter
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_rhs(a) + b;
        EXPECT_EQ(result, fix4(5.0));
    }

    // Narrowing
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_narrowing(a) + b;
        EXPECT_EQ(result, fix4(5.0));
    }

    // Widening
    {
        fix4 a = 1.5;
        fix8 b = 3.5;
        auto result = as_widening(a) + b;
        EXPECT_EQ(result, fix8(5.0));
    }
}
