/**
 * @file test_div.cpp
 * @brief Exhaustive integer division and modulo test suite.
 *
 * Exercises __aeabi_idiv, __aeabi_uidiv, __aeabi_idivmod, __aeabi_uidivmod
 * and compiler-generated shifts for power-of-2 divisors. Covers all
 * combinations of signed/unsigned 8/16/32/64-bit operands.
 *
 * All operands are volatile to prevent constant folding.
 */
#include <cstdint>
#include <climits>

#include <mgba_test.hpp>

// ---------------------------------------------------------------------------
// Helpers — volatile operands guarantee runtime division
// ---------------------------------------------------------------------------

template<typename T>
struct vol {
    volatile T v;
    constexpr vol(T x) : v{x} {}
    T get() const { return v; }
};

// Shorthand: perform division/modulo through volatile to force __aeabi calls
#define DIV(a, b) (vol<decltype(a)>{a}.get() / vol<decltype(b)>{b}.get())
#define MOD(a, b) (vol<decltype(a)>{a}.get() % vol<decltype(b)>{b}.get())

// ---------------------------------------------------------------------------
// Signed 32-bit (__aeabi_idiv / __aeabi_idivmod)
// ---------------------------------------------------------------------------

static void test_signed_32_basic() {
    // Identity & simple
    EXPECT_EQ(DIV( 0,  1),  0);
    EXPECT_EQ(DIV( 1,  1),  1);
    EXPECT_EQ(DIV( 7,  1),  7);
    EXPECT_EQ(DIV( 7,  7),  1);
    EXPECT_EQ(DIV(10,  3),  3);
    EXPECT_EQ(DIV(10,  5),  2);
    EXPECT_EQ(DIV(99,  10), 9);
    EXPECT_EQ(DIV(100, 10), 10);
    EXPECT_EQ(DIV(255, 17), 15);
    EXPECT_EQ(DIV(1000, 7), 142);

    // Modulo
    EXPECT_EQ(MOD( 0,  1), 0);
    EXPECT_EQ(MOD( 7,  1), 0);
    EXPECT_EQ(MOD(10,  3), 1);
    EXPECT_EQ(MOD(10,  5), 0);
    EXPECT_EQ(MOD(99,  10), 9);
    EXPECT_EQ(MOD(255, 17), 0);
    EXPECT_EQ(MOD(1000, 7), 6);
}

static void test_signed_32_negative() {
    // C99 truncation-toward-zero semantics
    EXPECT_EQ(DIV(-7,   2), -3);
    EXPECT_EQ(DIV( 7,  -2), -3);
    EXPECT_EQ(DIV(-7,  -2),  3);
    EXPECT_EQ(DIV(-1,   1), -1);
    EXPECT_EQ(DIV( 1,  -1), -1);
    EXPECT_EQ(DIV(-1,  -1),  1);
    EXPECT_EQ(DIV(-10,  3), -3);
    EXPECT_EQ(DIV( 10, -3), -3);
    EXPECT_EQ(DIV(-10, -3),  3);
    EXPECT_EQ(DIV(-99, 10), -9);
    EXPECT_EQ(DIV(-100, 10), -10);

    EXPECT_EQ(MOD(-7,   2), -1);
    EXPECT_EQ(MOD( 7,  -2),  1);
    EXPECT_EQ(MOD(-7,  -2), -1);
    EXPECT_EQ(MOD(-10,  3), -1);
    EXPECT_EQ(MOD( 10, -3),  1);
    EXPECT_EQ(MOD(-10, -3), -1);
    EXPECT_EQ(MOD(-99, 10), -9);
}

static void test_signed_32_power_of_two() {
    // Powers of 2 — compiler may emit shifts, but the result must match
    EXPECT_EQ(DIV( 128,  2),   64);
    EXPECT_EQ(DIV(-128,  2),  -64);
    EXPECT_EQ(DIV( 128, -2),  -64);
    EXPECT_EQ(DIV(-128, -2),   64);
    EXPECT_EQ(DIV( 1024, 4),  256);
    EXPECT_EQ(DIV(-1024, 4), -256);
    EXPECT_EQ(DIV( 255,  4),   63);
    EXPECT_EQ(DIV(-255,  4),  -63);
    EXPECT_EQ(DIV( 65536, 256), 256);
    EXPECT_EQ(DIV( 65536, 65536), 1);

    EXPECT_EQ(MOD( 255,  4),  3);
    EXPECT_EQ(MOD(-255,  4), -3);
    EXPECT_EQ(MOD( 255, -4),  3);
    EXPECT_EQ(MOD(-255, -4), -3);
    EXPECT_EQ(MOD( 127,  64), 63);
    EXPECT_EQ(MOD(-127,  64), -63);
}

static void test_signed_32_extreme() {
    constexpr int IMIN = INT_MIN;  // -2147483648
    constexpr int IMAX = INT_MAX;  //  2147483647

    // MAX / 1, MAX / -1
    EXPECT_EQ(DIV(IMAX,  1),  IMAX);
    EXPECT_EQ(DIV(IMAX, -1), -IMAX);
    EXPECT_EQ(MOD(IMAX,  1),  0);
    EXPECT_EQ(MOD(IMAX, -1),  0);

    // MIN / 1 (MIN / -1 is UB, skip)
    EXPECT_EQ(DIV(IMIN,  1),  IMIN);
    EXPECT_EQ(MOD(IMIN,  1),  0);

    // MIN / 2, MIN / -2
    EXPECT_EQ(DIV(IMIN,  2), IMIN / 2);
    EXPECT_EQ(DIV(IMIN, -2), -(IMIN / 2));
    EXPECT_EQ(MOD(IMIN,  2), 0);

    // Large dividend, small divisor
    EXPECT_EQ(DIV(IMAX, 2), 1073741823);
    EXPECT_EQ(MOD(IMAX, 2), 1);
    EXPECT_EQ(DIV(IMAX, 3), 715827882);
    EXPECT_EQ(MOD(IMAX, 3), 1);

    // Dividend < divisor
    EXPECT_EQ(DIV(1, IMAX), 0);
    EXPECT_EQ(MOD(1, IMAX), 1);
    EXPECT_EQ(DIV(1, IMIN), 0);
    EXPECT_EQ(MOD(1, IMIN), 1);

    // Self-division
    EXPECT_EQ(DIV(IMAX, IMAX), 1);
    EXPECT_EQ(MOD(IMAX, IMAX), 0);
    EXPECT_EQ(DIV(IMIN, IMIN), 1);
    EXPECT_EQ(MOD(IMIN, IMIN), 0);

    // Adjacent values
    EXPECT_EQ(DIV(IMAX, IMAX - 1), 1);
    EXPECT_EQ(MOD(IMAX, IMAX - 1), 1);
}

// ---------------------------------------------------------------------------
// Unsigned 32-bit (__aeabi_uidiv / __aeabi_uidivmod)
// ---------------------------------------------------------------------------

