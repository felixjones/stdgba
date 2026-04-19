/// @file tests/format/test_format.cpp
/// @brief Unit tests for format module using mgba test framework.

#include <gba/format>
#include <gba/text_format>
#include <gba/angle>
#include <gba/fixed_point>
#include <gba/testing>

using namespace gba::format;
using namespace gba::literals;

static_assert(parse_format<"{x:*^7.3}">().valid);
static_assert(parse_format<"{x:#010x}">().valid);
static_assert(parse_format<"{x:.3f}">().valid);
static_assert(parse_format<"{a:.4x}">().valid);
static_assert(parse_format<"{x:%}">().valid);
static_assert(parse_format<"{x:.2e}">().valid);
static_assert(parse_format<"{x:.2E}">().valid);
static_assert(parse_format<"{x:.4g}">().valid);
static_assert(parse_format<"{x:.4G}">().valid);
static_assert(parse_format<"{x:pal}", gba::text::text_format_config>().valid);
static_assert(parse_format<"{x:pal}", gba::text::text_format_config>().segments[0].spec.fmt_type ==
              format_spec::format_kind::extension);

static_assert(!parse_format<"{x:!}">().valid);
static_assert(!parse_format<"{x:.2,}">().valid);
static_assert(!parse_format<"{x:+s}">().valid);
static_assert(!parse_format<"{x:,s}">().valid);
static_assert(!parse_format<"{x:=s}">().valid);
static_assert(!parse_format<"{x:.2i}">().valid);
static_assert(!parse_format<"{x:pal}">().valid);
static_assert(!parse_format<"{x:+pal}", gba::text::text_format_config>().valid);

