/// @file bits/constexpr_assert.hpp
/// @brief Internal helpers for failing in constant-evaluated code without exceptions.
#pragma once

#include <source_location>

namespace gba::bits {

#if defined(__cpp_constexpr) && __cpp_constexpr >= 202306L && defined(__cpp_exceptions)
    inline constexpr bool supports_constexpr_throw = true;
#else
    inline constexpr bool supports_constexpr_throw = false;
#endif

    extern "C" [[noreturn]] void __assert_func(const char* file, int line, const char* func, const char* expr);

    [[noreturn]] consteval inline void constexpr_fail(const char* message) {
#if defined(__cpp_constexpr) && __cpp_constexpr >= 202306L && defined(__cpp_exceptions)
        throw message;
#else
        (void)message;
        __builtin_trap();
#endif
    }

    [[noreturn, gnu::cold]] inline void runtime_fail(
        const char* message, const std::source_location loc = std::source_location::current()) noexcept {
#ifdef NDEBUG
        (void)message;
        (void)loc;
        __builtin_trap();
#else
        __assert_func(loc.file_name(), static_cast<int>(loc.line()), loc.function_name(),
                      (message && message[0]) ? message : "stdgba precondition failed");
#endif
    }

    [[gnu::always_inline]] constexpr inline void constexpr_assert(
        bool violated, const char* message, const std::source_location loc = std::source_location::current()) {
        if (violated) {
            if consteval {
                constexpr_fail(message);
            } else {
                runtime_fail(message, loc);
            }
        }
    }

} // namespace gba::bits
