/**
 * @file tests/format/test_format.cpp
 * @brief Unit tests for format module using mgba test framework.
 */

#include <gba/format>

#include "../mgba_test.hpp"

using namespace gba::format;
using namespace gba::format::literals;

int main() {
    // Basic literal formatting
    {
        constexpr auto fmt = "Hello, World!"_fmt;
        char buf[64];
        auto len = fmt.to(buf);
        ASSERT_EQ(len, 13u);
        ASSERT_EQ(buf[0], 'H');
        ASSERT_EQ(buf[12], '!');
        ASSERT_EQ(buf[13], '\0');
    }

    // Single placeholder
    {
        constexpr auto fmt = "Value: {x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 42);
        ASSERT_EQ(len, 9u);
        ASSERT_EQ(buf[7], '4');
        ASSERT_EQ(buf[8], '2');
    }

    // Multiple placeholders
    {
        constexpr auto fmt = "{a}/{b}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 10, "b"_arg = 20);
        ASSERT_EQ(len, 5u);
        ASSERT_EQ(buf[0], '1');
        ASSERT_EQ(buf[2], '/');
        ASSERT_EQ(buf[3], '2');
    }

    // Function call syntax
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg(99));
        ASSERT_EQ(buf[0], '9');
        ASSERT_EQ(buf[1], '9');
    }

    // String argument
    {
        constexpr auto fmt = "Hello, {name}!"_fmt;
        char buf[64];
        fmt.to(buf, "name"_arg = "World");
        ASSERT_EQ(buf[0], 'H');
        ASSERT_EQ(buf[7], 'W');
    }

    // Hex formatting
    {
        constexpr auto fmt = "{x:x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 255);
        ASSERT_EQ(buf[0], 'f');
        ASSERT_EQ(buf[1], 'f');
    }

    {
        constexpr auto fmt = "{x:X}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 0xABCD);
        ASSERT_EQ(buf[0], 'A');
        ASSERT_EQ(buf[1], 'B');
        ASSERT_EQ(buf[2], 'C');
        ASSERT_EQ(buf[3], 'D');
    }

    // Negative numbers
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = -42);
        ASSERT_EQ(buf[0], '-');
        ASSERT_EQ(buf[1], '4');
        ASSERT_EQ(buf[2], '2');
    }

    // MSB-first digits (typewriter mode)
    {
        constexpr auto fmt = "{x}"_fmt;
        auto gen = fmt.generator("x"_arg = 123);

        auto c1 = gen();
        ASSERT_TRUE(c1.has_value());
        ASSERT_EQ(*c1, '1');

        auto c2 = gen();
        ASSERT_EQ(*c2, '2');

        auto c3 = gen();
        ASSERT_EQ(*c3, '3');
    }

    // Generator basic
    {
        constexpr auto fmt = "Hi"_fmt;
        auto gen = fmt.generator();

        auto c1 = gen();
        ASSERT_TRUE(c1.has_value());
        ASSERT_EQ(*c1, 'H');

        auto c2 = gen();
        ASSERT_TRUE(c2.has_value());
        ASSERT_EQ(*c2, 'i');

        auto c3 = gen();
        ASSERT_FALSE(c3.has_value());
        ASSERT_TRUE(gen.done());
    }

    // until_break() for line wrapping
    {
        constexpr auto fmt = "Hello World"_fmt;
        auto gen = fmt.generator();

        ASSERT_EQ(gen.until_break(), 5u);

        for (int i = 0; i < 5; ++i) gen();

        ASSERT_EQ(gen.until_break(), 0u);
    }

    // Generator reset
    {
        constexpr auto fmt = "AB"_fmt;
        auto gen = fmt.generator();

        gen(); gen(); gen();
        ASSERT_TRUE(gen.done());

        gen.reset();
        ASSERT_FALSE(gen.done());

        auto c = gen();
        ASSERT_EQ(*c, 'A');
    }

    // Escaped braces
    {
        constexpr auto fmt = "{{x}}"_fmt;
        char buf[64];
        fmt.to(buf);
        ASSERT_EQ(buf[0], '{');
        ASSERT_EQ(buf[1], 'x');
        ASSERT_EQ(buf[2], '}');
    }

    // Zero
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = 0);
        ASSERT_EQ(buf[0], '0');
        ASSERT_EQ(buf[1], '\0');
    }

    // Large number
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 1234567890);
        ASSERT_EQ(len, 10u);
    }

    // Constexpr formatting (to_static)
    {
        constexpr auto msg = "Answer: {x}"_fmt.to_static<32>("x"_arg = 42);
        ASSERT_EQ(msg[0], 'A');
        ASSERT_EQ(msg[8], '4');
        ASSERT_EQ(msg[9], '2');
    }

    // User workspace
    {
        char workspace[64];
        constexpr auto fmt = "{x}"_fmt;
        auto gen = fmt.generator_with(
            external_workspace{workspace, sizeof(workspace)},
            "x"_arg = 42
        );
        auto c = gen();
        ASSERT_EQ(*c, '4');
    }


    // until_break() at word boundaries
    {
        constexpr auto fmt = "A BC DEF"_fmt;
        auto gen = fmt.generator();

        ASSERT_EQ(gen.until_break(), 1u); // "A"
        gen();
        ASSERT_EQ(gen.until_break(), 0u); // at space
        gen();
        ASSERT_EQ(gen.until_break(), 2u); // "BC"
    }

    test::finalize();
}
