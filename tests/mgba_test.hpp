#pragma once

#include <gba/peripherals>

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <vector>

#include <mgba.h>
#include "mgba_test_macros.hpp"

namespace test {
    template<typename F>
    inline decltype(auto) do_not_optimize(F&& f) noexcept {
        asm volatile("" ::: "memory");
        if constexpr (std::is_same_v<void, std::invoke_result_t<F>>) {
            std::forward<F>(f)();
            asm volatile("" ::: "memory");
            return;
        } else {
            auto res = std::forward<F>(f)();
            asm volatile("" ::: "memory");
            return res;
        }
    }

    [[gnu::noreturn]]
    inline void exit(int code) noexcept {
        register auto r0 asm("r0") = code;
        asm volatile("swi 0xFF << ((1f - . == 4) * -16); 1:" ::"r"(r0));
        __builtin_unreachable();
    }

    inline int& failures() {
        static int f = 0;
        return f;
    }

    inline int& passes() {
        static int p = 0;
        return p;
    }

    struct LoggerSession {
        bool ok;
        LoggerSession() : ok(mgba_open()) {}
        ~LoggerSession() {
            if (ok) {
                mgba_close();
            }
        }
    };

    template<typename Fn>
    inline void with_logger(Fn&& fn) {
        if (const LoggerSession s; s.ok) {
            fn();
        }
    }

    // Helper: mgba_printf with 256-char buffer
    inline void mgba_printf_buf(int log_level, const char* fmt, ...) {
        char msgbuf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
        va_end(args);
        mgba_printf(log_level, "%s", msgbuf);
    }

    inline void record_success(const char* expr, const char* file, const int line, const unsigned int value) {
        passes()++;
    }

    inline void record_failure(const char* expr, const char* file, const int line, const unsigned int value) {
        failures()++;
        with_logger([&] {
            mgba_printf_buf(MGBA_LOG_FATAL, "[FAIL] %s (val=0x%X / %u) @ %s:%d (fail %d)", expr, value, value, file,
                            line, failures());
        });
    }

    [[gnu::noreturn]]
    inline void finalize() {
        with_logger([] static {
            if (failures()) {
                mgba_printf_buf(MGBA_LOG_ERROR, "RESULT: %d failure(s), %d pass(es)", failures(), passes());
            } else {
                mgba_printf_buf(MGBA_LOG_INFO, "RESULT: OK (%d assertion(s))", passes());
            }
        });
        exit(failures());
    }

    inline struct finalizer {
        ~finalizer() { finalize(); }
    } _auto_finalizer;

    [[gnu::noreturn]]
    inline void record_failure_and_abort(const char* expr, const char* file, int line, unsigned long long value) {
        record_failure(expr, file, line, value);
        with_logger([] static { mgba_printf_buf(MGBA_LOG_FATAL, "ABORTING: ASSERT failed; terminating test early"); });
        finalize();
    }

    // Helper trait to detect std::array
    template<typename T>
    struct is_std_array : std::false_type {};
    template<typename T, std::size_t N>
    struct is_std_array<std::array<T, N>> : std::true_type {};

    // Helper trait to detect types with .size() and operator[]
    template<typename T, typename = void>
    struct has_array_like : std::false_type {};
    template<typename T>
    struct has_array_like<
        T, std::void_t<decltype(std::declval<T>().size()), decltype(std::declval<T>()[static_cast<size_t>(0)])>>
        : std::true_type {};

    // Helper trait to detect indexable types (std::array, array-like, or built-in arrays)
    template<typename T>
    struct is_indexable : std::bool_constant<is_std_array<T>::value || has_array_like<T>::value || std::is_array_v<T>> {
    };

    // Trait to detect if a type has a .value() method
    template<typename T, typename = void>
    struct has_value_method : std::false_type {};
    template<typename T>
    struct has_value_method<T, std::void_t<decltype(std::declval<T>().value())>> : std::true_type {};

    // Trait to detect if a type is gba::registral<...>
    template<typename T>
    struct is_registral : std::false_type {};
    template<typename U>
    struct is_registral<gba::registral<U>> : std::true_type {};