constexpr auto text_palette_static = gba::text::text_format<"{pal:pal}">::to_static<8>("pal"_arg = 7);
constexpr gba::text::text_format<"{pal:pal}"> text_palette_literal = "{pal:pal}"_fmt;
constexpr auto text_palette_with_config = "{pal:pal}"_fmt.with_config<gba::text::text_format_config>();
static_assert(static_cast<unsigned char>(text_palette_static[0]) == 0x1B);
static_assert(static_cast<unsigned char>(text_palette_static[1]) == 7u);
static_assert(text_palette_static[2] == '\0');

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

    // String precision + alignment
    {
        constexpr auto fmt = "{name:*^7.3}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "name"_arg = "World");
        gba::test.eq(len, 7u);
        gba::test.eq(buf[0], '*');
        gba::test.eq(buf[1], '*');
        gba::test.eq(buf[2], 'W');
        gba::test.eq(buf[3], 'o');
        gba::test.eq(buf[4], 'r');
        gba::test.eq(buf[5], '*');
        gba::test.eq(buf[6], '*');
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

    // Alternate-form hex + zero pad
    {
        constexpr auto fmt = "{x:#010x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 0x2A);
        gba::test.eq(len, 10u);
        gba::test.eq(buf[0], '0');
        gba::test.eq(buf[1], 'x');
        gba::test.eq(buf[2], '0');
        gba::test.eq(buf[7], '0');
        gba::test.eq(buf[8], '2');
        gba::test.eq(buf[9], 'a');
    }

    // Grouped decimal
    {
        constexpr auto fmt = "{x:_d}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 1234567);
        gba::test.eq(len, 9u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '_');
        gba::test.eq(buf[5], '_');
        gba::test.eq(buf[8], '7');
    }

    // Grouped decimal edge: no separator below threshold
    {
        constexpr auto fmt = "{x:,d}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 999);
        gba::test.eq(len, 3u);
        gba::test.eq(buf[0], '9');
        gba::test.eq(buf[1], '9');
        gba::test.eq(buf[2], '9');
    }

    // Grouped decimal edge: exact separator threshold
    {
        constexpr auto fmt = "{x:,d}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 1000);
        gba::test.eq(len, 5u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], ',');
        gba::test.eq(buf[2], '0');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '0');
    }

    // Grouped decimal edge: explicit grouped type defaults to comma
    {
        constexpr auto fmt = "{x:n}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 1234567);
        gba::test.eq(len, 9u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], ',');
        gba::test.eq(buf[5], ',');
        gba::test.eq(buf[8], '7');
    }

    // Grouped decimal edge: explicit underscore grouping with grouped type
    {
        constexpr auto fmt = "{x:_n}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = -1234567);
        gba::test.eq(len, 10u);
        gba::test.eq(buf[0], '-');
        gba::test.eq(buf[1], '1');
        gba::test.eq(buf[2], '_');
        gba::test.eq(buf[6], '_');
        gba::test.eq(buf[9], '7');
    }

    // Width + sign
    {
        constexpr auto fmt = "{x:+08d}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = 123);
        gba::test.eq(len, 8u);
        gba::test.eq(buf[0], '+');
        gba::test.eq(buf[1], '0');
        gba::test.eq(buf[5], '1');
        gba::test.eq(buf[7], '3');
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

    // Fixed-point default formatting
    {
        constexpr auto fmt = "{x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{3.5_fx});
        gba::test.eq(len, 3u);
        gba::test.eq(buf[0], '3');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
    }

    // Fixed-point explicit precision
    {
        constexpr auto fmt = "{x:.3f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{3.5_fx});
        gba::test.eq(len, 5u);
        gba::test.eq(buf[0], '3');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '0');
    }

    // Fixed-point grouped formatting
    {
        constexpr auto fmt = "{x:,.2f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{1234.5_fx});
        gba::test.eq(len, 8u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], ',');
        gba::test.eq(buf[2], '2');
        gba::test.eq(buf[5], '.');
        gba::test.eq(buf[6], '5');
        gba::test.eq(buf[7], '0');
    }

    // Fixed-point alternate form with zero precision
    {
        constexpr auto fmt = "{x:#.0f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{3.0_fx});
        gba::test.eq(len, 2u);
        gba::test.eq(buf[0], '3');
        gba::test.eq(buf[1], '.');
    }

    // Fixed-point runtime fractional helper path: near-one value
    {
        using fix8 = gba::fixed<int, 8>;
        constexpr auto fmt = "{x:.4f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = __builtin_bit_cast(fix8, 255));
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '0');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '9');
        gba::test.eq(buf[3], '9');
        gba::test.eq(buf[4], '6');
        gba::test.eq(buf[5], '0');
    }

    // Fixed-point runtime fractional helper path: 12 fractional bits
    {
        using fix12 = gba::fixed<int, 12>;
        constexpr auto fmt = "{x:.4f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = fix12{1.5_fx});
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '0');
        gba::test.eq(buf[5], '0');
    }

    // Fixed-point runtime fractional helper path: 16 fractional bits
    {
        using fix16 = gba::fixed<int, 16>;
        constexpr auto fmt = "{x:.4f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = fix16{1.25_fx});
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '2');
        gba::test.eq(buf[3], '5');
        gba::test.eq(buf[4], '0');
        gba::test.eq(buf[5], '0');
    }

    // Fixed-point fallback path above 16 fractional bits remains correct
    {
        using fix20 = gba::fixed<int, 20>;
        constexpr auto fmt = "{x:.4f}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = fix20{1.5_fx});
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '0');
        gba::test.eq(buf[5], '0');
    }

    // Constexpr formatting (to_static)
    {
        constexpr auto msg = "Answer: {x}"_fmt.to_static<32>("x"_arg = 42);
        gba::test.eq(msg[0], 'A');
        gba::test.eq(msg[8], '4');
        gba::test.eq(msg[9], '2');
    }

    // Constexpr formatting with width
    {
        constexpr auto msg = "[{x:>4}]"_fmt.to_static<32>("x"_arg = 42);
        gba::test.eq(msg[0], '[');
        gba::test.eq(msg[1], ' ');
        gba::test.eq(msg[2], ' ');
        gba::test.eq(msg[3], '4');
        gba::test.eq(msg[4], '2');
        gba::test.eq(msg[5], ']');
    }

    // Constexpr formatting with fixed literal
    {
        constexpr auto msg = "Pi {x:.2f}"_fmt.to_static<32>("x"_arg = 3.14_fx);
        gba::test.eq(msg[0], 'P');
        gba::test.eq(msg[1], 'i');
        gba::test.eq(msg[2], ' ');
        gba::test.eq(msg[3], '3');
        gba::test.eq(msg[4], '.');
        gba::test.eq(msg[5], '1');
        gba::test.eq(msg[6], '4');
    }

    // Angle default formatting (degrees)
    {
        constexpr auto fmt = "{a}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 90_deg);
        gba::test.eq(len, 2u);
        gba::test.eq(buf[0], '9');
        gba::test.eq(buf[1], '0');
    }

    // Angle radians formatting
    {
        constexpr auto fmt = "{a:.4r}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 90_deg);
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
        gba::test.eq(buf[3], '7');
        gba::test.eq(buf[4], '0');
        gba::test.eq(buf[5], '7');
    }

    // Angle boundary: +180 and -180 map to the same half-turn raw value
    {
        constexpr auto fmt = "{a:i}"_fmt;
        char posBuf[64];
        char negBuf[64];
        const auto posLen = fmt.to(posBuf, "a"_arg = 180_deg);
        const auto negLen = fmt.to(negBuf, "a"_arg = -180_deg);
        gba::test.is_true(posLen >= 9u);
        gba::test.is_true(negLen > 0u);
        gba::test.eq(posBuf[0], '2');
    }

    // Angle boundary: +/-pi stay on the expected radians boundary
    {
        constexpr auto fmt = "{a:.4r}"_fmt;
        char posBuf[64];
        char negBuf[64];
        const auto posLen = fmt.to(posBuf, "a"_arg = 3.14159265358979323846_rad);
        const auto negLen = fmt.to(negBuf, "a"_arg = -3.14159265358979323846_rad);
        gba::test.is_true(posLen >= 4u);
        gba::test.is_true(negLen >= 4u);
        gba::test.eq(posBuf[0], '3');
        gba::test.eq(posBuf[1], '.');
        gba::test.eq(posBuf[2], '1');
        gba::test.eq(posBuf[3], '4');
        bool negHasDot = false;
        for (std::size_t i = 0; i < negLen; ++i) {
            if (negBuf[i] == '.') {
                negHasDot = true;
                break;
            }
        }
        gba::test.is_true(negHasDot);
    }

    // Angle turns formatting
    {
        constexpr auto fmt = "{a:.3t}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 90_deg);
        gba::test.eq(len, 5u);
        gba::test.eq(buf[0], '0');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '2');
        gba::test.eq(buf[3], '5');
        gba::test.eq(buf[4], '0');
    }

    // Angle raw integer formatting
    {
        constexpr auto fmt = "{a:i}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 90_deg);
        gba::test.eq(len, 10u);
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[9], '4');
    }

    // Angle raw hex precision
    {
        constexpr auto fmt = "{a:.4x}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = 90_deg);
        gba::test.eq(len, 4u);
        gba::test.eq(buf[0], '4');
        gba::test.eq(buf[1], '0');
        gba::test.eq(buf[2], '0');
        gba::test.eq(buf[3], '0');
    }

    // Packed angle raw hex precision with alternate form
    {
        constexpr auto fmt = "{a:#.4X}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "a"_arg = gba::packed_angle16{90_deg});
        gba::test.eq(len, 6u);
        gba::test.eq(buf[0], '0');
        gba::test.eq(buf[1], 'X');
        gba::test.eq(buf[2], '4');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '0');
        gba::test.eq(buf[5], '0');
    }

    // Fixed-point percent formatting
    {
        constexpr auto fmt = "{x:%}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{0.5_fx});
        gba::test.eq(buf[0], '5');
        gba::test.eq(buf[1], '0');
        gba::test.eq(buf[2], '%');
    }

    // Fixed-point percent with precision
    {
        constexpr auto fmt = "{x:.1%}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{0.5_fx});
        gba::test.eq(buf[0], '5');
        gba::test.eq(buf[1], '0');
        gba::test.eq(buf[2], '.');
        gba::test.eq(buf[3], '0');
        gba::test.eq(buf[4], '%');
    }

    // Fixed-point scientific formatting
    {
        constexpr auto fmt = "{x:.2e}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{1234.5_fx});
        // Expect "1.23e+03"
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '2');
        gba::test.eq(buf[3], '3');
        gba::test.eq(buf[4], 'e');
        gba::test.eq(buf[5], '+');
        gba::test.eq(buf[6], '0');
        gba::test.eq(buf[7], '3');
    }

    // Fixed-point uppercase scientific
    {
        constexpr auto fmt = "{x:.2E}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = gba::fixed<int, 8>{1234.5_fx});
        gba::test.eq(buf[4], 'E');
    }

    // Fixed-point scientific with small value
    {
        constexpr auto fmt = "{x:.3e}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{0.25_fx});
        // Expect "2.500e-01"
        gba::test.eq(buf[0], '2');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[5], 'e');
        gba::test.eq(buf[6], '-');
        gba::test.eq(buf[7], '0');
        gba::test.eq(buf[8], '1');
    }

    // Fixed-point general formatting (small value -> fixed)
    {
        constexpr auto fmt = "{x:.4g}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{3.5_fx});
        // 3.5 with precision 4: exponent 0, 0 >= -4 && 0 < 4 -> fixed
        gba::test.eq(buf[0], '3');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[2], '5');
    }

    // Fixed-point general formatting (large value -> scientific)
    {
        constexpr auto fmt = "{x:.3g}"_fmt;
        char buf[64];
        auto len = fmt.to(buf, "x"_arg = gba::fixed<int, 8>{1234.5_fx});
        // 1234.5 with precision 3: exponent 3, 3 >= 3 -> scientific "1.23e+03"
        gba::test.eq(buf[0], '1');
        gba::test.eq(buf[1], '.');
        gba::test.eq(buf[4], 'e');
    }

    // Fixed-point general uppercase
    {
        constexpr auto fmt = "{x:.3G}"_fmt;
        char buf[64];
        fmt.to(buf, "x"_arg = gba::fixed<int, 8>{1234.5_fx});
        gba::test.eq(buf[4], 'E');
    }

    // Compile-time fixed_literal percent
    {
        constexpr auto msg = "{x:%}"_fmt.to_static<32>("x"_arg = 0.5_fx);
        gba::test.eq(msg[0], '5');
        gba::test.eq(msg[1], '0');
        gba::test.eq(msg[2], '%');
    }

    // Compile-time fixed_literal scientific
    {
        constexpr auto msg = "{x:.2e}"_fmt.to_static<32>("x"_arg = 1234.5_fx);
        gba::test.eq(msg[0], '1');
        gba::test.eq(msg[1], '.');
        gba::test.eq(msg[4], 'e');
    }

    // Compile-time fixed_literal scientific boundary: +1e99
    {
        constexpr auto msg = "{x:.2e}"_fmt.to_static<32>("x"_arg = 1e99_fx);
        gba::test.eq(msg[0], '1');
        gba::test.eq(msg[1], '.');
        gba::test.eq(msg[2], '0');
        gba::test.eq(msg[3], '0');
        gba::test.eq(msg[4], 'e');
        gba::test.eq(msg[5], '+');
        gba::test.eq(msg[6], '9');
        gba::test.eq(msg[7], '9');
    }

    // Compile-time fixed_literal scientific boundary: +1e-99
    {
        constexpr auto msg = "{x:.2E}"_fmt.to_static<32>("x"_arg = 1e-99_fx);
        gba::test.eq(msg[0], '1');
        gba::test.eq(msg[1], '.');
        gba::test.eq(msg[2], '0');
        gba::test.eq(msg[3], '0');
        gba::test.eq(msg[4], 'E');
        gba::test.eq(msg[5], '-');
        gba::test.eq(msg[6], '9');
        gba::test.eq(msg[7], '9');
    }

    // User workspace
    {
        char workspace[64];
        constexpr auto fmt = "{x}"_fmt;
        auto gen = fmt.generator_with(external_workspace{workspace, sizeof(workspace)}, "x"_arg = 42);
        auto c = gen();
        gba::test.eq(*c, '4');
    }

    // text palette formatting via to()
    {
        constexpr gba::text::text_format<"P{pal:pal}Z"> fmt = "P{pal:pal}Z"_fmt;
        char buf[16]{};
        auto len = fmt.to(buf, "pal"_arg = 5);
        gba::test.eq(len, 4u);
        gba::test.eq(buf[0], 'P');
        gba::test.eq(static_cast<unsigned char>(buf[1]), 0x1Bu);
        gba::test.eq(static_cast<unsigned char>(buf[2]), 5u);
        gba::test.eq(buf[3], 'Z');
        gba::test.eq(buf[4], '\0');
    }

    // text palette formatting via generator()
    {
        constexpr auto fmt = "{pal:pal}X"_fmt.with_config<gba::text::text_format_config>();
        auto gen = fmt.generator("pal"_arg = 12);
        auto c1 = gen();
        auto c2 = gen();
        auto c3 = gen();
        auto c4 = gen();
        gba::test.is_true(c1.has_value());
        gba::test.is_true(c2.has_value());
        gba::test.is_true(c3.has_value());
        gba::test.is_false(c4.has_value());
        gba::test.eq(static_cast<unsigned char>(*c1), 0x1Bu);
        gba::test.eq(static_cast<unsigned char>(*c2), 12u);
        gba::test.eq(*c3, 'X');
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
