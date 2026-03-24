#include <gba/fixed_point>
#include <gba/testing>

int main() {
    using namespace gba;
    using fix8 = fixed<int, 8>;
    using fix4 = fixed<int, 4>;

    // Basic test - same types
    {
        fix8 a = 3.5;
        fix8 b = 2.0;
        auto result = a + b;
        gba::test.expect.eq(result, fix8(5.5));
    }

    // LHS converter
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_lhs(a) + b;
        gba::test.expect.eq(result, fix8(5.0));
    }

    // RHS converter
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_rhs(a) + b;
        gba::test.expect.eq(result, fix4(5.0));
    }

    // Narrowing
    {
        fix8 a = 3.5;
        fix4 b = 1.5;
        auto result = as_narrowing(a) + b;
        gba::test.expect.eq(result, fix4(5.0));
    }

    // Widening
    {
        fix4 a = 1.5;
        fix8 b = 3.5;
        auto result = as_widening(a) + b;
        gba::test.expect.eq(result, fix8(5.0));
    }

    // Converter wrappers forward integer power-of-two ops to fixed-point fast paths.
    {
        fix8 a = 1.5;
        auto mul = as_lhs(a) * 8;
        gba::test.expect.eq(mul, fix8(12.0));
    }

    // Constant and runtime divisors should match for signed tiny negative values.
    {
        const auto tinyNegative = __builtin_bit_cast(fix8, -1);
        const int runtimeDivisor = 2;

        const auto byConstant = as_lhs(tinyNegative) / 2;
        const auto byRuntime = as_lhs(tinyNegative) / runtimeDivisor;

        gba::test.expect.eq(byConstant, fix8(0));
        gba::test.expect.eq(byConstant, byRuntime);
    }

    // Negative power-of-two constants should preserve wrapper forwarding behavior.
    {
        fix8 a = 1.5;
        const int runtimeMultiplier = -8;
        const int runtimeDivisor = -8;

        const auto mulByConstant = as_lhs(a) * -8;
        const auto mulByRuntime = as_lhs(a) * runtimeMultiplier;
        gba::test.expect.eq(mulByConstant, fix8(-12.0));
        gba::test.expect.eq(mulByConstant, mulByRuntime);

        const auto divByConstant = as_lhs(fix8(12.0)) / -8;
        const auto divByRuntime = as_lhs(fix8(12.0)) / runtimeDivisor;
        gba::test.expect.eq(divByConstant, fix8(-1.5));
        gba::test.expect.eq(divByConstant, divByRuntime);
    }

    // Wrapper forwarding should also match for fixed-typed power-of-two operands.
    {
        fix8 a = 1.5;
        const fix8 runtimeMul = fix8(8.0);
        const auto mulByConstant = as_lhs(a) * fix8(8.0);
        const auto mulByRuntime = as_lhs(a) * runtimeMul;
        gba::test.expect.eq(mulByConstant, fix8(12.0));
        gba::test.expect.eq(mulByConstant, mulByRuntime);

        const fix8 runtimeNegDiv = fix8(-8.0);
        const auto divByConstant = as_lhs(fix8(12.0)) / fix8(-8.0);
        const auto divByRuntime = as_lhs(fix8(12.0)) / runtimeNegDiv;
        gba::test.expect.eq(divByConstant, fix8(-1.5));
        gba::test.expect.eq(divByConstant, divByRuntime);
    }
    return gba::test.finish();
}
