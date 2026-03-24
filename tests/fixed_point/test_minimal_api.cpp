#include <gba/fixed_point>
#include <gba/testing>

int main() {
    using namespace gba;
    using fix8 = fixed<int, 8>;
    using fix4 = fixed<int, 4>;
    using ufix8 = fixed<unsigned int, 8>;

    // Test: fixed<> has minimal API like fundamental types

    // Construction
    {
        fix8 a;       // Default constructor
        fix8 b = 3;   // From integer
        fix8 c = 3.5; // From float (consteval only)

        gba::test.expect.eq(b, fix8(3));
        gba::test.expect.eq(c, fix8(3.5));
    }

    // Conversion to integral (explicit)
    {
        fix8 a = 3.75;
        int i = static_cast<int>(a); // Explicit cast required
        gba::test.expect.eq(i, 3);
    }

    // Operators
    {
        fix8 a = 3.5;
        fix8 b = 2.0;

        auto c = +a; // Unary +
        auto d = -a; // Unary -

        gba::test.expect.eq(c, fix8(3.5));
        gba::test.expect.eq(d, fix8(-3.5));

        auto e = a + b; // Binary +
        auto f = a - b; // Binary -
        auto g = a * b; // Binary *
        auto h = a / b; // Binary /

        gba::test.expect.eq(e, fix8(5.5));
        gba::test.expect.eq(f, fix8(1.5));
        gba::test.expect.eq(g, fix8(7.0));
        gba::test.expect.eq(h, fix8(1.75));
    }

    // Compound assignment
    {
        fix8 a = 3.5;

        a += fix8(1.0);
        gba::test.expect.eq(a, fix8(4.5));

        a -= fix8(0.5);
        gba::test.expect.eq(a, fix8(4.0));

        a *= fix8(2.0);
        gba::test.expect.eq(a, fix8(8.0));

        a /= fix8(2.0);
        gba::test.expect.eq(a, fix8(4.0));
    }

    // Compound assignment with fixed power-of-two constants should match runtime operands.
    {
        fix8 mulValue = 1.5;
        const fix8 runtimeMul = fix8(8.0);
        const auto mulByConstant = mulValue * fix8(8.0);
        const auto mulByRuntime = mulValue * runtimeMul;
        gba::test.expect.eq(mulByConstant, fix8(12.0));
        gba::test.expect.eq(mulByConstant, mulByRuntime);

        fix8 divValue = 12.0;
        const fix8 runtimeDiv = fix8(8.0);
        const auto divByConstant = divValue / fix8(8.0);
        const auto divByRuntime = divValue / runtimeDiv;
        gba::test.expect.eq(divByConstant, fix8(1.5));
        gba::test.expect.eq(divByConstant, divByRuntime);

        const fix8 runtimeNegMul = fix8(-8.0);
        const auto mulNegByConstant = mulValue * fix8(-8.0);
        const auto mulNegByRuntime = mulValue * runtimeNegMul;
        gba::test.expect.eq(mulNegByConstant, fix8(-12.0));
        gba::test.expect.eq(mulNegByConstant, mulNegByRuntime);

        const fix8 runtimeNegDiv = fix8(-8.0);
        const auto divNegByConstant = divValue / fix8(-8.0);
        const auto divNegByRuntime = divValue / runtimeNegDiv;
        gba::test.expect.eq(divNegByConstant, fix8(-1.5));
        gba::test.expect.eq(divNegByConstant, divNegByRuntime);
    }

    // Compound assignment with integers
    {
        fix8 a = 3.5;

        a += 2;
        gba::test.expect.eq(a, fix8(5.5));

        a -= 1;
        gba::test.expect.eq(a, fix8(4.5));

        a *= 2;
        gba::test.expect.eq(a, fix8(9.0));

        a /= 3;
        gba::test.expect.eq(a, fix8(3.0));
    }

    // Constant power-of-two division keeps truncation-toward-zero semantics.
    {
        const auto tinyNegative = __builtin_bit_cast(fix8, -1);
        const int runtimeDivisor = 2;

        const auto byConstant = tinyNegative / 2;
        const auto byRuntime = tinyNegative / runtimeDivisor;

        gba::test.expect.eq(byConstant, fix8(0));
        gba::test.expect.eq(byConstant, byRuntime);
    }

    // Negative power-of-two constants should match runtime signed divisors/multipliers.
    {
        fix8 mulValue = 1.5;
        const int runtimeMultiplier = -8;

        const auto mulByConstant = mulValue * -8;
        const auto mulByRuntime = mulValue * runtimeMultiplier;

        gba::test.expect.eq(mulByConstant, fix8(-12.0));
        gba::test.expect.eq(mulByConstant, mulByRuntime);

        fix8 divValue = 12.0;
        const int runtimeDivisor = -8;

        const auto divByConstant = divValue / -8;
        const auto divByRuntime = divValue / runtimeDivisor;

        gba::test.expect.eq(divByConstant, fix8(-1.5));
        gba::test.expect.eq(divByConstant, divByRuntime);
    }

    // Unsigned divide-by-power-of-two constant should match runtime divisor path.
    {
        ufix8 value = 12.0;
        const unsigned int runtimeDivisor = 8;

        const auto byConstant = value / 8u;
        const auto byRuntime = value / runtimeDivisor;

        gba::test.expect.eq(byConstant, ufix8(1.5));
        gba::test.expect.eq(byConstant, byRuntime);
    }

    // Bit shifts
    {
        fix8 a = 3.5;

        auto b = a << 1; // Left shift
        auto c = a >> 1; // Right shift

        // Shifting by 1 doubles/halves the underlying representation
        gba::test.expect.eq(b, fix8(7.0));
        gba::test.expect.eq(c, fix8(1.75));
    }

    // Test: Access to bits ONLY via __builtin_bit_cast
    {
        fix8 a = 3.5;

        // The ONLY way to get the raw bits
        auto bits = __builtin_bit_cast(int, a);

        // 3.5 with 8 fractional bits = 3.5 * 256 = 896
        gba::test.expect.eq(bits, 896);

        // Can reconstruct from bits
        auto reconstructed = __builtin_bit_cast(fix8, bits);
        gba::test.expect.eq(reconstructed, a);
    }
    return gba::test.finish();
}
