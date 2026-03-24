#pragma once


namespace gba {

    namespace bits {

        template<std::integral T>
        constexpr bool is_positive_power_of_two(T value) noexcept {
            if constexpr (std::is_signed_v<T>) {
                if (value <= 0) {
                    return false;
                }
            } else {
                if (value == 0) {
                    return false;
                }
            }

            using unsigned_t = std::make_unsigned_t<T>;
            const auto u = static_cast<unsigned_t>(value);
            return (u & (u - 1)) == 0;
        }

        template<std::unsigned_integral T>
        constexpr int power_of_two_shift(T value) noexcept {
            int shift = 0;
            while ((value & T{1}) == 0) {
                value >>= 1;
                ++shift;
            }
            return shift;
        }

        template<std::integral T>
        constexpr bool is_negative_power_of_two(T value) noexcept {
            if constexpr (!std::is_signed_v<T>) {
                return false;
            } else {
                if (value >= 0) {
                    return false;
                }

                using unsigned_t = std::make_unsigned_t<T>;
                const auto magnitude = static_cast<unsigned_t>(0) - static_cast<unsigned_t>(value);
                return (magnitude & (magnitude - 1)) == 0;
            }
        }

        template<std::integral T>
        constexpr std::make_unsigned_t<T> unsigned_magnitude(T value) noexcept {
            using unsigned_t = std::make_unsigned_t<T>;
            if constexpr (std::is_signed_v<T>) {
                if (value < 0) {
                    return static_cast<unsigned_t>(0) - static_cast<unsigned_t>(value);
                }
            }
            return static_cast<unsigned_t>(value);
        }

        template<std::integral T>
        constexpr T div_by_power_of_two_toward_zero(T value, int shift) noexcept {
            auto quotient = ::gba::bits::shift_right(value, shift);
            if constexpr (std::is_signed_v<T>) {
                if (value < 0) {
                    using unsigned_t = std::make_unsigned_t<T>;
                    const auto rem_mask = static_cast<unsigned_t>((unsigned_t{1} << shift) - 1);
                    const auto remainder = static_cast<unsigned_t>(value) & rem_mask;
                    if (remainder != 0) {
                        ++quotient;
                    }
                }
            }
            return static_cast<T>(quotient);
        }

        template<std::integral Rep, unsigned int FracBits>
        struct fixed_power_of_two_factor {
            bool valid;
            bool negative;
            int shift;
        };

