/// @file tests/format/test_format.cpp
/// @brief Unit tests for format module using mgba test framework.

#include <gba/format>
#include <gba/testing>

using namespace gba::format;

int main() {
    // Basic literal formatting
    {
        constexpr auto fmt = "Hello, World!"_fmt;
        char buf[64];
        auto len = fmt.to(buf);
        gba::test.eq(len, 13u);
        gba::test.eq(buf[0], 'H');
        gba::test.eq(buf[12], '!');
        gba::test.eq(buf[13], '\0');
    }

    // Single placeholder
    {
        constexpr auto fmt = "Value: {x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 42);
        gba::test.eq(len, 9u);
        gba::test.eq(buf[7], '4');
        gba::test.eq(buf[8], '2');
    }

    // Multiple placeholders
    {
        constexpr auto fmt = "{a}/{b}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 10, "b"_arg = 20);
        gba::test.eq(len, 5u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[2], '/');
        gba::test.eq(buf[3], '2');
    }

    // Function call syntax
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg(99));
        gba::test.eq(buf[0], '9');
        gba::test.eq(buf[1], '9');
    }

    // String argument
    {
        constexpr auto fmt = "Hello, {name}!"_fmt;
        char buf[64];
        fmt.to(buf, "name"_arg = "World");
        gba::test.eq(buf[0], 'H');
        gba::test.eq(buf[7], 'W');
    }

    // Hex formatting
    {
        constexpr auto fmt = "{x:x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 255);
        gba::test.eq(buf[0], 'f');
        gba::test.eq(buf[1], 'f');
    }

    {
        constexpr auto fmt = "{x:X}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 0xABCD);
        gba::test.eq(buf[0], 'A');
        gba::test.eq(buf[1], 'B');
        gba::test.eq(buf[2], 'C');
        gba::test.eq(buf[3], 'D');
    }

    // Negative numbers
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = -42);
        gba::test.eq(buf[0], '-');
        gba::test.eq(buf[1], '4');
        gba::test.eq(buf[2], '2');
    }

    // MSB-first digits (typewriter mode)
    {
        constexpr auto fmt = "{x}"_fmt;
        auto gen = fmt.generator("x"_arg = 123);

        auto c1 = gen();
        gba::test.is_true(c1.has_value());
        gba::test.eq(*c1, '1');

        auto c2 = gen();
        gba::test.eq(*c2, '2');

        auto c3 = gen();
        gba::test.eq(*c3, '3');
    }

    // Generator basic
    {
        constexpr auto fmt = "Hi"_fmt;
        auto gen = fmt.generator();

        auto c1 = gen();
        gba::test.is_true(c1.has_value());
        gba::test.eq(*c1, 'H');

        auto c2 = gen();
        gba::test.is_true(c2.has_value());
        gba::test.eq(*c2, 'i');

        auto c3 = gen();
        gba::test.is_false(c3.has_value());
        gba::test.is_true(gen.done());
    }

    // until_break() for line wrapping
    {
        constexpr auto fmt = "Hello World"_fmt;
        auto gen = fmt.generator();

        gba::test.eq(gen.until_break(), 5u);

        for (int i = 0; i < 5; ++i) gen();

        gba::test.eq(gen.until_break(), 0u);
    }

    // Generator reset
    {
        constexpr auto fmt = "AB"_fmt;
        auto gen = fmt.generator();

        gen();
        gen();
        gen();
        gba::test.is_true(gen.done());

        gen.reset();
        gba::test.is_false(gen.done());

        auto c = gen();
        gba::test.eq(*c, 'A');
    }

    // Escaped braces
    {
        constexpr auto fmt = "{{x}}"_fmt;
        char buf[64];
        fmt.to(buf);
        gba::test.eq(buf[0], '{');
        gba::test.eq(buf[1], 'x');
        gba::test.eq(buf[2], '}');
    }

    // Zero
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 0);
        gba::test.eq(buf[0], '0');
        gba::test.eq(buf[1], '\0');
    }

    // Large number
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 1234567890);
        gba::test.eq(len, 10u);
    }

    // Constexpr formatting (to_static)
    {
        constexpr auto msg = "Answer: {x}"_fmt.to_static<32>("x"_arg = 42);
        gba::test.eq(msg[0], 'A');
        gba::test.eq(msg[8], '4');
        gba::test.eq(msg[9], '2');
    }

    // User workspace
    {
        char workspace[64];
        constexpr auto fmt = "{x}"_fmt;
        auto gen = fmt.generator_with(external_workspace{workspace, sizeof(workspace)}, "x"_arg = 42);
        auto c = gen();
        gba::test.eq(*c, '4');
    }

    // until_break() at word boundaries
    {
        constexpr auto fmt = "A BC DEF"_fmt;
        auto gen = fmt.generator();

        gba::test.eq(gen.until_break(), 1u); // "A"
        gen();
        gba::test.eq(gen.until_break(), 0u); // at space
        gen();
        gba::test.eq(gen.until_break(), 2u); // "BC"
    }

    return gba::test.finish();
}
