/// @file test_testing.cpp
/// @brief Smoke tests for the public gba/testing API.

#include <gba/testing>

#include <source_location>

int main() {
    gba::test.eq(1 + 1, 2, "implicit assert eq");
    gba::test.is_true(true, "implicit assert is_true");
    gba::test.is_false(false, "implicit assert is_false");
    gba::test.neq(1, 2, "implicit assert neq");
    gba::test.zero(0, "implicit assert zero");
    gba::test.nz(7, "implicit assert nz");
    gba::test.gtz(7, "implicit assert gtz");
    gba::test.ltz(-7, "implicit assert ltz");
    gba::test.ltz(7u, "implicit assert ltz unsigned behaves like nz");
    gba::test.gr(3, 2, "implicit assert gr");
    gba::test.lt(2, 3, "implicit assert lt");
    gba::test.ge(3, 3, "implicit assert ge");
    gba::test.le(3, 3, "implicit assert le");

    gba::test("call-operator", [] {
        gba::test.expect.eq(2 + 2, 4, "2+2 == 4");
        gba::test.expect.is_true(true, "basic check");
        gba::test.expect.is_false(false, "expect is_false");
        gba::test.expect.neq(9, 1, "expect neq");
        gba::test.expect.zero(0, "expect zero");
        gba::test.expect.nz(5, "expect nz");
        gba::test.expect.gtz(5, "expect gtz");
        gba::test.expect.ltz(-5, "expect ltz");
        gba::test.expect.ltz(5u, "expect ltz unsigned behaves like nz");
        gba::test.expect.gr(8, 2, "expect gr");
        gba::test.expect.lt(1, 9, "expect lt");
        gba::test.expect.ge(4, 4, "expect ge");
        gba::test.expect.le(4, 4, "expect le");
    });

    const auto loc = std::source_location::current();
    gba::test.expect.is_true(true, "explicit source_location", loc);

    const auto stats = gba::test.stats();
    gba::test.expect.eq(stats.failures, 0u, "no failures expected");
    gba::test.expect.gr(stats.passes, 10u, "expected pass count");
    return gba::test.finish();
}
