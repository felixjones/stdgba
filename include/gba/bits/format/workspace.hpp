/// @file bits/format/workspace.hpp
/// @brief Workspace types for gba::format.

#pragma once

#include <array>
#include <cstddef>

namespace gba::format {
    /// @brief External workspace for user-provided memory.
    struct external_workspace {
        char* buffer;
        std::size_t capacity;
        std::size_t used = 0;

        constexpr external_workspace(char* buf, std::size_t cap) : buffer{buf}, capacity{cap} {}

        constexpr char* allocate(std::size_t n) {
            if (used + n > capacity) return nullptr;
            char* ptr = buffer + used;
            used += n;
            return ptr;
        }

        constexpr void reset() { used = 0; }
    };

    /// @brief Internal workspace with compile-time sized buffer.
    template<std::size_t Size>
    struct internal_workspace {
        std::array<char, Size> buffer{};
        std::size_t used = 0;

        constexpr char* allocate(std::size_t n) {
            if (used + n > Size) return nullptr;
            char* ptr = buffer.data() + used;
            used += n;
            return ptr;
        }

        constexpr void reset() { used = 0; }
    };
} // namespace gba::format