static void test_unsigned_32_basic() {
    EXPECT_EQ(DIV(0u,  1u), 0u);
    EXPECT_EQ(DIV(1u,  1u), 1u);
    EXPECT_EQ(DIV(7u,  1u), 7u);
    EXPECT_EQ(DIV(10u, 3u), 3u);
    EXPECT_EQ(DIV(10u, 5u), 2u);
    EXPECT_EQ(DIV(99u, 10u), 9u);
    EXPECT_EQ(DIV(255u, 17u), 15u);
    EXPECT_EQ(DIV(1000u, 7u), 142u);

    EXPECT_EQ(MOD(0u,  1u), 0u);
    EXPECT_EQ(MOD(10u, 3u), 1u);
    EXPECT_EQ(MOD(99u, 10u), 9u);
    EXPECT_EQ(MOD(255u, 17u), 0u);
    EXPECT_EQ(MOD(1000u, 7u), 6u);
}

static void test_unsigned_32_power_of_two() {
    EXPECT_EQ(DIV(128u,   2u),  64u);
    EXPECT_EQ(DIV(1024u,  4u), 256u);
    EXPECT_EQ(DIV(255u,   4u),  63u);
    EXPECT_EQ(DIV(65536u, 256u), 256u);
    EXPECT_EQ(DIV(65536u, 65536u), 1u);

    EXPECT_EQ(MOD(255u, 4u),  3u);
    EXPECT_EQ(MOD(127u, 64u), 63u);
    EXPECT_EQ(MOD(256u, 256u), 0u);
}

static void test_unsigned_32_extreme() {
    constexpr unsigned UMAX = UINT_MAX;  // 4294967295

    EXPECT_EQ(DIV(UMAX, 1u), UMAX);
    EXPECT_EQ(MOD(UMAX, 1u), 0u);
    EXPECT_EQ(DIV(UMAX, 2u), 2147483647u);
    EXPECT_EQ(MOD(UMAX, 2u), 1u);
    EXPECT_EQ(DIV(UMAX, 3u), 1431655765u);
    EXPECT_EQ(MOD(UMAX, 3u), 0u);
    EXPECT_EQ(DIV(UMAX, UMAX), 1u);
    EXPECT_EQ(MOD(UMAX, UMAX), 0u);
    EXPECT_EQ(DIV(1u, UMAX), 0u);
    EXPECT_EQ(MOD(1u, UMAX), 1u);

    // High-bit set values (regression: unsigned treated as signed)
    constexpr unsigned HI = 0x80000000u;
    EXPECT_EQ(DIV(HI, 1u), HI);
    EXPECT_EQ(DIV(HI, 2u), 0x40000000u);
    EXPECT_EQ(MOD(HI, 3u), 2u);
    EXPECT_EQ(DIV(0xFFFFFFFFu, 0x10000u), 0xFFFFu);
    EXPECT_EQ(MOD(0xFFFFFFFFu, 0x10000u), 0xFFFFu);
}

// ---------------------------------------------------------------------------
// Signed 64-bit (__aeabi_ldivmod)
// ---------------------------------------------------------------------------

static void test_signed_64() {
    using i64 = std::int64_t;

    EXPECT_EQ(DIV(i64{0},  i64{1}),  i64{0});
    EXPECT_EQ(DIV(i64{10}, i64{3}),  i64{3});
    EXPECT_EQ(MOD(i64{10}, i64{3}),  i64{1});
    EXPECT_EQ(DIV(i64{-10}, i64{3}), i64{-3});
    EXPECT_EQ(MOD(i64{-10}, i64{3}), i64{-1});
    EXPECT_EQ(DIV(i64{10}, i64{-3}), i64{-3});
    EXPECT_EQ(MOD(i64{10}, i64{-3}), i64{1});

    // Large values exceeding 32 bits
    constexpr i64 BIG = 1000000000000LL;  // 10^12
    EXPECT_EQ(DIV(BIG, i64{1000000}), i64{1000000});
    EXPECT_EQ(MOD(BIG, i64{1000000}), i64{0});
    EXPECT_EQ(DIV(BIG, i64{7}), i64{142857142857LL});
    EXPECT_EQ(MOD(BIG, i64{7}), i64{1});
    EXPECT_EQ(DIV(BIG + 1, i64{7}), i64{142857142857LL});
    EXPECT_EQ(MOD(BIG + 1, i64{7}), i64{2});

    // INT64 extremes
    constexpr i64 I64MAX = INT64_MAX;
    EXPECT_EQ(DIV(I64MAX, i64{1}), I64MAX);
    EXPECT_EQ(MOD(I64MAX, i64{1}), i64{0});
    EXPECT_EQ(DIV(I64MAX, i64{2}), i64{4611686018427387903LL});
    EXPECT_EQ(MOD(I64MAX, i64{2}), i64{1});
    EXPECT_EQ(DIV(I64MAX, I64MAX), i64{1});
    EXPECT_EQ(MOD(I64MAX, I64MAX), i64{0});

    constexpr i64 I64MIN = INT64_MIN;
    EXPECT_EQ(DIV(I64MIN, i64{1}), I64MIN);
    EXPECT_EQ(MOD(I64MIN, i64{1}), i64{0});
    EXPECT_EQ(DIV(I64MIN, i64{2}), i64{-4611686018427387904LL});
    EXPECT_EQ(MOD(I64MIN, i64{2}), i64{0});
    EXPECT_EQ(DIV(I64MIN, I64MIN), i64{1});
    EXPECT_EQ(MOD(I64MIN, I64MIN), i64{0});
}

// ---------------------------------------------------------------------------
// Unsigned 64-bit (__aeabi_uldivmod)
// ---------------------------------------------------------------------------

static void test_unsigned_64() {
    using u64 = std::uint64_t;

    EXPECT_EQ(DIV(u64{0},  u64{1}),  u64{0});
    EXPECT_EQ(DIV(u64{10}, u64{3}),  u64{3});
    EXPECT_EQ(MOD(u64{10}, u64{3}),  u64{1});
    EXPECT_EQ(DIV(u64{1000000000000ULL}, u64{7}), u64{142857142857ULL});
    EXPECT_EQ(MOD(u64{1000000000000ULL}, u64{7}), u64{1});

    constexpr u64 U64MAX = UINT64_MAX;
    EXPECT_EQ(DIV(U64MAX, u64{1}), U64MAX);
    EXPECT_EQ(MOD(U64MAX, u64{1}), u64{0});
    EXPECT_EQ(DIV(U64MAX, u64{2}), u64{9223372036854775807ULL});
    EXPECT_EQ(MOD(U64MAX, u64{2}), u64{1});
    EXPECT_EQ(DIV(U64MAX, U64MAX), u64{1});
    EXPECT_EQ(MOD(U64MAX, U64MAX), u64{0});
    EXPECT_EQ(DIV(u64{1}, U64MAX), u64{0});
    EXPECT_EQ(MOD(u64{1}, U64MAX), u64{1});

    // High bit set
    constexpr u64 HI64 = u64{1} << 63;
    EXPECT_EQ(DIV(HI64, u64{1}), HI64);
    EXPECT_EQ(DIV(HI64, u64{2}), u64{1} << 62);
    EXPECT_EQ(MOD(HI64, u64{3}), u64{2});
}

