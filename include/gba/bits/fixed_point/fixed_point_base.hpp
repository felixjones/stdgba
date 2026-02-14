#pragma once

#include <gba/bits/int_util.hpp>

#include <compare>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace gba {

    namespace literals {
        struct fixed_literal;
    } // namespace literals

    template<typename>
    struct is_fixed_point : std::false_type {};

    template<typename T>
    inline constexpr bool is_fixed_point_v = is_fixed_point<T>::value;

    template<typename T>
    concept fixed_point = is_fixed_point<T>::value;

    template<typename>
    struct fixed_point_traits {};

    /**
     * @brief Core fixed-point arithmetic type
     *
     * This is the base fixed-point type that provides all arithmetic operations.
     * It does NOT provide conversion wrappers - those are handled separately.
     * All operations use truncation; rounding is handled by conversion wrappers.
     *
     * @tparam Rep Underlying integer representation type
     * @tparam FracBits Number of fractional bits
     * @tparam IntermediateRep Type used for intermediate calculations (must be larger or equal to Rep)
     */
    template<std::integral Rep = int,
             unsigned int FracBits = std::numeric_limits<std::make_unsigned_t<Rep>>::digits / 2,
             std::integral IntermediateRep = typename bits::int_util<Rep>::promote_type>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    class fixed {
        static constexpr auto frac_mul = IntermediateRep{1} << FracBits;

        static constexpr auto convert_fixed(fixed_point auto rhs) noexcept;

        template<fixed_point Other>
        static constexpr bool trivially_converts_from();

    public:
        constexpr fixed() noexcept = default;

        constexpr explicit(false) fixed(std::integral auto num) noexcept;
        consteval explicit(false) fixed(std::floating_point auto flt) noexcept;
        consteval explicit(false) fixed(literals::fixed_literal lit) noexcept;

        constexpr explicit(false) fixed(fixed_point auto rhs) noexcept
            requires(trivially_converts_from<decltype(rhs)>());

        template<std::integral T>
        constexpr explicit operator T() const noexcept;

        template<std::floating_point T>
        consteval explicit operator T() const noexcept;

        constexpr fixed operator+() const noexcept;
        constexpr fixed operator-() const noexcept;

        constexpr fixed& operator+=(fixed rhs) noexcept;
        constexpr fixed& operator-=(fixed rhs) noexcept;
        constexpr fixed& operator*=(fixed rhs) noexcept;
        constexpr fixed& operator/=(fixed rhs) noexcept;

        constexpr fixed& operator+=(std::integral auto rhs) noexcept;
        constexpr fixed& operator-=(std::integral auto rhs) noexcept;
        constexpr fixed& operator*=(std::integral auto rhs) noexcept;
        constexpr fixed& operator/=(std::integral auto rhs) noexcept;

        constexpr fixed& operator<<=(std::integral auto rhs) noexcept;
        constexpr fixed& operator>>=(std::integral auto rhs) noexcept;

    private:
        Rep m_data;
    };

    template<std::integral T>
    fixed(T) -> fixed<T>;

    template<std::floating_point F>
    fixed(F) -> fixed<int>;

    template<fixed_point U>
    fixed(U) -> fixed<typename fixed_point_traits<U>::underlying_type, fixed_point_traits<U>::fractional_digits>;

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
    struct is_fixed_point<fixed<Rep, FracBits, IntermediateRep>> : std::true_type {};

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
    struct fixed_point_traits<fixed<Rep, FracBits, IntermediateRep>> {
        using rep = Rep;
        using intermediate_rep = IntermediateRep;
        static constexpr unsigned int frac_bits = FracBits;
        static constexpr auto fractional_digits = FracBits;
        using underlying_type = Rep;
    };

    constexpr auto bit_cast(fixed_point auto value) noexcept {
        using rep = typename fixed_point_traits<decltype(value)>::rep;
        return __builtin_bit_cast(rep, value);
    }

    template<std::integral Rep>
    constexpr Rep bit_cast(fixed_point auto value) noexcept {
        using inner = typename fixed_point_traits<decltype(value)>::rep;
        const auto raw = __builtin_bit_cast(inner, value);
        return static_cast<Rep>(raw);
    }

    template<fixed_point T>
    constexpr std::strong_ordering operator<=>(T lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr bool operator==(T lhs, T rhs) noexcept;

    constexpr std::strong_ordering operator<=>(fixed_point auto lhs, std::integral auto rhs) noexcept;
    constexpr bool operator==(fixed_point auto lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator+(T lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator-(T lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator*(T lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator/(T lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator+(T lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator+(std::integral auto lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator-(T lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator-(std::integral auto lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator*(T lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator*(std::integral auto lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator/(T lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator/(std::integral auto lhs, T rhs) noexcept;

    template<fixed_point T>
    constexpr T operator<<(T lhs, std::integral auto rhs) noexcept;

    template<fixed_point T>
    constexpr T operator>>(T lhs, std::integral auto rhs) noexcept;

} // namespace gba

namespace std {
    template<typename T>
        requires gba::is_fixed_point_v<T>
    struct is_arithmetic<T> : std::true_type {};
} // namespace std

#include <gba/bits/fixed_point/fixed_point_base.tpp>
