#pragma once


namespace gba {

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr auto fixed<Rep, FracBits, IntermediateRep>::convert_fixed(fixed_point auto rhs) noexcept {
        static constexpr auto shift_right = fixed_point_traits<decltype(rhs)>::fractional_digits -
                                            fixed_point_traits<fixed>::fractional_digits;

        const auto rhsData = __builtin_bit_cast(typename fixed_point_traits<decltype(rhs)>::underlying_type, rhs);

        return bits::shift_right(rhsData, shift_right);
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    template<fixed_point Other>
    constexpr bool fixed<Rep, FracBits, IntermediateRep>::trivially_converts_from() {
        if (fixed_point_traits<Other>::fractional_digits > FracBits) {
            return false;
        }

        using other_type = typename fixed_point_traits<Other>::underlying_type;
        if constexpr (std::is_unsigned_v<other_type>) {
            return std::numeric_limits<other_type>::digits <= std::numeric_limits<Rep>::digits;
        }

        return std::is_signed_v<Rep>;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>::fixed(std::integral auto num) noexcept
        : m_data{static_cast<Rep>(num * frac_mul)} {}

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    consteval fixed<Rep, FracBits, IntermediateRep>::fixed(std::floating_point auto flt) noexcept
        : m_data{static_cast<Rep>(flt * frac_mul)} {}

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>::fixed(fixed_point auto rhs) noexcept
        requires(trivially_converts_from<decltype(rhs)>())
        : m_data{static_cast<Rep>(convert_fixed(rhs))} {}

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    template<std::integral T>
    constexpr fixed<Rep, FracBits, IntermediateRep>::operator T() const noexcept {
        return static_cast<T>(m_data / frac_mul);
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    template<std::floating_point T>
    consteval fixed<Rep, FracBits, IntermediateRep>::operator T() const noexcept {
        return static_cast<T>(m_data) / frac_mul;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep> fixed<Rep, FracBits, IntermediateRep>::operator+() const noexcept {
        return __builtin_bit_cast(fixed, static_cast<Rep>(+m_data));
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep> fixed<Rep, FracBits, IntermediateRep>::operator-() const noexcept {
        return __builtin_bit_cast(fixed, static_cast<Rep>(-m_data));
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator+=(
        fixed rhs) noexcept {
        if constexpr (sizeof(Rep) < sizeof(int)) {
            using WordType = std::conditional_t<std::is_signed_v<Rep>, int, unsigned int>;
            const auto lhs_word = static_cast<WordType>(m_data);
            const auto rhs_word = static_cast<WordType>(rhs.m_data);
            m_data = static_cast<Rep>(lhs_word + rhs_word);
        } else {
            m_data += rhs.m_data;
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator-=(
        fixed rhs) noexcept {
        if constexpr (sizeof(Rep) < sizeof(int)) {
            using WordType = std::conditional_t<std::is_signed_v<Rep>, int, unsigned int>;
            const auto lhs_word = static_cast<WordType>(m_data);
            const auto rhs_word = static_cast<WordType>(rhs.m_data);
            m_data = static_cast<Rep>(lhs_word - rhs_word);
        } else {
            m_data -= rhs.m_data;
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator*=(
        fixed rhs) noexcept {
        // Fast path: if Rep fits in 16 bits, use 32-bit intermediate (ARM7TDMI native)
        if constexpr (sizeof(Rep) <= 2) {
            using FastInter = std::conditional_t<std::is_signed_v<Rep>, std::int32_t, std::uint32_t>;
            auto prod = static_cast<FastInter>(m_data) * static_cast<FastInter>(rhs.m_data);
            m_data = static_cast<Rep>(bits::shift_right(prod, static_cast<int>(FracBits)));
        } else {
            auto prod = static_cast<IntermediateRep>(m_data) * static_cast<IntermediateRep>(rhs.m_data);
            m_data = static_cast<Rep>(bits::shift_right(prod, static_cast<int>(FracBits)));
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator/=(
        fixed rhs) noexcept {
        // Fast path: if Rep fits in 16 bits, use 32-bit intermediate (ARM7TDMI native)
        if constexpr (sizeof(Rep) <= 2) {
            using FastInter = std::conditional_t<std::is_signed_v<Rep>, std::int32_t, std::uint32_t>;
            auto num = static_cast<FastInter>(m_data);
            auto den = static_cast<FastInter>(rhs.m_data);
            num = bits::shift_left(num, static_cast<int>(FracBits));
            m_data = static_cast<Rep>(num / den);
        } else {
            auto num = static_cast<IntermediateRep>(m_data);
            auto den = static_cast<IntermediateRep>(rhs.m_data);
            num = bits::shift_left(num, static_cast<int>(FracBits));
            m_data = static_cast<Rep>(num / den);
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator+=(
        std::integral auto rhs) noexcept {
        if constexpr (sizeof(Rep) < sizeof(int)) {
            using WordType = std::conditional_t<std::is_signed_v<Rep>, int, unsigned int>;
            const auto lhs_word = static_cast<WordType>(m_data);
            const auto rhs_scaled = static_cast<WordType>(rhs * frac_mul);
            m_data = static_cast<Rep>(lhs_word + rhs_scaled);
        } else {
            m_data += rhs * frac_mul;
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator-=(
        std::integral auto rhs) noexcept {
        if constexpr (sizeof(Rep) < sizeof(int)) {
            using WordType = std::conditional_t<std::is_signed_v<Rep>, int, unsigned int>;
            const auto lhs_word = static_cast<WordType>(m_data);
            const auto rhs_scaled = static_cast<WordType>(rhs * frac_mul);
            m_data = static_cast<Rep>(lhs_word - rhs_scaled);
        } else {
            m_data -= rhs * frac_mul;
        }
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator*=(
        std::integral auto rhs) noexcept {
        m_data *= rhs;
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator/=(
        std::integral auto rhs) noexcept {
        m_data /= rhs;
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator<<=(
        std::integral auto rhs) noexcept {
        m_data <<= rhs;
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator>>=(
        std::integral auto rhs) noexcept {
        m_data >>= rhs;
        return *this;
    }

    template<fixed_point T>
    constexpr std::strong_ordering operator<=>(T lhs, T rhs) noexcept {
        using rep = typename fixed_point_traits<T>::underlying_type;
        return __builtin_bit_cast(rep, lhs) <=> __builtin_bit_cast(rep, rhs);
    }

    template<fixed_point T>
    constexpr bool operator==(T lhs, T rhs) noexcept {
        using rep = typename fixed_point_traits<T>::underlying_type;
        return __builtin_bit_cast(rep, lhs) == __builtin_bit_cast(rep, rhs);
    }

    constexpr std::strong_ordering operator<=>(fixed_point auto lhs, std::integral auto rhs) noexcept {
        return lhs <=> static_cast<decltype(lhs)>(rhs);
    }

    constexpr bool operator==(fixed_point auto lhs, std::integral auto rhs) noexcept {
        return lhs == static_cast<decltype(lhs)>(rhs);
    }

    template<fixed_point T>
    constexpr T operator+(T lhs, T rhs) noexcept {
        lhs += rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator-(T lhs, T rhs) noexcept {
        lhs -= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator*(T lhs, T rhs) noexcept {
        lhs *= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator/(T lhs, T rhs) noexcept {
        lhs /= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator+(T lhs, std::integral auto rhs) noexcept {
        lhs += rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator+(std::integral auto lhs, T rhs) noexcept {
        rhs += lhs;
        return rhs;
    }

    template<fixed_point T>
    constexpr T operator-(T lhs, std::integral auto rhs) noexcept {
        lhs -= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator-(std::integral auto lhs, T rhs) noexcept {
        T tmp{lhs};
        tmp -= rhs;
        return tmp;
    }

    template<fixed_point T>
    constexpr T operator*(T lhs, std::integral auto rhs) noexcept {
        lhs *= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator*(std::integral auto lhs, T rhs) noexcept {
        rhs *= lhs;
        return rhs;
    }

    template<fixed_point T>
    constexpr T operator/(T lhs, std::integral auto rhs) noexcept {
        lhs /= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator/(std::integral auto lhs, T rhs) noexcept {
        T tmp{lhs};
        tmp /= rhs;
        return tmp;
    }

    template<fixed_point T>
    constexpr T operator<<(T lhs, std::integral auto rhs) noexcept {
        lhs <<= rhs;
        return lhs;
    }

    template<fixed_point T>
    constexpr T operator>>(T lhs, std::integral auto rhs) noexcept {
        lhs >>= rhs;
        return lhs;
    }

} // namespace gba