    // Trait to detect if a type is gba::fixed<...>
    template<typename T>
    struct is_fixed_point : std::false_type {};
    template<typename Rep, unsigned int FracBits, typename IntermediateRep>
    struct is_fixed_point<gba::fixed<Rep, FracBits, IntermediateRep>> : std::true_type {};

    // Helper to format fixed-point value as decimal
    template<typename T>
    inline void format_fixed_point(char* buf, size_t bufsize, const T& value) {
        if constexpr (is_fixed_point<T>::value) {
            // Convert to integer and fractional parts manually
            auto raw = __builtin_bit_cast(typename gba::fixed_point_traits<T>::rep, value);
            constexpr auto frac_bits = gba::fixed_point_traits<T>::frac_bits;

            // Extract integer and fractional parts
            using rep_type = typename gba::fixed_point_traits<T>::rep;
            constexpr rep_type frac_mask = (1 << frac_bits) - 1;

            // Handle signed types
            bool is_negative = false;
            rep_type abs_raw = raw;
            if constexpr (std::is_signed_v<rep_type>) {
                if (raw < 0) {
                    is_negative = true;
                    abs_raw = -raw;
                }
            }

            auto integer_part = abs_raw >> frac_bits;
            auto frac_part = abs_raw & frac_mask;

            // Convert fractional part to decimal (4 digits)
            // frac_part / (1 << frac_bits) * 10000
            unsigned int frac_decimal = (static_cast<unsigned int>(frac_part) * 10000) >> frac_bits;

            // Format as integer.fractional (0xhex)
            snprintf(buf, bufsize, "%s%u.%04u (0x%X)", is_negative ? "-" : "", static_cast<unsigned int>(integer_part),
                     frac_decimal, static_cast<unsigned int>(raw));
        } else {
            snprintf(buf, bufsize, "0x%X", static_cast<unsigned int>(value));
        }
    }

    // Helper to select log level
    inline int get_log_level(bool abort) {
        return abort ? MGBA_LOG_FATAL : MGBA_LOG_ERROR;
    }

    // Registral overload: only for gba::registral types
    template<typename RegArray, typename PlainArray>
    inline std::enable_if_t<is_registral<RegArray>::value &&
                            (std::is_array_v<PlainArray> || is_std_array<PlainArray>::value ||
                             has_array_like<PlainArray>::value)>
    expect_eq_impl(const RegArray& reg, const PlainArray& arr, const char* exprA, const char* exprB, const char* file,
                   int line, bool abort = false) {
        int log_level = get_log_level(abort);
        size_t size = 0;
        if constexpr (is_std_array<PlainArray>::value || has_array_like<PlainArray>::value) {
            size = arr.size();
        } else if constexpr (std::is_array_v<PlainArray>) {
            size = std::extent<PlainArray>::value;
        }
        bool failed = false;
        for (size_t i = 0; i < size; ++i) {
            auto reg_val = reg[i].value();
            auto arr_val = arr[i];
            std::uintptr_t offset = 0;
            if constexpr (requires {
                              reg.m_address;
                              reg.m_stride;
                          }) {
                offset = reg.m_address + i * reg.m_stride;
            } else {
                offset = i * sizeof(std::remove_cv_t<decltype(reg_val)>);
            }
            if (reg_val != arr_val) {
                failed = true;
                failures()++;
                if (const LoggerSession s; s.ok) {
                    mgba_printf_buf(log_level, "[FAIL] %s[%u] == %s[%u] @ 0x%X (LHS=0x%X, RHS=0x%X) @ %s:%d (fail %d)",
                                    exprA, i, exprB, i, static_cast<unsigned int>(offset),
                                    static_cast<unsigned int>(reg_val), static_cast<unsigned int>(arr_val), file, line,
                                    failures());
                }
            }
        }
        if (!failed) {
            passes()++;
        } else if (abort) {
            if (const LoggerSession s; s.ok) {
                mgba_printf_buf(MGBA_LOG_FATAL, "ABORTING: ASSERT failed; terminating test early");
            }
            finalize();
        }
    }

