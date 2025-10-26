#pragma once

#include <gba/bits/fixed_point/fixed_point_base.hpp>

namespace gba {

// Forward declare for constructor
namespace literals {
    struct fixed_literal;
}

} // namespace gba

namespace gba::literals {

/**
 * @brief Compile-time fixed-point literal type
 *
 * This type can only exist at compile-time and stores a floating-point value
 * that will be converted to the appropriate fixed-point representation when
 * assigned to a fixed<> type.
 *
 * Usage:
 *   using namespace gba::literals;
 *   auto a = 1_fx + 2.3_fx;
 *   fixed<int, 8> b = a;  // Converts at compile-time
 *
 * Or with selective import:
 *   using gba::literals::operator""_fx;
 *   auto a = 1_fx + 2.3_fx;
 */
struct fixed_literal {
    double value;

    consteval fixed_literal(double v) noexcept : value(v) {}
    consteval fixed_literal(long double v) noexcept : value(static_cast<double>(v)) {}
    consteval fixed_literal(float v) noexcept : value(static_cast<double>(v)) {}
    consteval fixed_literal(long long v) noexcept : value(static_cast<double>(v)) {}
    consteval fixed_literal(unsigned long long v) noexcept : value(static_cast<double>(v)) {}
    consteval fixed_literal(int v) noexcept : value(static_cast<double>(v)) {}
    consteval fixed_literal(unsigned int v) noexcept : value(static_cast<double>(v)) {}

    // Arithmetic operators
    consteval fixed_literal operator+(fixed_literal rhs) const noexcept {
        return fixed_literal(value + rhs.value);
    }

    consteval fixed_literal operator-(fixed_literal rhs) const noexcept {
        return fixed_literal(value - rhs.value);
    }

    consteval fixed_literal operator*(fixed_literal rhs) const noexcept {
        return fixed_literal(value * rhs.value);
    }

    consteval fixed_literal operator/(fixed_literal rhs) const noexcept {
        return fixed_literal(value / rhs.value);
    }

    consteval fixed_literal operator-() const noexcept {
        return fixed_literal(-value);
    }

    consteval fixed_literal operator+() const noexcept {
        return *this;
    }

    // Allow mixing with numeric types
    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator+(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value + static_cast<double>(rhs));
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator+(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) + rhs.value);
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator+(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value + static_cast<double>(rhs));
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator+(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) + rhs.value);
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator-(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value - static_cast<double>(rhs));
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator-(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) - rhs.value);
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator-(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value - static_cast<double>(rhs));
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator-(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) - rhs.value);
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator*(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value * static_cast<double>(rhs));
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator*(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) * rhs.value);
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator*(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value * static_cast<double>(rhs));
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator*(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) * rhs.value);
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator/(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value / static_cast<double>(rhs));
    }

    template<typename T> requires std::integral<T>
    consteval friend fixed_literal operator/(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) / rhs.value);
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator/(fixed_literal lhs, T rhs) noexcept {
        return fixed_literal(lhs.value / static_cast<double>(rhs));
    }

    template<typename T> requires std::floating_point<T>
    consteval friend fixed_literal operator/(T lhs, fixed_literal rhs) noexcept {
        return fixed_literal(static_cast<double>(lhs) / rhs.value);
    }
};

// User-defined literal for integers
consteval fixed_literal operator""_fx(unsigned long long value) noexcept {
    return fixed_literal(static_cast<double>(value));
}

// User-defined literal for floating-point
consteval fixed_literal operator""_fx(long double value) noexcept {
    return fixed_literal(static_cast<double>(value));
}

} // namespace gba::literals

namespace gba {

// Implementation of fixed<> constructor for fixed_literal
// This must come after fixed_literal is fully defined
template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
requires (
    FracBits > 0 &&
    FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
    sizeof(IntermediateRep) >= sizeof(Rep) &&
    std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>
)
consteval fixed<Rep, FracBits, IntermediateRep>::fixed(literals::fixed_literal lit) noexcept
    : m_data{static_cast<Rep>(lit.value * frac_mul)} {}

} // namespace gba
