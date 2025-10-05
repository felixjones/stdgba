#pragma once

#include <concepts>

namespace gba::bits {

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