    // Indexable overload: for non-registral types
    template<typename A, typename B>
    inline std::enable_if_t<!is_registral<A>::value && is_indexable<A>::value && is_indexable<B>::value> expect_eq_impl(
        const A& a, const B& b, const char* exprA, const char* exprB, const char* file, int line, bool abort = false) {
        auto compare = [&](auto v, auto w) {
            return v == w;
        };
        auto fail_report = [](const char* exprA, const char* exprB, const char* idxbuf, size_t offset, auto lhs,
                              auto rhs, const char* file, int line, int fails, int log_level) {
            mgba_printf_buf(log_level, "[FAIL] %s%s == %s%s @ 0x%X (LHS=0x%X, RHS=0x%X) @ %s:%d (fail %d)", exprA,
                            idxbuf, exprB, idxbuf, static_cast<unsigned int>(offset), static_cast<unsigned int>(lhs),
                            static_cast<unsigned int>(rhs), file, line, fails);
        };
        size_t sizeA = std::distance(std::begin(a), std::end(a));
        size_t sizeB = std::distance(std::begin(b), std::end(b));
        if (sizeA != sizeB) {
            failures()++;
            if (const LoggerSession s; s.ok) {
                mgba_printf_buf(MGBA_LOG_ERROR, "[FAIL] %s and %s have different lengths (%u vs %u) @ %s:%d (fail %d)",
                                exprA, exprB, sizeA, sizeB, file, line, failures());
            }
            if (abort) finalize();
            return;
        }
        for (size_t i = 0; i < sizeA; ++i) {
            std::vector<size_t> indices = {i};
            expect_elements_impl_recursive(
                a[i], exprA, exprB, file, line, abort, indices, [&](auto lhs) { return compare(lhs, b[i]); },
                [&](const char* exprA, const char* exprB, const char* idxbuf, size_t offset, auto lhs, const char* file,
                    int line, int fails, int log_level) {
                    fail_report(exprA, exprB, idxbuf, offset, lhs, b[i], file, line, fails, log_level);
                });
        }
    }

    // Scalar fallback overload: for non-indexable, non-registral types
    template<typename A, typename B>
    inline std::enable_if_t<!is_registral<A>::value && !is_indexable<A>::value> expect_eq_impl(
        const A& a, const B& b, const char* exprA, const char* exprB, const char* file, int line, bool abort = false) {
        int log_level = get_log_level(abort);
        if (!(a == b)) {
            failures()++;
            if (const LoggerSession s; s.ok) {
                // Format values differently for fixed-point types
                char lhs_buf[64];
                char rhs_buf[64];
                format_fixed_point(lhs_buf, sizeof(lhs_buf), a);
                format_fixed_point(rhs_buf, sizeof(rhs_buf), b);
                mgba_printf_buf(log_level, "[FAIL] %s == %s (LHS=%s, RHS=%s) @ %s:%d (fail %d)", exprA, exprB, lhs_buf,
                                rhs_buf, file, line, failures());
            }
            if (abort) {
                if (const LoggerSession s; s.ok) {
                    mgba_printf_buf(MGBA_LOG_FATAL, "ABORTING: ASSERT failed; terminating test early");
                }
                finalize();
            }
        } else {
            passes()++;
        }
    }

    // Helper to build index string from vector
    inline void build_indices_str(char* buf, size_t bufsize, const std::vector<size_t>& indices) {
        size_t offset = 0;
        for (size_t idx : indices) {
            offset += snprintf(buf + offset, bufsize - offset, "[%u]", idx);
        }
    }

    // Helper to compute strides for N-dimensional arrays
    template<typename T, size_t... Dims>
    constexpr size_t compute_offset_from_indices(const size_t (&indices)[sizeof...(Dims)],
                                                 std::index_sequence<Dims...>) {
        size_t offset = 0;
        size_t stride = 1;
        size_t dims[] = {std::extent<T, Dims>::value...};
        constexpr size_t N = sizeof...(Dims);
        for (int i = N - 1; i >= 0; --i) {
            offset += indices[i] * stride;
            stride *= dims[i];
        }
        return offset * sizeof(typename std::remove_all_extents<T>::type);
    }