// ---------------------------------------------------------------------------
// Narrow types — signed 8/16-bit (promoted to int, then __aeabi_idiv)
// ---------------------------------------------------------------------------

static void test_signed_narrow() {
    // int8_t
    using i8 = std::int8_t;
    EXPECT_EQ(DIV(i8{127},  i8{1}),   127);
    EXPECT_EQ(DIV(i8{-128}, i8{1}),  -128);
    EXPECT_EQ(DIV(i8{127},  i8{2}),    63);
    EXPECT_EQ(MOD(i8{127},  i8{2}),     1);
    EXPECT_EQ(DIV(i8{-128}, i8{2}),   -64);
    EXPECT_EQ(MOD(i8{-128}, i8{2}),     0);
    EXPECT_EQ(DIV(i8{100},  i8{7}),    14);
    EXPECT_EQ(MOD(i8{100},  i8{7}),     2);
    EXPECT_EQ(DIV(i8{-100}, i8{7}),   -14);
    EXPECT_EQ(MOD(i8{-100}, i8{7}),    -2);

    // int16_t
    using i16 = std::int16_t;
    EXPECT_EQ(DIV(i16{32767},  i16{1}),  32767);
    EXPECT_EQ(DIV(i16{-32768}, i16{1}), -32768);
    EXPECT_EQ(DIV(i16{32767},  i16{2}),  16383);
    EXPECT_EQ(MOD(i16{32767},  i16{2}),  1);
    EXPECT_EQ(DIV(i16{-32768}, i16{2}), -16384);
    EXPECT_EQ(MOD(i16{-32768}, i16{2}),  0);
    EXPECT_EQ(DIV(i16{30000},  i16{7}),  4285);
    EXPECT_EQ(MOD(i16{30000},  i16{7}),  5);
    EXPECT_EQ(DIV(i16{-30000}, i16{7}), -4285);
    EXPECT_EQ(MOD(i16{-30000}, i16{7}), -5);
}

// ---------------------------------------------------------------------------
// Narrow types — unsigned 8/16-bit (promoted to int/unsigned, then uidiv)
// ---------------------------------------------------------------------------

static void test_unsigned_narrow() {
    // uint8_t
    using u8 = std::uint8_t;
    EXPECT_EQ(DIV(u8{255}, u8{1}),  255);
    EXPECT_EQ(DIV(u8{255}, u8{2}),  127);
    EXPECT_EQ(MOD(u8{255}, u8{2}),    1);
    EXPECT_EQ(DIV(u8{200}, u8{7}),   28);
    EXPECT_EQ(MOD(u8{200}, u8{7}),    4);
    EXPECT_EQ(DIV(u8{0},   u8{255}),  0);
    EXPECT_EQ(MOD(u8{0},   u8{255}),  0);

    // uint16_t
    using u16 = std::uint16_t;
    EXPECT_EQ(DIV(u16{65535}, u16{1}),  65535);
    EXPECT_EQ(DIV(u16{65535}, u16{2}),  32767);
    EXPECT_EQ(MOD(u16{65535}, u16{2}),  1);
    EXPECT_EQ(DIV(u16{50000}, u16{7}),  7142);
    EXPECT_EQ(MOD(u16{50000}, u16{7}),  6);
    EXPECT_EQ(DIV(u16{0},     u16{65535}), 0);
}

// ---------------------------------------------------------------------------
// Mixed-width operations (implicit promotion rules)
// ---------------------------------------------------------------------------

static void test_mixed_width() {
    // int8_t / int32_t -> int (promoted)
    EXPECT_EQ(DIV(std::int8_t{100}, 7), 14);
    EXPECT_EQ(MOD(std::int8_t{100}, 7), 2);

    // uint16_t / uint32_t -> unsigned
    EXPECT_EQ(DIV(std::uint16_t{50000}, 7u), 7142u);
    EXPECT_EQ(MOD(std::uint16_t{50000}, 7u), 6u);

    // int32_t / int64_t -> int64_t (promoted)
    EXPECT_EQ(DIV(1000000, std::int64_t{7}), std::int64_t{142857});
    EXPECT_EQ(MOD(1000000, std::int64_t{7}), std::int64_t{1});

    // Large unsigned fitting in 32 bits / small — verifies unsigned path
    EXPECT_EQ(DIV(0xDEADBEEFu, 16u), 0xDEADBEEFu / 16u);
    EXPECT_EQ(MOD(0xDEADBEEFu, 16u), 0xDEADBEEFu % 16u);
    EXPECT_EQ(DIV(0xDEADBEEFu, 10u), 0xDEADBEEFu / 10u);
    EXPECT_EQ(MOD(0xDEADBEEFu, 10u), 0xDEADBEEFu % 10u);
}

// ---------------------------------------------------------------------------
// Divisor = 1 (identity) and dividend = 0 (zero) sweeps
// ---------------------------------------------------------------------------

static void test_identity_and_zero() {
    // Division by 1 for many values
    for (int i = -50; i <= 50; ++i) {
        volatile int vi = i;
        EXPECT_EQ(vi / 1, i);
        EXPECT_EQ(vi % 1, 0);
    }

    // Zero dividend for many divisors
    for (int d = 1; d <= 50; ++d) {
        volatile int zero = 0;
        volatile int vd = d;
        EXPECT_EQ(zero / vd, 0);
        EXPECT_EQ(zero % vd, 0);
    }

    // Unsigned identity
    for (unsigned i = 0; i <= 100; ++i) {
        volatile unsigned vi = i;
        EXPECT_EQ(vi / 1u, i);
        EXPECT_EQ(vi % 1u, 0u);
    }
}

// ---------------------------------------------------------------------------
// Divisor sweep — exercises many different divisors
// ---------------------------------------------------------------------------

