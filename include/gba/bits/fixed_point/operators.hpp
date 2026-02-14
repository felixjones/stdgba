#pragma once

#include <concepts>
#include <type_traits>

namespace gba {

    /**
     * @brief CRTP mixin providing arithmetic operators for conversion wrappers
     *
     * This mixin allows conversion wrapper types to inherit all the standard
     * arithmetic operators without having to reimplement them.
     *
     * @tparam Derived The derived conversion wrapper type
     */
    template<typename Derived>
    struct conversion_operators {
        constexpr auto operator+() const noexcept { return +move(static_cast<const Derived&>(*this)); }

        constexpr auto operator-() const noexcept { return -move(static_cast<const Derived&>(*this)); }

        constexpr auto operator<=>(const Derived& rhs) const noexcept {
            return move(static_cast<const Derived&>(*this)) <=> move(rhs);
        }

        constexpr bool operator==(const Derived& rhs) const noexcept {
            return move(static_cast<const Derived&>(*this)) == move(rhs);
        }

        constexpr auto operator<=>(std::integral auto rhs) const noexcept {
            return move(static_cast<const Derived&>(*this)) <=> rhs;
        }

        constexpr bool operator==(std::integral auto rhs) const noexcept {
            return move(static_cast<const Derived&>(*this)) == rhs;
        }
    };

    /**
     * @brief Binary operators for conversion wrappers
     *
     * These operators are defined outside the class to provide symmetric
     * operations between wrappers, fixed_point types, and integers.
     */

    template<typename T>
    constexpr decltype(auto) unwrap(const T& value) noexcept {
        if constexpr (conversion_wrapper<T>) {
            return move(value);
        } else {
            return value;
        }
    }

    template<conversion_wrapper L, conversion_wrapper R>
        requires std::is_same_v<L, R>
    constexpr auto operator+(const L& lhs, const R& rhs) noexcept {
        return L::convert_binary_result(unwrap(lhs), unwrap(rhs), [](const auto& a, const auto& b) { return a + b; });
    }

    template<conversion_wrapper L, conversion_wrapper R>
        requires std::is_same_v<L, R>
    constexpr auto operator-(const L& lhs, const R& rhs) noexcept {
        return L::convert_binary_result(unwrap(lhs), unwrap(rhs), [](const auto& a, const auto& b) { return a - b; });
    }

    template<conversion_wrapper L, conversion_wrapper R>
        requires std::is_same_v<L, R>
    constexpr auto operator*(const L& lhs, const R& rhs) noexcept {
        return L::convert_binary_result(unwrap(lhs), unwrap(rhs), [](const auto& a, const auto& b) { return a * b; });
    }

    template<conversion_wrapper L, conversion_wrapper R>
        requires std::is_same_v<L, R>
    constexpr auto operator/(const L& lhs, const R& rhs) noexcept {
        return L::convert_binary_result(unwrap(lhs), unwrap(rhs), [](const auto& a, const auto& b) { return a / b; });
    }

    template<conversion_wrapper W>
    constexpr auto operator+(const W& lhs, std::integral auto rhs) noexcept {
        return unwrap(lhs) + rhs;
    }

    template<conversion_wrapper W>
    constexpr auto operator+(std::integral auto lhs, const W& rhs) noexcept {
        return lhs + unwrap(rhs);
    }

    template<conversion_wrapper W>
    constexpr auto operator-(const W& lhs, std::integral auto rhs) noexcept {
        return unwrap(lhs) - rhs;
    }

    template<conversion_wrapper W>
    constexpr auto operator-(std::integral auto lhs, const W& rhs) noexcept {
        return lhs - unwrap(rhs);
    }

    template<conversion_wrapper W>
    constexpr auto operator*(const W& lhs, std::integral auto rhs) noexcept {
        return unwrap(lhs) * rhs;
    }

    template<conversion_wrapper W>
    constexpr auto operator*(std::integral auto lhs, const W& rhs) noexcept {
        return lhs * unwrap(rhs);
    }

    template<conversion_wrapper W>
    constexpr auto operator/(const W& lhs, std::integral auto rhs) noexcept {
        return unwrap(lhs) / rhs;
    }

    template<conversion_wrapper W>
    constexpr auto operator/(std::integral auto lhs, const W& rhs) noexcept {
        return lhs / unwrap(rhs);
    }

    template<conversion_wrapper W, fixed_point F>
        requires(!conversion_wrapper<F>)
    constexpr auto operator+(const W& lhs, const F& rhs) noexcept {
        return W::convert_binary_result(unwrap(lhs), rhs, [](const auto& a, const auto& b) { return a + b; });
    }

    template<fixed_point F, conversion_wrapper W>
        requires(!conversion_wrapper<F>)
    constexpr auto operator+(const F& lhs, const W& rhs) noexcept {
        return W::convert_binary_result(lhs, unwrap(rhs), [](const auto& a, const auto& b) { return a + b; });
    }

    template<conversion_wrapper W, fixed_point F>
        requires(!conversion_wrapper<F>)
    constexpr auto operator-(const W& lhs, const F& rhs) noexcept {
        return W::convert_binary_result(unwrap(lhs), rhs, [](const auto& a, const auto& b) { return a - b; });
    }

    template<fixed_point F, conversion_wrapper W>
        requires(!conversion_wrapper<F>)
    constexpr auto operator-(const F& lhs, const W& rhs) noexcept {
        return W::convert_binary_result(lhs, unwrap(rhs), [](const auto& a, const auto& b) { return a - b; });
    }

    template<conversion_wrapper W, fixed_point F>
        requires(!conversion_wrapper<F>)
    constexpr auto operator*(const W& lhs, const F& rhs) noexcept {
        return W::convert_binary_result(unwrap(lhs), rhs, [](const auto& a, const auto& b) { return a * b; });
    }

    template<fixed_point F, conversion_wrapper W>
        requires(!conversion_wrapper<F>)
    constexpr auto operator*(const F& lhs, const W& rhs) noexcept {
        return W::convert_binary_result(lhs, unwrap(rhs), [](const auto& a, const auto& b) { return a * b; });
    }

    template<conversion_wrapper W, fixed_point F>
        requires(!conversion_wrapper<F>)
    constexpr auto operator/(const W& lhs, const F& rhs) noexcept {
        return W::convert_binary_result(unwrap(lhs), rhs, [](const auto& a, const auto& b) { return a / b; });
    }

    template<fixed_point F, conversion_wrapper W>
        requires(!conversion_wrapper<F>)
    constexpr auto operator/(const F& lhs, const W& rhs) noexcept {
        return W::convert_binary_result(lhs, unwrap(rhs), [](const auto& a, const auto& b) { return a / b; });
    }

} // namespace gba