    // DRY: shared recursive element-wise comparison for EQ and ZERO
    template<typename T, typename CompareFn, typename FailFn>
    void expect_elements_impl_recursive(const T& value, const char* exprA, const char* exprB, const char* file,
                                        int line, bool abort, std::vector<size_t> indices, CompareFn compare,
                                        FailFn fail_report) {
        int log_level = get_log_level(abort);
        if constexpr (is_indexable<T>::value) {
            size_t size = 0;
            if constexpr (is_std_array<T>::value || has_array_like<T>::value) {
                size = value.size();
            } else if constexpr (std::is_array_v<T>) {
                size = std::extent<T>::value;
            }
            for (size_t idx = 0; idx < size; ++idx) {
                auto new_indices = indices;
                new_indices.push_back(idx);
                expect_elements_impl_recursive(value[idx], exprA, exprB, file, line, abort, new_indices, compare,
                                               fail_report);
            }
        } else if constexpr (has_value_method<T>::value) {
            expect_elements_impl_recursive(value.value(), exprA, exprB, file, line, abort, indices, compare,
                                           fail_report);
        } else if constexpr (std::is_arithmetic_v<T>) {
            size_t offset = 0;
            if (!indices.empty()) {
                offset = indices.back() * sizeof(T);
                if (indices.size() > 1) {
                    size_t stride = 1;
                    for (size_t i = indices.size() - 1; i > 0; --i) {
                        stride *= indices[i];
                        offset += indices[i - 1] * stride * sizeof(T);
                    }
                }
            }
            if (!compare(value)) {
                failures()++;
                if (const LoggerSession s; s.ok) {
                    char idxbuf[32] = "";
                    build_indices_str(idxbuf, sizeof(idxbuf), indices);
                    mgba_printf_buf(log_level, "[FAIL] %s%s @ 0x%X == 0 (val=0x%X) @ %s:%d (fail %d)", exprA, idxbuf,
                                    static_cast<unsigned int>(offset), static_cast<unsigned int>(value), file, line,
                                    failures());
                }
                if (abort) {
                    if (const LoggerSession s; s.ok) {
                        mgba_printf_buf(MGBA_LOG_FATAL, "ABORTING: ASSERT failed; terminating test early");
                    }
                    finalize();
                }
            } else {
                passes()++;
            }
        } else {
            static_assert(sizeof(T) == 0,
                          "expect_elements_impl_recursive: Type is not indexable, arithmetic, or .value()-coercible");
        }
    }

    // Refactored EXPECT_ZERO
    template<typename T>
    inline void expect_zero_impl(const T& value, const char* expr, const char* file, int line, bool abort = false) {
        auto compare = [](auto v) {
            return v == 0;
        };
        auto fail_report = [](const char* exprA, const char*, const char* idxbuf, size_t offset, auto value,
                              const char* file, int line, int fails, int log_level) {
            mgba_printf_buf(log_level, "[FAIL] %s%s @ 0x%X == 0 (val=0x%X) @ %s:%d (fail %d)", exprA, idxbuf,
                            static_cast<unsigned int>(offset), static_cast<unsigned int>(value), file, line, fails);
        };
        expect_elements_impl_recursive(value, expr, nullptr, file, line, abort, {}, compare, fail_report);
    }

    // Refactored EXPECT_NZ (non-zero)
    template<typename T>
    inline void expect_nz_impl(const T& value, const char* expr, const char* file, int line, bool abort = false) {
        auto compare = [](auto v) {
            return v != 0;
        };
        auto fail_report = [](const char* exprA, const char*, const char* idxbuf, size_t offset, auto value,
                              const char* file, int line, int fails, int log_level) {
            mgba_printf_buf(log_level, "[FAIL] %s%s @ 0x%X != 0 (val=0x%X) @ %s:%d (fail %d)", exprA, idxbuf,
                            static_cast<unsigned int>(offset), static_cast<unsigned int>(value), file, line, fails);
        };
        expect_elements_impl_recursive(value, expr, nullptr, file, line, abort, {}, compare, fail_report);
    }
} // namespace test