static void test_divisor_sweep() {
    // Signed: divide a constant by many divisors
    constexpr int N = 123456789;
    volatile int vn = N;

    EXPECT_EQ(vn / vol<int>{1}.get(), N);
    EXPECT_EQ(vn / vol<int>{2}.get(), N / 2);
    EXPECT_EQ(vn / vol<int>{3}.get(), N / 3);
    EXPECT_EQ(vn / vol<int>{4}.get(), N / 4);
    EXPECT_EQ(vn / vol<int>{5}.get(), N / 5);
    EXPECT_EQ(vn / vol<int>{6}.get(), N / 6);
    EXPECT_EQ(vn / vol<int>{7}.get(), N / 7);
    EXPECT_EQ(vn / vol<int>{8}.get(), N / 8);
    EXPECT_EQ(vn / vol<int>{9}.get(), N / 9);
    EXPECT_EQ(vn / vol<int>{10}.get(), N / 10);
    EXPECT_EQ(vn / vol<int>{11}.get(), N / 11);
    EXPECT_EQ(vn / vol<int>{12}.get(), N / 12);
    EXPECT_EQ(vn / vol<int>{13}.get(), N / 13);
    EXPECT_EQ(vn / vol<int>{15}.get(), N / 15);
    EXPECT_EQ(vn / vol<int>{16}.get(), N / 16);
    EXPECT_EQ(vn / vol<int>{17}.get(), N / 17);
    EXPECT_EQ(vn / vol<int>{25}.get(), N / 25);
    EXPECT_EQ(vn / vol<int>{32}.get(), N / 32);
    EXPECT_EQ(vn / vol<int>{64}.get(), N / 64);
    EXPECT_EQ(vn / vol<int>{100}.get(), N / 100);
    EXPECT_EQ(vn / vol<int>{127}.get(), N / 127);
    EXPECT_EQ(vn / vol<int>{128}.get(), N / 128);
    EXPECT_EQ(vn / vol<int>{255}.get(), N / 255);
    EXPECT_EQ(vn / vol<int>{256}.get(), N / 256);
    EXPECT_EQ(vn / vol<int>{1000}.get(), N / 1000);
    EXPECT_EQ(vn / vol<int>{10000}.get(), N / 10000);
    EXPECT_EQ(vn / vol<int>{65535}.get(), N / 65535);
    EXPECT_EQ(vn / vol<int>{65536}.get(), N / 65536);

    // Unsigned: same idea
    constexpr unsigned UN = 3141592653u;
    volatile unsigned vun = UN;

    EXPECT_EQ(vun / vol<unsigned>{1}.get(), UN / 1);
    EXPECT_EQ(vun / vol<unsigned>{2}.get(), UN / 2);
    EXPECT_EQ(vun / vol<unsigned>{3}.get(), UN / 3);
    EXPECT_EQ(vun / vol<unsigned>{5}.get(), UN / 5);
    EXPECT_EQ(vun / vol<unsigned>{7}.get(), UN / 7);
    EXPECT_EQ(vun / vol<unsigned>{10}.get(), UN / 10);
    EXPECT_EQ(vun / vol<unsigned>{16}.get(), UN / 16);
    EXPECT_EQ(vun / vol<unsigned>{100}.get(), UN / 100);
    EXPECT_EQ(vun / vol<unsigned>{255}.get(), UN / 255);
    EXPECT_EQ(vun / vol<unsigned>{256}.get(), UN / 256);
    EXPECT_EQ(vun / vol<unsigned>{1000}.get(), UN / 1000);
    EXPECT_EQ(vun / vol<unsigned>{65536}.get(), UN / 65536);
    EXPECT_EQ(vun / vol<unsigned>{0x10000}.get(), UN / 0x10000);
}

// ---------------------------------------------------------------------------
// Quotient-remainder invariant: a == (a/b)*b + (a%b)
// ---------------------------------------------------------------------------

