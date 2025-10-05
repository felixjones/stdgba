// ReSharper disable CppRedundantInlineSpecifier
#pragma once

#include "registral_ops.hpp"

#include <bit>
#include <cstdint>

namespace gba::bits {

template<typename ValueType> requires (sizeof(ValueType) == 8)
[[gnu::always_inline]]
inline ValueType value_by_copy8(std::uintptr_t address) {
    // ReSharper disable once CppDeprecatedRegisterStorageClassSpecifier
    register std::uint64_t destination asm ("r0");
    asm volatile ("ldm %[address]!, {%[destination]-r1}" : [destination]"=l"(destination), [address]"+l"(address));
    return __builtin_bit_cast(ValueType, destination);
}

template<typename Self>
[[gnu::always_inline]]
inline typename Self::value_type read_ops::value(this const Self& self) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        std::uint8_t destination;
        asm volatile ("ldrb %[destination], [%[address]]" : [destination]"=l"(destination) : [address]"l"(self.m_address));
        return __builtin_bit_cast(value_type, destination);
    } else if constexpr (sizeof(value_type) == 2) {
        std::uint16_t destination;
        asm volatile ("ldrh %[destination], [%[address]]" : [destination]"=l"(destination) : [address]"l"(self.m_address));
        return __builtin_bit_cast(value_type, destination);
    } else if constexpr (sizeof(value_type) == 4) {
        std::uint32_t destination;
        asm volatile ("ldr %[destination], [%[address]]" : [destination]"=l"(destination) : [address]"l"(self.m_address));
        return __builtin_bit_cast(value_type, destination);
    } else if constexpr (sizeof(value_type) == 8) {
        return value_by_copy8<value_type>(self.m_address);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t destination[sizeof(value_type) / 4];
        std::uint32_t temp;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp], [%[address], #i]\n"
            "str %[temp], [%[destination], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp]"=&l"(temp) : [address]"l"(self.m_address), [destination]"l"(destination), "i"(sizeof(value_type)) : "memory"
        );
        return __builtin_bit_cast(value_type, destination);
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

[[gnu::always_inline]]
inline void write_by_copy6(std::uintptr_t address, auto value) requires (sizeof(decltype(value)) == 6) {
    auto valuePtr = &value;
    asm volatile (
        "ldm %[value]!, {r1-r2}\n"
        "str r1, [%[address]]\n"
        "strh r2, [%[address], #4]"
        : [value]"+l"(valuePtr) : [address]"l"(address) : "memory", "r1", "r2"
    );
}

[[gnu::always_inline]]
inline void write_by_copy8(std::uintptr_t address, auto value) requires (sizeof(decltype(value)) == 8) {
    // ReSharper disable once CppDeprecatedRegisterStorageClassSpecifier
    register const auto valueTemp asm ("r1") = __builtin_bit_cast(std::uint64_t, value);
    asm volatile ("stm %[address]!, {%[value]-r2}" : [address]"+l"(address) : [value]"l"(valueTemp) : "memory");
}

[[gnu::always_inline]]
inline void write_by_copy12(std::uintptr_t address, auto value) requires (sizeof(decltype(value)) == 12) {
    // ReSharper disable once CppDeprecatedRegisterStorageClassSpecifier
    register const auto valueTemp asm ("r1") = &value;
    asm volatile (
        "ldm %[value_ptr], {%[value_ptr]-r3}\n"
        "stm %[address]!, {%[value_ptr]-r3}"
    : [address]"+l"(address) : [value_ptr]"l"(valueTemp) : "memory", "r2", "r3");
}

template<typename Self>
[[gnu::always_inline]]
inline void write_ops::write(this const Self& lhs, typename Self::value_type&& rhs) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        const auto value = __builtin_bit_cast(std::uint8_t, rhs);
        asm volatile ("strb %[value], [%[address]]" :: [address]"l"(lhs.m_address), [value]"l"(value) : "memory");
    } else if constexpr (sizeof(value_type) == 2) {
        const auto value = __builtin_bit_cast(std::uint16_t, rhs);
        asm volatile ("strh %[value], [%[address]]" :: [address]"l"(lhs.m_address), [value]"l"(value) : "memory");
    } else if constexpr (sizeof(value_type) == 4) {
        const auto value = __builtin_bit_cast(std::uint32_t, rhs);
        asm volatile ("str %[value], [%[address]]" :: [address]"l"(lhs.m_address), [value]"l"(value) : "memory");
    } else if constexpr (sizeof(value_type) == 6) {
        write_by_copy6(lhs.m_address, rhs);
    } else if constexpr (sizeof(value_type) == 8) {
        write_by_copy8(lhs.m_address, rhs);
    } else if constexpr (sizeof(value_type) == 12) {
        write_by_copy12(lhs.m_address, rhs);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t temp;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp], [%[value], #i]\n"
            "str %[temp], [%[address], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp]"=&l"(temp) : [address]"l"(lhs.m_address), [value]"l"(&rhs), "i"(sizeof(value_type)) : "memory"
        );
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