        template<std::integral Rep, unsigned int FracBits>
        constexpr fixed_power_of_two_factor<Rep, FracBits> classify_fixed_power_of_two_factor(Rep raw) noexcept {
            using unsigned_rep = std::make_unsigned_t<Rep>;
            constexpr auto frac_scale = static_cast<unsigned_rep>(unsigned_rep{1} << FracBits);

            bool negative = false;
            unsigned_rep magnitude = 0;

            if (is_positive_power_of_two(raw)) {
                magnitude = unsigned_magnitude(raw);
            } else if constexpr (std::is_signed_v<Rep>) {
                if (is_negative_power_of_two(raw)) {
                    magnitude = unsigned_magnitude(raw);
                    negative = true;
                } else {
                    return {false, false, 0};
                }
            } else {
                return {false, false, 0};
            }

            if (magnitude < frac_scale || (magnitude % frac_scale) != 0) {
                return {false, false, 0};
            }

            const auto factor = static_cast<unsigned_rep>(magnitude / frac_scale);
            if (factor == 0 || (factor & (factor - 1)) != 0) {
                return {false, false, 0};
            }

            return {true, negative, power_of_two_shift(factor)};
        }

    } // namespace bits

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
#if defined(__clang__) || defined(__GNUC__)
        if (__builtin_constant_p(rhs.m_data)) {
            using unsigned_rep = std::make_unsigned_t<Rep>;

            const auto factor = bits::classify_fixed_power_of_two_factor<Rep, FracBits>(rhs.m_data);
            if (factor.valid && factor.shift < std::numeric_limits<unsigned_rep>::digits) {
                const auto shifted = bits::shift_left(m_data, factor.shift);
                if constexpr (std::is_signed_v<Rep>) {
                    m_data = factor.negative ? static_cast<Rep>(-shifted) : static_cast<Rep>(shifted);
                } else {
                    m_data = static_cast<Rep>(shifted);
                }
                return *this;
            }
        }
#endif

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
#if defined(__clang__) || defined(__GNUC__)
        if (__builtin_constant_p(rhs.m_data)) {
            using unsigned_rep = std::make_unsigned_t<Rep>;

            const auto factor = bits::classify_fixed_power_of_two_factor<Rep, FracBits>(rhs.m_data);
            if (factor.valid && factor.shift < std::numeric_limits<unsigned_rep>::digits) {
                if constexpr (std::is_signed_v<Rep>) {
                    const auto quotient = bits::div_by_power_of_two_toward_zero(m_data, factor.shift);
                    m_data = factor.negative ? static_cast<Rep>(-quotient) : static_cast<Rep>(quotient);
                } else {
                    m_data = bits::shift_right(m_data, factor.shift);
                }
                return *this;
            }
        }
#endif

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
#if defined(__clang__) || defined(__GNUC__)
        if (__builtin_constant_p(rhs) && bits::is_positive_power_of_two(rhs)) {
            const auto magnitude = bits::unsigned_magnitude(rhs);
            const auto shift = bits::power_of_two_shift(magnitude);
            if (shift < std::numeric_limits<std::make_unsigned_t<Rep>>::digits) {
                m_data = bits::shift_left(m_data, shift);
                return *this;
            }
        }

        if constexpr (std::is_signed_v<Rep>) {
            if (__builtin_constant_p(rhs) && bits::is_negative_power_of_two(rhs)) {
                const auto magnitude = bits::unsigned_magnitude(rhs);
                const auto shift = bits::power_of_two_shift(magnitude);
                if (shift < std::numeric_limits<std::make_unsigned_t<Rep>>::digits) {
                    m_data = static_cast<Rep>(-bits::shift_left(m_data, shift));
                    return *this;
                }
            }
        }
#endif

        m_data *= rhs;
        return *this;
    }

    template<std::integral Rep, unsigned int FracBits, std::integral IntermediateRep>
        requires(FracBits > 0 && FracBits <= std::numeric_limits<std::make_unsigned_t<Rep>>::digits - 1 &&
                 sizeof(IntermediateRep) >= sizeof(Rep) && std::is_signed_v<IntermediateRep> == std::is_signed_v<Rep>)
    constexpr fixed<Rep, FracBits, IntermediateRep>& fixed<Rep, FracBits, IntermediateRep>::operator/=(
        std::integral auto rhs) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        if (__builtin_constant_p(rhs) && bits::is_positive_power_of_two(rhs)) {
            const auto magnitude = bits::unsigned_magnitude(rhs);
            const auto shift = bits::power_of_two_shift(magnitude);
            if (shift < std::numeric_limits<std::make_unsigned_t<Rep>>::digits) {
                if constexpr (std::is_signed_v<Rep>) {
                    m_data = bits::div_by_power_of_two_toward_zero(m_data, shift);
                } else {
                    m_data = bits::shift_right(m_data, shift);
                }
                return *this;
            }
        }

        if constexpr (std::is_signed_v<Rep>) {
            if (__builtin_constant_p(rhs) && bits::is_negative_power_of_two(rhs)) {
                const auto magnitude = bits::unsigned_magnitude(rhs);
                const auto shift = bits::power_of_two_shift(magnitude);
                if (shift < std::numeric_limits<std::make_unsigned_t<Rep>>::digits) {
                    const auto positive_divisor_quotient = bits::div_by_power_of_two_toward_zero(m_data, shift);
                    m_data = static_cast<Rep>(-positive_divisor_quotient);
                    return *this;
                }
            }
        }
#endif

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