static void test_invariant_signed() {
    // Pairs of (dividend, divisor) that stress the invariant
    constexpr struct { int a; int b; } cases[] = {
        {0, 1}, {1, 1}, {-1, 1}, {7, 3}, {-7, 3}, {7, -3}, {-7, -3},
        {INT_MAX, 1}, {INT_MAX, 2}, {INT_MAX, -1}, {INT_MAX, 3},
        {INT_MIN, 1}, {INT_MIN, 2}, {INT_MIN, -2}, {INT_MIN, 3},
        {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN},
        {123456789, 7}, {-123456789, 7}, {123456789, -7},
        {100, 3}, {-100, 3}, {100, -3}, {-100, -3},
        {1, INT_MAX}, {-1, INT_MAX}, {1, INT_MIN},
        {0x7FFFFFFF, 0x7FFFFFFE}, {0x7FFFFFFF, 127},
    };

    for (const auto& [a, b] : cases) {
        volatile int va = a, vb = b;
        int q = va / vb;
        int r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

static void test_invariant_unsigned() {
    constexpr struct { unsigned a; unsigned b; } cases[] = {
        {0, 1}, {1, 1}, {10, 3}, {255, 17}, {1000, 7},
        {UINT_MAX, 1}, {UINT_MAX, 2}, {UINT_MAX, 3}, {UINT_MAX, UINT_MAX},
        {0x80000000u, 1}, {0x80000000u, 3}, {0x80000000u, 0x80000000u},
        {0xDEADBEEFu, 10}, {0xDEADBEEFu, 256}, {0xDEADBEEFu, 65537u},
        {3141592653u, 7}, {3141592653u, 100}, {3141592653u, 65536},
        {1, UINT_MAX},
    };

    for (const auto& [a, b] : cases) {
        volatile unsigned va = a, vb = b;
        unsigned q = va / vb;
        unsigned r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

// ---------------------------------------------------------------------------
// Dividend close to divisor (quotient = 0 or 1 boundary)
// ---------------------------------------------------------------------------

static void test_boundary() {
    // Just below, at, and just above
    EXPECT_EQ(DIV( 99, 100), 0);
    EXPECT_EQ(MOD( 99, 100), 99);
    EXPECT_EQ(DIV(100, 100), 1);
    EXPECT_EQ(MOD(100, 100), 0);
    EXPECT_EQ(DIV(101, 100), 1);
    EXPECT_EQ(MOD(101, 100), 1);

    // Same for unsigned
    EXPECT_EQ(DIV( 99u, 100u), 0u);
    EXPECT_EQ(DIV(100u, 100u), 1u);
    EXPECT_EQ(DIV(101u, 100u), 1u);

    // Near overflow boundary
    EXPECT_EQ(DIV(INT_MAX, INT_MAX - 1), 1);
    EXPECT_EQ(MOD(INT_MAX, INT_MAX - 1), 1);
    EXPECT_EQ(DIV(UINT_MAX, UINT_MAX - 1), 1u);
    EXPECT_EQ(MOD(UINT_MAX, UINT_MAX - 1), 1u);
}

// ---------------------------------------------------------------------------
// All powers of 2 as divisor (1 through 2^30 for signed, 2^31 for unsigned)
// ---------------------------------------------------------------------------

static void test_all_power_of_two_divisors() {
    constexpr int DIVIDEND = 0x7ABCDEF0;

    for (int shift = 0; shift < 31; ++shift) {
        int divisor = 1 << shift;
        volatile int va = DIVIDEND, vd = divisor;
        EXPECT_EQ(va / vd, DIVIDEND / divisor);
        EXPECT_EQ(va % vd, DIVIDEND % divisor);
    }

    // Negative dividend
    constexpr int NEG_DIVIDEND = -0x7ABCDEF0;
    for (int shift = 0; shift < 31; ++shift) {
        int divisor = 1 << shift;
        volatile int va = NEG_DIVIDEND, vd = divisor;
        EXPECT_EQ(va / vd, NEG_DIVIDEND / divisor);
        EXPECT_EQ(va % vd, NEG_DIVIDEND % divisor);
    }

    // Unsigned powers of 2
    constexpr unsigned UDIVIDEND = 0xFABCDEF0u;
    for (int shift = 0; shift < 32; ++shift) {
        unsigned divisor = 1u << shift;
        volatile unsigned va = UDIVIDEND, vd = divisor;
        EXPECT_EQ(va / vd, UDIVIDEND / divisor);
        EXPECT_EQ(va % vd, UDIVIDEND % divisor);
    }
}

// ---------------------------------------------------------------------------
// Bit-width boundary: numerator with exactly N bits / small denominator
// Exercises every computed-jump entry in the unrolled loop
// ---------------------------------------------------------------------------

static void test_bit_width_boundaries() {
    // Unsigned: 2^N - 1 divided by 3 (exercises all bit positions)
    for (int bits = 1; bits <= 32; ++bits) {
        unsigned num = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
        volatile unsigned vn = num;
        volatile unsigned vd = 3u;
        unsigned q = vn / vd;
        unsigned r = vn % vd;
        EXPECT_EQ(q * 3u + r, num);
    }

    // Unsigned: exact powers of 2 as numerator / 3
    for (int bits = 0; bits < 32; ++bits) {
        unsigned num = 1u << bits;
        volatile unsigned vn = num;
        volatile unsigned vd = 3u;
        unsigned q = vn / vd;
        unsigned r = vn % vd;
        EXPECT_EQ(q * 3u + r, num);
    }

    // Signed: 2^N - 1 divided by 5 (positive, exercises bit alignment)
    for (int bits = 1; bits <= 31; ++bits) {
        int num = (1 << bits) - 1;
        volatile int vn = num;
        volatile int vd = 5;
        int q = vn / vd;
        int r = vn % vd;
        EXPECT_EQ(q * 5 + r, num);
    }
}

// ---------------------------------------------------------------------------
// Sequential numerator sweep: many consecutive numerators / fixed denominator
// Catches off-by-one in quotient transitions
// ---------------------------------------------------------------------------

static void test_sequential_sweep() {
    // Signed: -200 to +200 by 7
    for (int a = -200; a <= 200; ++a) {
        volatile int va = a;
        volatile int vd = 7;
        int q = va / vd;
        int r = va % vd;
        EXPECT_EQ(q * 7 + r, a);
    }

    // Unsigned: 0 to 400 by 13
    for (unsigned a = 0; a <= 400; ++a) {
        volatile unsigned va = a;
        volatile unsigned vd = 13u;
        unsigned q = va / vd;
        unsigned r = va % vd;
        EXPECT_EQ(q * 13u + r, a);
    }
}

// ---------------------------------------------------------------------------
// Large numerator / small denominator stress
// These maximize the number of iterations in the division loop
// ---------------------------------------------------------------------------

static void test_large_num_small_denom() {
    constexpr struct { unsigned a; unsigned b; unsigned q; unsigned r; } ucases[] = {
        {0xFFFFFFFFu, 1u, 0xFFFFFFFFu, 0u},
        {0xFFFFFFFFu, 2u, 0x7FFFFFFFu, 1u},
        {0xFFFFFFFFu, 3u, 0x55555555u, 0u},
        {0xFFFFFFFFu, 5u, 0x33333333u, 0u},
        {0xFFFFFFFFu, 7u, 613566756u, 3u},
        {0xFFFFFFFFu, 9u, 477218588u, 3u},
        {0xFFFFFFFFu, 10u, 429496729u, 5u},
        {0xFFFFFFFFu, 11u, 390451572u, 3u},
        {0xFFFFFFFEu, 2u, 0x7FFFFFFFu, 0u},
        {0x80000000u, 3u, 0x2AAAAAAAu, 2u},
        {0x80000001u, 3u, 0x2AAAAAABu, 0u},
    };

    for (const auto& [a, b, eq, er] : ucases) {
        volatile unsigned va = a, vb = b;
        EXPECT_EQ(va / vb, eq);
        EXPECT_EQ(va % vb, er);
    }

    constexpr struct { int a; int b; int q; int r; } icases[] = {
        {INT_MAX, 3, 715827882, 1},
        {INT_MAX, 5, 429496729, 2},
        {INT_MAX, 7, 306783378, 1},
        {INT_MAX, 9, 238609294, 1},
        {INT_MAX, 10, 214748364, 7},
        {INT_MAX, 11, 195225786, 1},
        {INT_MIN, 3, -715827882, -2},
        {INT_MIN, 5, -429496729, -3},
        {INT_MIN, 7, -306783378, -2},
        {INT_MIN, -3, 715827882, -2},
        {INT_MIN, -7, 306783378, -2},
    };

    for (const auto& [a, b, eq, er] : icases) {
        volatile int va = a, vb = b;
        EXPECT_EQ(va / vb, eq);
        EXPECT_EQ(va % vb, er);
    }
}

// ---------------------------------------------------------------------------
// Near-equal operands (quotient 1, various remainders)
// ---------------------------------------------------------------------------

static void test_near_equal() {
    // n / (n-k) for small k
    for (unsigned base = 100; base <= 100000; base *= 10) {
        for (unsigned k = 0; k <= 5; ++k) {
            unsigned n = base + k;
            volatile unsigned vn = n, vd = base;
            EXPECT_EQ(vn / vd * base + vn % vd, n);
        }
    }

    // Signed near-equal with negatives
    for (int base = 100; base <= 100000; base *= 10) {
        for (int k = -5; k <= 5; ++k) {
            int n = base + k;
            volatile int vn = n, vd = base;
            EXPECT_EQ(vn / vd * base + vn % vd, n);

            volatile int vnn = -n;
            EXPECT_EQ(vnn / vd * base + vnn % vd, -n);
        }
    }
}

// ---------------------------------------------------------------------------
// Denominator larger than numerator (quotient = 0)
// ---------------------------------------------------------------------------

static void test_denom_larger() {
    // Unsigned: small / large
    constexpr unsigned uvals[] = {0, 1, 2, 100, 0x7FFFFFFFu, 0xFFFFFFFEu};
    for (unsigned n : uvals) {
        volatile unsigned vn = n, vd = 0xFFFFFFFFu;
        EXPECT_EQ(vn / vd, (n == 0xFFFFFFFFu) ? 1u : 0u);
        EXPECT_EQ(vn % vd, (n == 0xFFFFFFFFu) ? 0u : n);
    }

    // Signed: small / large
    constexpr int ivals[] = {0, 1, -1, 100, -100, INT_MAX / 2};
    for (int n : ivals) {
        volatile int vn = n, vd = INT_MAX;
        EXPECT_EQ(vn / vd, 0);
        EXPECT_EQ(vn % vd, n);
    }
}

// ---------------------------------------------------------------------------
// Alternating bit patterns (0xAAAAAAAA, 0x55555555, etc.)
// Stress the carry chain in non-restoring division
// ---------------------------------------------------------------------------

static void test_alternating_bits() {
    constexpr struct { unsigned a; unsigned b; } cases[] = {
        {0xAAAAAAAAu, 0x55555555u},
        {0x55555555u, 0xAAAAAAAAu},
        {0xAAAAAAAAu, 3u},
        {0x55555555u, 3u},
        {0xAAAAAAAAu, 5u},
        {0x55555555u, 7u},
        {0xAAAAAAAAu, 0xFFu},
        {0x55555555u, 0xFFu},
        {0xAAAAAAAAu, 0xAAAAAAAAu},
        {0x55555555u, 0x55555555u},
        {0xF0F0F0F0u, 0x0F0F0F0Fu},
        {0x0F0F0F0Fu, 0xF0F0F0F0u},
        {0xFF00FF00u, 0x00FF00FFu},
    };

    for (const auto& [a, b] : cases) {
        volatile unsigned va = a, vb = b;
        unsigned q = va / vb;
        unsigned r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

// ---------------------------------------------------------------------------
// 64/32 path: denom_hi == 0, num_hi != 0 (exercises .Luluidiv_checked)
// ---------------------------------------------------------------------------

static void test_u64_div32_basic() {
    using u64 = std::uint64_t;

    // Large numerator / small denominator -- forces 64/32 path
    EXPECT_EQ(DIV(u64{0x100000000ULL}, u64{1}), u64{0x100000000ULL});
    EXPECT_EQ(MOD(u64{0x100000000ULL}, u64{1}), u64{0});
    EXPECT_EQ(DIV(u64{0x100000000ULL}, u64{2}), u64{0x80000000ULL});
    EXPECT_EQ(MOD(u64{0x100000000ULL}, u64{2}), u64{0});
    EXPECT_EQ(DIV(u64{0x100000000ULL}, u64{3}), u64{0x55555555ULL});
    EXPECT_EQ(MOD(u64{0x100000000ULL}, u64{3}), u64{1});
    EXPECT_EQ(DIV(u64{0x100000001ULL}, u64{3}), u64{0x55555555ULL});
    EXPECT_EQ(MOD(u64{0x100000001ULL}, u64{3}), u64{2});

    // UINT64_MAX / small denoms -- max iterations, carry chain stress
    constexpr u64 UMAX = UINT64_MAX;
    EXPECT_EQ(DIV(UMAX, u64{3}), u64{6148914691236517205ULL});
    EXPECT_EQ(MOD(UMAX, u64{3}), u64{0});
    EXPECT_EQ(DIV(UMAX, u64{5}), u64{3689348814741910323ULL});
    EXPECT_EQ(MOD(UMAX, u64{5}), u64{0});
    EXPECT_EQ(DIV(UMAX, u64{7}), u64{2635249153387078802ULL});
    EXPECT_EQ(MOD(UMAX, u64{7}), u64{1});
    EXPECT_EQ(DIV(UMAX, u64{9}), u64{2049638230412172401ULL});
    EXPECT_EQ(MOD(UMAX, u64{9}), u64{6});
    EXPECT_EQ(DIV(UMAX, u64{10}), u64{1844674407370955161ULL});
    EXPECT_EQ(MOD(UMAX, u64{10}), u64{5});
    EXPECT_EQ(DIV(UMAX, u64{255}), u64{72340172838076673ULL});
    EXPECT_EQ(MOD(UMAX, u64{255}), u64{0});
    EXPECT_EQ(DIV(UMAX, u64{256}), u64{72057594037927935ULL});
    EXPECT_EQ(MOD(UMAX, u64{256}), u64{255});
    EXPECT_EQ(DIV(UMAX, u64{65536}), u64{281474976710655ULL});
    EXPECT_EQ(MOD(UMAX, u64{65536}), u64{65535});

    // Specific regression: denom=1 with large num (r_hi==0 fast path)
    EXPECT_EQ(DIV(u64{0xDEADBEEF12345678ULL}, u64{1}), u64{0xDEADBEEF12345678ULL});
    EXPECT_EQ(MOD(u64{0xDEADBEEF12345678ULL}, u64{1}), u64{0});
}

// ---------------------------------------------------------------------------
// 64/32 path: bit-width sweep (exercises every computed-jump entry point)
// ---------------------------------------------------------------------------

static void test_u64_div32_bit_sweep() {
    using u64 = std::uint64_t;

    // 2^N / 3 for N=33..63 -- each hits a different computed-jump offset
    for (int n = 33; n <= 63; ++n) {
        u64 num = u64{1} << n;
        volatile u64 vn = num;
        volatile u64 vd = 3;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q * 3 + r, num);
    }

    // (2^N - 1) / 7 for N=33..64 -- all-ones patterns at each width
    for (int n = 33; n <= 63; ++n) {
        u64 num = (u64{1} << n) - 1;
        volatile u64 vn = num;
        volatile u64 vd = 7;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q * 7 + r, num);
    }
    // N=64 case
    {
        u64 num = UINT64_MAX;
        volatile u64 vn = num;
        volatile u64 vd = 7;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q * 7 + r, num);
    }
}

// ---------------------------------------------------------------------------
// 64/32 path: divisor sweep with fixed large numerator
// ---------------------------------------------------------------------------

static void test_u64_div32_divisor_sweep() {
    using u64 = std::uint64_t;

    constexpr u64 BIG = 0xFEDCBA9876543210ULL;
    constexpr u64 divisors[] = {
        1, 2, 3, 5, 7, 9, 10, 11, 13, 16, 17, 25, 32, 64,
        100, 127, 128, 255, 256, 1000, 10000, 65535, 65536,
        0x7FFFFFFF, 0x80000000, 0xFFFFFFFF
    };

    for (u64 d : divisors) {
        volatile u64 vn = BIG, vd = d;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q * d + r, BIG);
    }
}

// ---------------------------------------------------------------------------
// 64/64 path: both operands have nonzero high words (.Luldiv64_checked)
// ---------------------------------------------------------------------------

static void test_u64_div64_basic() {
    using u64 = std::uint64_t;

    // Equal operands
    constexpr u64 V = 0x123456789ABCDEF0ULL;
    EXPECT_EQ(DIV(V, V), u64{1});
    EXPECT_EQ(MOD(V, V), u64{0});

    // num > denom, both > 32 bits
    EXPECT_EQ(DIV(u64{0xFFFFFFFFFFFFFFFFULL}, u64{0x100000000ULL}), u64{0xFFFFFFFFULL});
    EXPECT_EQ(MOD(u64{0xFFFFFFFFFFFFFFFFULL}, u64{0x100000000ULL}), u64{0xFFFFFFFFULL});

    EXPECT_EQ(DIV(u64{0xFFFFFFFFFFFFFFFFULL}, u64{0x8000000000000000ULL}), u64{1});
    EXPECT_EQ(MOD(u64{0xFFFFFFFFFFFFFFFFULL}, u64{0x8000000000000000ULL}), u64{0x7FFFFFFFFFFFFFFFULL});

    // Quotient with many bits set
    EXPECT_EQ(DIV(u64{0xFFFFFFFE00000001ULL}, u64{0xFFFFFFFFULL}), u64{0xFFFFFFFFULL});
    EXPECT_EQ(MOD(u64{0xFFFFFFFE00000001ULL}, u64{0xFFFFFFFFULL}), u64{0});

    // Near-equal 64-bit operands
    EXPECT_EQ(DIV(u64{0x8000000000000001ULL}, u64{0x8000000000000000ULL}), u64{1});
    EXPECT_EQ(MOD(u64{0x8000000000000001ULL}, u64{0x8000000000000000ULL}), u64{1});

    // Denominator just above 32 bits
    EXPECT_EQ(DIV(u64{0xAAAAAAAAAAAAAAAAULL}, u64{0x100000001ULL}), u64{0xAAAAAAAAULL});
    // num < denom -- quotient = 0
    EXPECT_EQ(DIV(u64{0x100000000ULL}, u64{0x100000001ULL}), u64{0});
    EXPECT_EQ(MOD(u64{0x100000000ULL}, u64{0x100000001ULL}), u64{0x100000000ULL});
}

// ---------------------------------------------------------------------------
// 64/64 path: invariant check with varied operand sizes
// ---------------------------------------------------------------------------

static void test_u64_div64_invariant() {
    using u64 = std::uint64_t;

    constexpr struct { u64 a; u64 b; } cases[] = {
        // Both operands just above 32-bit boundary
        {0x100000000ULL, 0x100000000ULL},
        {0x100000001ULL, 0x100000000ULL},
        {0x1FFFFFFFFULL, 0x100000000ULL},
        {0x200000000ULL, 0x100000001ULL},
        // Large operands, denom slightly smaller
        {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL},
        {0xFFFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL},
        {0xFFFFFFFFFFFFFFFFULL, 0x100000000ULL},
        {0xDEADBEEFCAFEBABEULL, 0x123456789ULL},
        {0xDEADBEEFCAFEBABEULL, 0xCAFEBABEDEADBEEFULL},
        // Alternating bit patterns
        {0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL},
        {0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL},
        // Powers of 2
        {0x8000000000000000ULL, 0x4000000000000000ULL},
        {0x8000000000000000ULL, 0x8000000000000000ULL},
        {0x4000000000000000ULL, 0x2000000000000000ULL},
        // Denom with only high word set
        {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFF00000000ULL},
        {0xFFFFFFFF00000000ULL, 0xFFFFFFFF00000000ULL},
    };

    for (const auto& [a, b] : cases) {
        volatile u64 va = a, vb = b;
        u64 q = va / vb;
        u64 r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

// ---------------------------------------------------------------------------
// 64/64 path: bit-width sweep -- shift denom to hit every alignment
// ---------------------------------------------------------------------------

static void test_u64_div64_bit_sweep() {
    using u64 = std::uint64_t;

    // UINT64_MAX / (1 << N) for N=32..63 -- each denom_hi has a different leading bit
    for (int n = 32; n <= 63; ++n) {
        u64 denom = u64{1} << n;
        volatile u64 vn = UINT64_MAX, vd = denom;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q * denom + r, u64{UINT64_MAX});
    }

    // (1 << 63) / (1 << N) for N=32..62
    for (int n = 32; n <= 62; ++n) {
        u64 num = u64{1} << 63;
        u64 denom = u64{1} << n;
        volatile u64 vn = num, vd = denom;
        u64 q = vn / vd;
        u64 r = vn % vd;
        EXPECT_EQ(q, u64{1} << (63 - n));
        EXPECT_EQ(r, u64{0});
    }
}

// ---------------------------------------------------------------------------
// Signed 64-bit: comprehensive sign combinations + large values
// ---------------------------------------------------------------------------

static void test_signed_64_extended() {
    using i64 = std::int64_t;

    constexpr i64 I64MAX = INT64_MAX;
    constexpr i64 I64MIN = INT64_MIN;

    // Negation of large values
    EXPECT_EQ(DIV(i64{-1000000000000LL}, i64{7}), i64{-142857142857LL});
    EXPECT_EQ(MOD(i64{-1000000000000LL}, i64{7}), i64{-1});
    EXPECT_EQ(DIV(i64{1000000000000LL}, i64{-7}), i64{-142857142857LL});
    EXPECT_EQ(MOD(i64{1000000000000LL}, i64{-7}), i64{1});
    EXPECT_EQ(DIV(i64{-1000000000000LL}, i64{-7}), i64{142857142857LL});
    EXPECT_EQ(MOD(i64{-1000000000000LL}, i64{-7}), i64{-1});

    // MAX / negative, MIN / negative
    EXPECT_EQ(DIV(I64MAX, i64{-1}), -I64MAX);
    EXPECT_EQ(MOD(I64MAX, i64{-1}), i64{0});
    EXPECT_EQ(DIV(I64MAX, i64{-2}), i64{-4611686018427387903LL});
    EXPECT_EQ(MOD(I64MAX, i64{-2}), i64{1});
    EXPECT_EQ(DIV(I64MIN, i64{-1}), I64MIN); // UB in C, but our asm returns this
    EXPECT_EQ(DIV(I64MIN, i64{-2}), i64{4611686018427387904LL});
    EXPECT_EQ(MOD(I64MIN, i64{-2}), i64{0});

    // Large dividend / large divisor (64/64 signed path)
    EXPECT_EQ(DIV(I64MAX, I64MIN), i64{0});
    EXPECT_EQ(MOD(I64MAX, I64MIN), I64MAX);
    EXPECT_EQ(DIV(I64MIN, I64MAX), i64{-1});
    EXPECT_EQ(MOD(I64MIN, I64MAX), i64{-1});

    // Negative / negative large
    EXPECT_EQ(DIV(i64{-0x7000000000000000LL}, i64{-0x3000000000000000LL}), i64{2});
    EXPECT_EQ(MOD(i64{-0x7000000000000000LL}, i64{-0x3000000000000000LL}), i64{-0x1000000000000000LL});

    // 64-bit division by 32-bit negative
    EXPECT_EQ(DIV(i64{0x100000000LL}, i64{-1}), i64{-0x100000000LL});
    EXPECT_EQ(DIV(i64{-0x100000000LL}, i64{3}), i64{-1431655765LL});
    EXPECT_EQ(MOD(i64{-0x100000000LL}, i64{3}), i64{-1});
}

// ---------------------------------------------------------------------------
// Signed 64-bit: invariant q*b+r==a with many sign/size combos
// ---------------------------------------------------------------------------

static void test_signed_64_invariant() {
    using i64 = std::int64_t;

    constexpr struct { i64 a; i64 b; } cases[] = {
        {0, 1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
        {INT64_MAX, 1}, {INT64_MAX, -1}, {INT64_MAX, 2}, {INT64_MAX, -2},
        {INT64_MIN, 1}, {INT64_MIN, 2}, {INT64_MIN, -2}, {INT64_MIN, 3},
        {INT64_MAX, INT64_MAX}, {INT64_MIN, INT64_MIN},
        {INT64_MAX, INT64_MIN}, {INT64_MIN, INT64_MAX},
        // 64/32 signed
        {1000000000000LL, 7}, {-1000000000000LL, 7},
        {1000000000000LL, -7}, {-1000000000000LL, -7},
        {0x7FFFFFFFFFFFFFFFLL, 3}, {-0x7FFFFFFFFFFFFFFFLL, 3},
        // 64/64 signed (both > 32 bits after abs)
        {0x7FFFFFFFFFFFFFFFLL, 0x100000000LL},
        {-0x7FFFFFFFFFFFFFFFLL, 0x100000000LL},
        {0x7FFFFFFFFFFFFFFFLL, -0x100000000LL},
        {-0x7FFFFFFFFFFFFFFFLL, -0x100000000LL},
        // Denom just above 32 bits
        {0x7000000000000000LL, 0x3000000000000000LL},
        {-0x7000000000000000LL, 0x3000000000000000LL},
    };

    for (const auto& [a, b] : cases) {
        volatile i64 va = a, vb = b;
        i64 q = va / vb;
        i64 r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

// ---------------------------------------------------------------------------
// Bridge path: num_hi == 0, denom_hi == 0 (.Lbridge_32 -> __aeabi_uidivmod)
// ---------------------------------------------------------------------------

static void test_u64_bridge_32() {
    using u64 = std::uint64_t;

    // Both values fit in 32 bits but are typed as u64
    EXPECT_EQ(DIV(u64{0}, u64{1}), u64{0});
    EXPECT_EQ(DIV(u64{1}, u64{1}), u64{1});
    EXPECT_EQ(DIV(u64{100}, u64{7}), u64{14});
    EXPECT_EQ(MOD(u64{100}, u64{7}), u64{2});
    EXPECT_EQ(DIV(u64{0xFFFFFFFF}, u64{1}), u64{0xFFFFFFFF});
    EXPECT_EQ(DIV(u64{0xFFFFFFFF}, u64{3}), u64{0x55555555});
    EXPECT_EQ(MOD(u64{0xFFFFFFFF}, u64{3}), u64{0});
    EXPECT_EQ(DIV(u64{0xFFFFFFFF}, u64{0xFFFFFFFF}), u64{1});
    EXPECT_EQ(MOD(u64{0xFFFFFFFF}, u64{0xFFFFFFFF}), u64{0});
    EXPECT_EQ(DIV(u64{1}, u64{0xFFFFFFFF}), u64{0});
    EXPECT_EQ(MOD(u64{1}, u64{0xFFFFFFFF}), u64{1});
}

// ---------------------------------------------------------------------------
// 64-bit alternating bit patterns (carry chain stress)
// ---------------------------------------------------------------------------

static void test_u64_alternating_bits() {
    using u64 = std::uint64_t;

    constexpr struct { u64 a; u64 b; } cases[] = {
        {0xAAAAAAAAAAAAAAAAULL, 3},
        {0x5555555555555555ULL, 3},
        {0xAAAAAAAAAAAAAAAAULL, 7},
        {0x5555555555555555ULL, 7},
        {0xAAAAAAAAAAAAAAAAULL, 0xFF},
        {0x5555555555555555ULL, 0xFF},
        {0xAAAAAAAAAAAAAAAAULL, 0xFFFFFFFF},
        {0x5555555555555555ULL, 0xFFFFFFFF},
        {0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL},
        {0xFF00FF00FF00FF00ULL, 0x00FF00FF00FF00FFULL},
        {0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL},
        {0xFFFF0000FFFF0000ULL, 0x0000FFFF0000FFFFULL},
    };

    for (const auto& [a, b] : cases) {
        volatile u64 va = a, vb = b;
        u64 q = va / vb;
        u64 r = va % vb;
        EXPECT_EQ(q * b + r, a);
    }
}

// ---------------------------------------------------------------------------
// 64-bit near-equal values (quotient boundary at 0/1)
// ---------------------------------------------------------------------------

static void test_u64_near_equal() {
    using u64 = std::uint64_t;

    // n / (n-k) and n / (n+k) for small k
    constexpr u64 bases[] = {
        0x100000000ULL, 0x100000001ULL, 0x7FFFFFFFFFFFFFFFULL,
        0x8000000000000000ULL, 0xDEADBEEFCAFEBABEULL,
    };
    for (u64 base : bases) {
        for (u64 k = 0; k <= 3; ++k) {
            if (base + k < base) break; // overflow
            u64 n = base + k;
            volatile u64 vn = n, vd = base;
            u64 q = vn / vd;
            u64 r = vn % vd;
            EXPECT_EQ(q * base + r, n);
        }
    }
}

// ---------------------------------------------------------------------------
// 64-bit denom larger than num (quotient = 0, remainder = num)
// ---------------------------------------------------------------------------

static void test_u64_denom_larger() {
    using u64 = std::uint64_t;

    constexpr struct { u64 n; u64 d; } cases[] = {
        {1, 2},
        {0xFFFFFFFF, 0x100000000ULL},
        {0x100000000ULL, 0x100000001ULL},
        {0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL},
        {0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL},
        {1, 0xFFFFFFFFFFFFFFFFULL},
        {0, 1},
        {0, 0xFFFFFFFFFFFFFFFFULL},
    };

    for (const auto& [n, d] : cases) {
        volatile u64 vn = n, vd = d;
        EXPECT_EQ(vn / vd, u64{0});
        EXPECT_EQ(vn % vd, n);
    }
}

// ---------------------------------------------------------------------------

int main() {
    test_signed_32_basic();
    test_signed_32_negative();
    test_signed_32_power_of_two();
    test_signed_32_extreme();

    test_unsigned_32_basic();
    test_unsigned_32_power_of_two();
    test_unsigned_32_extreme();

    test_signed_64();
    test_unsigned_64();

    test_signed_narrow();
    test_unsigned_narrow();
    test_mixed_width();

    test_identity_and_zero();
    test_divisor_sweep();

    test_invariant_signed();
    test_invariant_unsigned();

    test_boundary();
    test_all_power_of_two_divisors();

    test_bit_width_boundaries();
    test_sequential_sweep();
    test_large_num_small_denom();
    test_near_equal();
    test_denom_larger();
    test_alternating_bits();

    // 64-bit path-specific tests
    test_u64_div32_basic();
    test_u64_div32_bit_sweep();
    test_u64_div32_divisor_sweep();
    test_u64_div64_basic();
    test_u64_div64_invariant();
    test_u64_div64_bit_sweep();
    test_signed_64_extended();
    test_signed_64_invariant();
    test_u64_bridge_32();
    test_u64_alternating_bits();
    test_u64_near_equal();
    test_u64_denom_larger();

    test::exit(test::failures());
}