template<typename Self>
[[gnu::always_inline]]
inline void write_ops::write(this const Self& lhs, const typename Self::value_type& rhs) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        asm volatile ("strb %[value], [%[address]]" :: [value]"l"(rhs), [address]"l"(lhs.m_address) : "memory");
    } else if constexpr (sizeof(value_type) == 2) {
        asm volatile ("strh %[value], [%[address]]" :: [value]"l"(rhs), [address]"l"(lhs.m_address) : "memory");
    } else if constexpr (sizeof(value_type) == 4) {
        asm volatile ("str %[value], [%[address]]" :: [value]"l"(rhs), [address]"l"(lhs.m_address) : "memory");
    } else if constexpr (sizeof(value_type) == 8) {
        write_by_copy8(lhs.m_address, rhs);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t temp;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp], [%[value], #i]\n"
            "str %[temp], [%[address], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp]"=&l"(temp) : [address]"l"(lhs.m_address), [value]"l"(&rhs), "i"(sizeof(value_type)) : "memory"
        );
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

template<typename Self>
[[gnu::always_inline]]
inline void write_ops::write_integer(this const Self& lhs, std::integral auto value) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value) > sizeof(value_type)) {
        if constexpr (sizeof(value_type) == 1) {
            asm volatile ("strb %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else if constexpr (sizeof(value_type) == 2) {
            asm volatile ("strh %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else if constexpr (sizeof(value_type) == 4) {
            asm volatile ("str %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else {
            static_assert(false, "Impossible branch");
        }
    } else {
        if constexpr (sizeof(value) == 1) {
            asm volatile ("strb %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else if constexpr (sizeof(value) == 2) {
            asm volatile ("strh %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else if constexpr (sizeof(value) == 4) {
            asm volatile ("str %[value], [%[address]]" :: [value]"l"(value), [address]"l"(lhs.m_address) : "memory");
        } else if constexpr (sizeof(value) == 8) {
            write_by_copy8(lhs.m_address, value);
        } else {
            static_assert(false, "Impossible branch");
        }
    }
}

[[gnu::always_inline]]
inline void copy_by_copy8(std::uintptr_t dest, std::uintptr_t source) {
    // ReSharper disable once CppDeprecatedRegisterStorageClassSpecifier
    register auto temp asm ("r1") = static_cast<std::uint64_t>(source);
    asm volatile (
        "ldm %[rhs], {%[rhs]-r2}\n"
        "stm %[lhs]!, {%[rhs]-r2}"
        : [lhs]"+l"(dest), [rhs]"+l"(temp) :: "memory"
    );
}

template<typename Self, typename Other>
[[gnu::always_inline]]
inline void write_ops::copy(this const Self& lhs, const Other& rhs) noexcept requires std::derived_from<Other, read_ops> && std::same_as<typename Self::value_type, typename Other::value_type> {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        std::uint8_t temp;
        asm volatile (
            "ldrb %[temp], [%[other_address]]\n"
            "strb %[temp], [%[address]]"
            : [temp]"=&l"(temp) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 2) {
        std::uint16_t temp;
        asm volatile (
            "ldrh %[temp], [%[other_address]]\n"
            "strh %[temp], [%[address]]"
            : [temp]"=&l"(temp) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 4) {
        std::uint32_t temp;
        asm volatile (
            "ldr %[temp], [%[other_address]]\n"
            "str %[temp], [%[address]]"
            : [temp]"=&l"(temp) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 8) {
        copy_by_copy8(lhs.m_address, rhs.m_address);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t temp;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp], [%[src], #i]\n"
            "str %[temp], [%[dest], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp]"=&l"(temp) : [dest]"l"(lhs.m_address), [src]"l"(rhs.m_address), "i"(sizeof(value_type)) : "memory"
        );
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

[[gnu::always_inline]]
inline void swap_by_copy8(std::uintptr_t a, std::uintptr_t b) {
    // ReSharper disable CppDeprecatedRegisterStorageClassSpecifier
    register auto tempA asm ("r0") = static_cast<std::uint64_t>(a);
    register auto tempB asm ("r2") = static_cast<std::uint64_t>(b);
    // ReSharper restore CppDeprecatedRegisterStorageClassSpecifier
    asm volatile (
        "ldm %[temp_a], {%[temp_a]-r1}\n"
        "ldm %[temp_b], {%[temp_b]-r3}\n"
        "stm %[b]!, {%[temp_a]-r1}\n"
        "stm %[a]!, {%[temp_b]-r3}"
        : [temp_a]"+l"(tempA), [temp_b]"+l"(tempB) : [a]"l"(a), [b]"l"(b) : "memory"
    );
}

template<typename Self>
[[gnu::always_inline]]
inline void read_write_ops::swap(this const Self& lhs, const Self& rhs) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        std::uint8_t temp, temp2;
        asm volatile (
            "ldrb %[temp], [%[address]]\n"
            "ldrb %[temp2], [%[other_address]]\n"
            "strb %[temp], [%[other_address]]\n"
            "strb %[temp2], [%[address]]"
            : [temp]"=&l"(temp), [temp2]"=&l"(temp2) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 2) {
        std::uint16_t temp, temp2;
        asm volatile (
            "ldrh %[temp], [%[address]]\n"
            "ldrh %[temp2], [%[other_address]]\n"
            "strh %[temp], [%[other_address]]\n"
            "strh %[temp2], [%[address]]"
            : [temp]"=&l"(temp), [temp2]"=&l"(temp2) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 4) {
        std::uint32_t temp, temp2;
        asm volatile (
            "ldr %[temp], [%[address]]\n"
            "ldr %[temp2], [%[other_address]]\n"
            "str %[temp], [%[other_address]]\n"
            "str %[temp2], [%[address]]"
            : [temp]"=&l"(temp), [temp2]"=&l"(temp2) : [address]"l"(lhs.m_address), [other_address]"l"(rhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 8) {
        swap_by_copy8(lhs.m_address, rhs.m_address);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t tempA, tempB;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp_a], [%[a], #i]\n"
            "ldr %[temp_b], [%[b], #i]\n"
            "str %[temp_b], [%[a], #i]\n"
            "str %[temp_a], [%[b], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp_a]"=&l"(tempA), [temp_b]"=&l"(tempB) : [a]"l"(lhs.m_address), [b]"l"(rhs.m_address), "i"(sizeof(value_type)) : "memory"
        );
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

template<typename ValueType>
[[gnu::always_inline]]
inline void swap_by_copy8(std::uintptr_t a, ValueType b) {
    // ReSharper disable CppDeprecatedRegisterStorageClassSpecifier
    register auto tempA asm ("r0") = static_cast<std::uint64_t>(a);
    register auto tempB asm ("r2") = __builtin_bit_cast(std::uint64_t, b);
    // ReSharper restore CppDeprecatedRegisterStorageClassSpecifier
    asm volatile (
        "ldm %[temp_a], {%[temp_a]-r1}\n"
        "ldm %[temp_b], {%[temp_b]-r3}\n"
        "stm %[b]!, {%[temp_a]-r1}\n"
        "stm %[a]!, {%[temp_b]-r3}"
        : [temp_a]"+l"(tempA), [temp_b]"+l"(tempB) : [a]"l"(a), [b]"l"(b) : "memory"
    );
}

template<typename Self>
[[gnu::always_inline]]
inline void read_write_ops::swap(this const Self& lhs, typename Self::value_type& rhs) noexcept {
    using value_type = typename Self::value_type;

    if constexpr (sizeof(value_type) == 1) {
        std::uint8_t temp;
        asm volatile (
            "ldrb %[temp], [%[address]]\n"
            "strb %[value], [%[address]]\n"
            "mov %[value], %[temp]"
            : [temp]"=&l"(temp), [value]"+&l"(rhs) : [address]"l"(lhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 2) {
        std::uint16_t temp;
        asm volatile (
            "ldrh %[temp], [%[address]]\n"
            "strh %[value], [%[address]]\n"
            "mov %[value], %[temp]"
            : [temp]"=&l"(temp), [value]"+&l"(rhs) : [address]"l"(lhs.m_address) : "memory"
        );
    } else if constexpr (sizeof(value_type) == 4) {
        std::uint32_t temp;
        asm volatile (
            "ldr %[temp], [%[address]]\n"
            "str %[value], [%[address]]\n"
            "mov %[value], %[temp]"
            : [temp]"=&l"(temp), [value]"+&l"(rhs) : [address]"l"(lhs.m_address) : "memory"
            );
    } else if constexpr (sizeof(value_type) == 8) {
        swap_by_copy8(lhs.m_address, rhs);
    } else if constexpr (std::has_single_bit(sizeof(value_type))) {
        std::uint32_t tempA, tempB;
        asm volatile (
            ".set i, 0\n"
            ".rept %c3 / 4\n"
            "ldr %[temp_a], [%[a], #i]\n"
            "ldr %[temp_b], [%[b], #i]\n"
            "str %[temp_b], [%[a], #i]\n"
            "str %[temp_a], [%[b], #i]\n"
            ".set i, i + 4\n"
            ".endr"
            : [temp_a]"=&l"(tempA), [temp_b]"=&l"(tempB) : [a]"l"(lhs.m_address), [b]"l"(&rhs), "i"(sizeof(value_type)) : "memory"
        );
    } else {
        static_assert(false, "Unimplemented"); // TODO
    }
}

}
