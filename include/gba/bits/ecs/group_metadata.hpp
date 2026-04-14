/// @file bits/ecs/group_metadata.hpp
/// @brief Compile-time metadata for component groups.
#pragma once

#include <gba/bits/ecs/group.hpp>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace gba::ecs {

    namespace detail {
        /// Forward declaration for registry template
        template<std::size_t Capacity, typename... Components>
        class registry_impl;

        template<typename Target, typename... Ts>
        consteval std::size_t group_type_index() {
            constexpr bool matches[] = {std::is_same_v<Target, Ts>...};
            for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
                if (matches[i]) return i;
            }
            throw "group_metadata: component type not registered";
        }
    }

    /// @brief Precomputed compile-time metadata for a component group within a registry.
    ///
    /// This template is specialized for a specific registry to compute group-specific
    /// properties like membership bitmask, component count, and type tuple.
    ///
    /// Enables optimizations:
    /// - Precomputed bitmasks for group-aware queries
    /// - Fast membership testing
    /// - Group-level component access patterns
    ///
    /// @tparam Registry The registry type (for bitmask computation)
    /// @tparam G The group type to compute metadata for
    template<typename Registry, typename G>
    struct group_metadata : group_metadata<Registry, flatten_groups_t<G>> {};

    /// Specialization for a flattened group within a registry.
    ///
    /// Provides:
    /// - `mask`: precomputed bitmask for all components in the group
    /// - `component_count`: number of components in the group
    /// - `types`: tuple type containing all component types
    template<std::size_t Capacity, typename... RegCs, typename... GroupCs>
    struct group_metadata<detail::registry_impl<Capacity, RegCs...>, group<GroupCs...>> {
        /// Precomputed bitmask for all components in this group.
        ///
        /// Useful for fast membership testing and group-aware view filtering.
        static constexpr std::uint32_t mask = ((std::uint32_t{1} << detail::group_type_index<GroupCs, RegCs...>()) | ...);

        /// Number of components in this group.
        static constexpr std::size_t component_count = sizeof...(GroupCs);

        /// Tuple of component types in this group.
        using types = std::tuple<GroupCs...>;

        /// Helper: check if an entity has all components in this group.
        ///
        /// Given a slot's mask from the registry, returns true if the mask
        /// indicates that all components in this group are present.
        [[gnu::always_inline]] static constexpr bool has_all_in_slot(std::uint32_t slot_mask) noexcept {
            return (slot_mask & mask) == mask;
        }
    };

} // namespace gba::ecs
