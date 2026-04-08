/// @file bits/codegen/patch_args.hpp
/// @brief Bound patch argument helpers for gba::codegen.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace gba::codegen {

    /// @brief A compile-time fixed-size string for use as NTTP.
    template<std::size_t N>
    struct fixed_string {
        char data[N]{};
        static constexpr std::size_t size = N - 1; // Exclude null terminator

        consteval fixed_string() = default;

        consteval fixed_string(const char (&str)[N]) { std::copy_n(str, N, data); }

        [[nodiscard]] consteval char operator[](std::size_t i) const { return data[i]; }

        [[nodiscard]] consteval const char* begin() const { return data; }
        [[nodiscard]] consteval const char* end() const { return data + size; }

        [[nodiscard]] consteval bool empty() const { return size == 0; }

        template<std::size_t M>
        [[nodiscard]] consteval bool operator==(const fixed_string<M>& other) const {
            if constexpr (N != M) {
                return false;
            } else {
                for (std::size_t i = 0; i < N; ++i) {
                    if (data[i] != other.data[i]) return false;
                }
                return true;
            }
        }
    };

    template<std::size_t N>
    fixed_string(const char (&)[N]) -> fixed_string<N>;

    /// @brief FNV-1a hash for compile-time string hashing.
    consteval unsigned int fnv1a_hash(const char* str, std::size_t len) {
        unsigned int hash = 2166136261u;
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<unsigned int>(str[i]);
            hash *= 16777619u;
        }
        return hash;
    }

    template<fixed_string S>
    consteval unsigned int fnv1a_hash() {
        return fnv1a_hash(S.data, S.size);
    }

    /// @brief Bound patch argument: stores hash + value with get() method.
    template<unsigned int Hash, typename T>
    struct bound_patch_arg {
        static constexpr unsigned int hash = Hash;
        static constexpr bool is_supplier = std::invocable<T>;
        T stored;

        constexpr decltype(auto) get() const {
            if constexpr (is_supplier) return stored();
            else return stored;
        }
    };

    /// @brief Patch argument binder: creates bound_patch_arg via operator= / operator().
    template<fixed_string Name>
    struct patch_arg_binder {
        static constexpr auto hash = fnv1a_hash<Name>();

        template<typename T>
        constexpr auto operator()(T&& v) const {
            return bound_patch_arg<hash, std::decay_t<T>>{std::forward<T>(v)};
        }

        template<typename T>
        constexpr auto operator=(T&& v) const {
            return bound_patch_arg<hash, std::decay_t<T>>{std::forward<T>(v)};
        }
    };

    namespace literals {
        template<fixed_string Name>
        consteval auto operator""_arg() {
            return patch_arg_binder<Name>{};
        }
    }

} // namespace gba::codegen
