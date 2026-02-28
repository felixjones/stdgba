#pragma once

#include <gba/bits/fixed_point/fixed_point_base.hpp>

#include <type_traits>

namespace gba {

    /// @brief Trait to identify conversion wrapper types
    template<typename>
    struct is_conversion_wrapper : std::false_type {};

    template<typename T>
    constexpr decltype(auto) move(const T& wrapper) noexcept;

    /// @brief Concept to identify conversion wrapper types
    ///
    /// Must have wrapped_type that is a fixed_point
    template<typename T>
    concept conversion_wrapper = requires {
        typename T::wrapped_type;
        requires fixed_point<typename T::wrapped_type>;
    };

} // namespace gba

#include <gba/bits/fixed_point/operators.hpp>

namespace gba {

    /// @brief Base template for conversion wrappers
    ///
    /// Conversion wrappers wrap a fixed-point reference and define how operations
    /// between different fixed-point types should be converted. Different conversion
    /// policies can be implemented by inheriting from this class.
    ///
    /// @tparam T The wrapped fixed-point type
    /// @tparam Derived The derived conversion wrapper type (CRTP)
    template<fixed_point T, typename Derived>
    struct conversion_wrapper_base : conversion_operators<Derived> {
        using wrapped_type = T;

        constexpr conversion_wrapper_base(const T& value) noexcept : m_value(value) {}

        constexpr operator T() const noexcept { return m_value; }

        template<fixed_point U>
        constexpr operator U() const noexcept {
            return convert_fixed<U>(m_value);
        }

        template<typename U>
        friend constexpr decltype(auto) move(const U& wrapper) noexcept;

    protected:
        const T& m_value;
    };

    /// @brief Helper to convert between fixed-point types via bit manipulation
    ///
    /// This is used by converters since we don't allow static_cast between
    /// incompatible fixed-point types.
    template<fixed_point To, fixed_point From>
    constexpr To convert_fixed(const From& from) noexcept {
        using from_traits = fixed_point_traits<From>;
        using to_traits = fixed_point_traits<To>;

        const auto from_bits = __builtin_bit_cast(typename from_traits::rep, from);

        constexpr int shift = static_cast<int>(from_traits::frac_bits) - static_cast<int>(to_traits::frac_bits);
        const auto adjusted_bits = shift > 0 ? (from_bits >> shift) : (from_bits << -shift);

        return __builtin_bit_cast(To, static_cast<typename to_traits::rep>(adjusted_bits));
    }

    /// @brief Helper to "move" the value out of a conversion wrapper
    ///
    /// This extracts the wrapped fixed-point value without exposing a .get() method.
    /// Works like std::move conceptually - extracting the contained value.
    ///
    /// Note: This function accesses the m_value member directly, so it's a friend
    /// of conversion_wrapper_base.
    template<typename T>
    constexpr decltype(auto) move(const T& wrapper) noexcept {
        return wrapper.m_value;
    }

} // namespace gba
