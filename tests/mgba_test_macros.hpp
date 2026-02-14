#pragma once

// Macros for mgba test assertions
// These depend on the ::test namespace functions in mgba_test.hpp

#define EXPECT_ZERO(expr)                                                   \
    do {                                                                    \
        ::test::expect_zero_impl((expr), #expr, __FILE__, __LINE__, false); \
    } while (0)

#define ASSERT_ZERO(expr)                                                  \
    do {                                                                   \
        ::test::expect_zero_impl((expr), #expr, __FILE__, __LINE__, true); \
    } while (0)

#define EXPECT_NZ(expr)                                                   \
    do {                                                                  \
        ::test::expect_nz_impl((expr), #expr, __FILE__, __LINE__, false); \
    } while (0)

#define ASSERT_NZ(expr)                                                  \
    do {                                                                 \
        ::test::expect_nz_impl((expr), #expr, __FILE__, __LINE__, true); \
    } while (0)

// Modern C++20/23 approach using __VA_OPT__
// This handles template arguments with commas properly
#define EXPECT_EQ(...) ::test::expect_eq_impl(__VA_ARGS__ __VA_OPT__(, ) "LHS", "RHS", __FILE__, __LINE__)

#define ASSERT_EQ(...) ::test::expect_eq_impl(__VA_ARGS__ __VA_OPT__(, ) "LHS", "RHS", __FILE__, __LINE__, true)

// Boolean assertion macros
#define ASSERT_TRUE(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            test::record_failure_and_abort(#cond " is not true", __FILE__, __LINE__, 0); \
        } else {                                                                         \
            test::passes()++;                                                            \
        }                                                                                \
    } while (0)

#define ASSERT_FALSE(cond)                                                                \
    do {                                                                                  \
        if (cond) {                                                                       \
            test::record_failure_and_abort(#cond " is not false", __FILE__, __LINE__, 1); \
        } else {                                                                          \
            test::passes()++;                                                             \
        }                                                                                 \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            test::record_failure(#cond " is not true", __FILE__, __LINE__, 0); \
        } else {                                                               \
            test::passes()++;                                                  \
        }                                                                      \
    } while (0)

#define EXPECT_FALSE(cond)                                                      \
    do {                                                                        \
        if (cond) {                                                             \
            test::record_failure(#cond " is not false", __FILE__, __LINE__, 1); \
        } else {                                                                \
            test::passes()++;                                                   \
        }                                                                       \
    } while (0)
