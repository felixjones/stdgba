#pragma once

#include <concepts>

namespace gba {
    template<typename...> struct plex;
}

namespace gba::bits {

/// @brief Type trait to check if a type is a plex specialization.
template<typename T>
struct is_plex : std::false_type {};

template<typename... Ts>
struct is_plex<gba::plex<Ts...>> : std::true_type {};

/// @brief Concept for plex types that can be safely bit-casted to a register-sized integer.
/// @note Only 32-bit plex types are supported for register writes.
template<typename T>
concept IsRegisterPlex = is_plex<T>::value && sizeof(T) == sizeof(unsigned int);

struct read_ops {
    template<typename Self>
    typename Self::value_type value(this const Self& self) noexcept;
};

struct write_ops {
    template<typename Self, typename Other>
    void copy(this const Self& lhs, const Other& rhs) noexcept requires std::derived_from<Other, read_ops> && std::same_as<typename Self::value_type, typename Other::value_type>;
protected:
    template<typename Self>
    void write(this const Self& lhs, typename Self::value_type&& rhs) noexcept;

    template<typename Self>
    void write(this const Self& lhs, const typename Self::value_type& rhs) noexcept;

    template<typename Self>
    void write_integer(this const Self& lhs, std::integral auto value) noexcept;
};

struct read_write_ops : read_ops, write_ops {
    template<typename Self>
    void swap(this const Self& lhs, const Self& rhs) noexcept;

    template<typename Self>
    void swap(this const Self& lhs, typename Self::value_type& rhs) noexcept;
};

}

#include "registral_ops.tpp"
