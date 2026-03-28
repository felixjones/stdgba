/// @file bits/ecs/entity.hpp
/// @brief Entity identifier and ECS utility types.
#pragma once

#include <cstddef>
#include <cstdint>

namespace gba::ecs {

    /// @brief Entity identifier: slot (low byte) + generation (high byte) packed in uint16_t.
    ///
    /// Slot identifies the storage index. Generation detects use-after-destroy.
    /// Maximum 255 live entities (slots 0-254); slot 255 is reserved for null.
    struct entity_id {
        std::uint16_t value;

        /// @brief Extract the storage slot index (low 8 bits).
        [[nodiscard]] constexpr std::uint8_t slot() const noexcept { return static_cast<std::uint8_t>(value); }

        /// @brief Extract the generation counter (high 8 bits).
        [[nodiscard]] constexpr std::uint8_t generation() const noexcept {
            return static_cast<std::uint8_t>(value >> 8);
        }

        /// @brief Construct an entity_id from slot and generation.
        [[nodiscard]] static constexpr entity_id make(std::uint8_t slot, std::uint8_t gen) noexcept {
            return {static_cast<std::uint16_t>(static_cast<std::uint16_t>(gen) << 8 | slot)};
        }

        constexpr bool operator==(const entity_id&) const = default;
        constexpr bool operator!=(const entity_id&) const = default;
    };

    /// @brief Sentinel entity: never valid in any registry.
    inline constexpr entity_id null{0xFFFF};

    /// @brief Padding utility for power-of-two component size compliance.
    ///
    /// @code{.cpp}
    /// struct sprite_id {
    ///     std::uint8_t id;
    ///     gba::ecs::pad<3> _;   // pad to 4 bytes (power of two)
    /// };
    /// static_assert(sizeof(sprite_id) == 4);
    /// @endcode
    template<std::size_t N>
    struct pad {
        std::byte data[N]{};
    };

} // namespace gba::ecs
