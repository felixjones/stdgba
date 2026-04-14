/// @file bits/ecs/group.hpp
/// @brief Component group concept for batching related components.
#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace gba::ecs {

    /// @brief A named group of related components.
    ///
    /// Groups provide logical organization of components without affecting
    /// runtime behavior. They are flattened at compile time into the registry's
    /// internal component list.
    ///
    /// @code{.cpp}
    /// // Group related components
    /// using physics = gba::ecs::group<position, velocity, acceleration>;
    /// using rendering = gba::ecs::group<sprite_id, palette_id, x_offset>;
    ///
    /// // Use groups in registry declaration
    /// gba::ecs::registry<128, physics, rendering> world;
    ///
    /// // Registry automatically flattens to:
    /// // gba::ecs::registry<128, position, velocity, acceleration,
    /// //                        sprite_id, palette_id, x_offset>
    ///
    /// // Use view() and emplace() just like normal:
    /// world.view<position, velocity>().each([](position& p, velocity& v) {
    ///     p.x += v.vx;
    /// });
    /// @endcode
    template<typename... Cs>
    struct group {
        /// Marker to identify this as a group.
        static constexpr bool is_ecs_group = true;

        /// Tuple of the contained component types (for metaprogramming).
        using components_tuple = std::tuple<Cs...>;
    };

    namespace detail {
        template<typename T, typename G>
        struct contains;

        template<typename T, typename... Cs>
        struct contains<T, group<Cs...>> : std::bool_constant<(std::is_same_v<T, Cs> || ...)> {};

        template<typename G, typename T>
        struct append_if_missing;

        template<typename... Cs, typename T>
        struct append_if_missing<group<Cs...>, T> {
            using type = std::conditional_t<contains<T, group<Cs...>>::value, group<Cs...>, group<Cs..., T>>;
        };

        template<typename Acc, typename T>
        struct flatten_one {
            using type = typename append_if_missing<Acc, T>::type;
        };

        template<typename Acc, typename... Nested>
        struct flatten_one<Acc, group<Nested...>>;

        template<typename Acc, typename... Ts>
        struct flatten_groups_acc;

        template<typename Acc>
        struct flatten_groups_acc<Acc> {
            using type = Acc;
        };

        template<typename Acc, typename T, typename... Rest>
        struct flatten_groups_acc<Acc, T, Rest...> {
            using next = typename flatten_one<Acc, T>::type;
            using type = typename flatten_groups_acc<next, Rest...>::type;
        };

        template<typename Acc, typename... Nested>
        struct flatten_one<Acc, group<Nested...>> {
            using type = typename flatten_groups_acc<Acc, Nested...>::type;
        };

        /// @brief Helper to extract all components from a mixed list of types and groups.
        ///
        /// Unwraps all `group<Cs...>` instances and deduplicates component types
        /// while preserving first-seen order.
        template<typename... Ts>
        struct flatten_groups {
            using type = typename flatten_groups_acc<group<>, Ts...>::type;
        };

    } // namespace detail

    /// @brief Convenience alias to flatten a mixed list of types and groups.
    ///
    /// Transforms `flatten_groups_t<T1, T2, group<T3, T4>, T5>` into `group<T1, T2, T3, T4, T5>`.
    template<typename... Ts>
    using flatten_groups_t = typename detail::flatten_groups<Ts...>::type;

} // namespace gba::ecs
