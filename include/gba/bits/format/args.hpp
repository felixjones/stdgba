/// @file bits/format/args.hpp
/// @brief Bound argument helpers for gba::format.

#pragma once

#include <gba/bits/format/fixed_string.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace gba::format {
    template<typename... BoundArgs>
    struct arg_pack {
        std::tuple<BoundArgs...> args;

        constexpr arg_pack(BoundArgs... a) : args{std::move(a)...} {}
    };

    template<>
    struct arg_pack<> {
        std::tuple<> args;

        constexpr arg_pack() = default;
    };

    template<unsigned int Hash, typename T>
    struct bound_arg {
        static constexpr unsigned int hash = Hash;
        static constexpr bool is_supplier = std::invocable<T>;
        T stored;

        constexpr decltype(auto) get() const {
            if constexpr (is_supplier) return stored();
            else return stored;
        }
    };

    template<fixed_string Name>
    struct arg_binder {
        static constexpr auto hash = fnv1a_hash<Name>();

        template<typename T>
        constexpr auto operator()(T&& v) const {
            return bound_arg<hash, std::decay_t<T>>{std::forward<T>(v)};
        }

        template<typename T>
        constexpr auto operator=(T&& v) const {
            return bound_arg<hash, std::decay_t<T>>{std::forward<T>(v)};
        }
    };
} // namespace gba::format
