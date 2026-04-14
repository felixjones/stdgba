/// @file bits/ecs/registry.hpp
/// @brief Static ECS registry with compile-time component list.
#pragma once

#include <gba/bits/ecs/entity.hpp>
#include <gba/bits/ecs/group.hpp>
#include <gba/bits/ecs/group_metadata.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace gba::ecs {

    namespace detail {

        /// @brief Consteval type index lookup in a parameter pack.
        ///
        /// Returns the zero-based index of @p Target in @p Ts... .
        /// Throws a compile-time diagnostic if the type is not found.
        template<typename Target, typename... Ts>
        consteval std::size_t type_index() {
            constexpr bool matches[] = {std::is_same_v<Target, Ts>...};
            for (std::size_t i = 0; i < sizeof...(Ts); ++i) {
                if (matches[i]) return i;
            }
            throw "component type not registered in this registry";
        }

        /// @brief Extract component types from a flattened group.
        template<typename G>
        struct extract_components;

        template<typename... Cs>
        struct extract_components<group<Cs...>> {
            using type = std::tuple<Cs...>;
        };

    } // namespace detail

    template<std::size_t Capacity, typename... Components>
    class registry_impl;

    /// @brief The actual registry implementation with flattened components.
    ///
    /// This is the core ECS implementation. Users typically use the wrapper
    /// `registry<Capacity, ComponentsAndGroups...>` instead, which flattens
    /// component groups before delegating here.
    template<std::size_t Capacity, typename... Components>
    class registry_impl {
        static_assert(Capacity > 0 && Capacity <= 255, "capacity must be in [1, 255]");
        static_assert(sizeof...(Components) > 0 && sizeof...(Components) <= 31, "component count must be in [1, 31]");
        static_assert(((std::has_single_bit(sizeof(Components))) && ...), "all component sizes must be powers of two");

        // -- Type-level helpers -------------------------------------------

        /// Index of component C in the Components... pack.
        template<typename C>
        static constexpr std::size_t index_of = detail::type_index<C, Components...>();

        /// Bitmask for component C.
        template<typename C>
        static constexpr std::uint32_t bit_of = std::uint32_t{1} << index_of<C>;

        /// Bit 31 marks an entity slot as alive (independent of components).
        static constexpr std::uint32_t alive_bit = std::uint32_t{1} << 31;

        // -- Storage ------------------------------------------------------
        // Layout: small hot metadata first (low offsets -> cheap Thumb access
        // in emplace/destroy), large pools last (each_arm uses ARM mode with
        // 12-bit immediates, so high offsets are free).

        /// Per-component alive count (enables mask-check elision when all match).
        std::uint8_t m_component_count[sizeof...(Components)]{};

        /// Number of entries in the free stack.
        std::uint8_t m_free_top{0};

        /// Next virgin slot (high-water mark).
        std::uint8_t m_next_slot{0};

        /// Number of currently alive entities.
        std::uint8_t m_alive{0};

        /// True when alive slots are exactly [0, m_alive).
        std::uint8_t m_dense_prefix{1};

        /// Per-slot bitmask: bit 31 = alive, bits 0-30 = component presence.
        std::uint32_t m_mask[Capacity]{};

        /// Per-slot generation counter (incremented on destroy).
        std::uint8_t m_gen[Capacity]{};

        /// Recycled-slot stack (LIFO).
        std::uint8_t m_free_stack[Capacity]{};

        /// Dense packed array of alive entity slots (iteration order).
        std::uint8_t m_alive_list[Capacity]{};

        /// Reverse map: slot -> index in m_alive_list (for O(1) swap-and-pop).
        std::uint8_t m_alive_index[Capacity]{};

        /// Flat component pools (one array per component type).
        std::tuple<std::array<Components, Capacity>...> m_pools{};

        [[gnu::always_inline]] constexpr std::uint8_t allocate_slot() noexcept {
            std::uint8_t slot;
            if (m_free_top > 0) {
                slot = m_free_stack[--m_free_top];
            } else {
                slot = m_next_slot++;
            }

            const auto oldAlive = m_alive;
            if (m_dense_prefix && slot != oldAlive) m_dense_prefix = 0;

            m_mask[slot] = alive_bit;
            m_alive_list[oldAlive] = slot;
            m_alive_index[slot] = oldAlive;
            ++m_alive;

            // Recovered when there are no free slots and alive exactly matches prefix size.
            if (!m_dense_prefix && m_free_top == 0 && m_next_slot == m_alive) m_dense_prefix = 1;
            return slot;
        }

        /// Access the raw pool for component C.
        template<typename C>
        [[gnu::always_inline]] constexpr auto& pool() noexcept {
            return std::get<index_of<C>>(m_pools);
        }
        template<typename C>
        [[gnu::always_inline]] constexpr const auto& pool() const noexcept {
            return std::get<index_of<C>>(m_pools);
        }

        /// Access component C at a given slot.
        template<typename C>
        [[gnu::always_inline]] constexpr C& pool_ref(unsigned int slot) noexcept {
            return std::get<index_of<C>>(m_pools)[slot];
        }
        template<typename C>
        [[gnu::always_inline]] constexpr const C& pool_ref(unsigned int slot) const noexcept {
            return std::get<index_of<C>>(m_pools)[slot];
        }

    public:
        /// @brief Lightweight view over entities matching a component set.
        ///
        /// Supports range-based for (structured bindings) and `.each()`.
        ///
        /// @code{.cpp}
        /// for (auto [pos, vel] : world.view<position, velocity>()) {
        ///     pos.x += vel.vx;
        /// }
        /// // or:
        /// world.view<position, velocity>().each([](position& p, velocity& v) { ... });
        /// @endcode
        template<typename... ViewCs>
        class basic_view {
            static_assert(sizeof...(ViewCs) > 0, "view requires at least one component");

            /// Required mask: alive + all requested components.
            static constexpr std::uint32_t required = (bit_of<ViewCs> | ...) | alive_bit;

            registry_impl* m_reg;

        public:
            explicit constexpr basic_view(registry_impl* r) noexcept : m_reg(r) {}

            /// Sentinel type for range-based for.
            struct sentinel {};

            /// Forward iterator over matching entity slots.
            class iterator {
                registry_impl* m_reg;
                unsigned int m_idx;
                unsigned int m_end;
                bool m_all_match;

                constexpr void advance() noexcept {
                    if (m_all_match) return;
                    while (m_idx < m_end) {
                        const auto slot = m_reg->m_alive_list[m_idx];
                        if ((m_reg->m_mask[slot] & required) == required) return;
                        ++m_idx;
                    }
                }

            public:
                constexpr iterator(registry_impl* r, unsigned int start, unsigned int end, bool allMatch) noexcept
                    : m_reg(r), m_idx(start), m_end(end), m_all_match(allMatch) {
                    advance();
                }

                /// @brief Dereference: returns tuple of component references.
                constexpr auto operator*() const noexcept {
                    const auto slot = m_reg->m_alive_list[m_idx];
                    return std::tie(m_reg->template pool_ref<ViewCs>(slot)...);
                }

                constexpr iterator& operator++() noexcept {
                    ++m_idx;
                    advance();
                    return *this;
                }

                constexpr bool operator==(sentinel) const noexcept { return m_idx >= m_end; }
                constexpr bool operator!=(sentinel) const noexcept { return m_idx < m_end; }
            };

            [[nodiscard]] constexpr iterator begin() const noexcept {
                const bool allMatch = ((m_reg->m_component_count[index_of<ViewCs>] == m_reg->m_alive) && ...);
                return {m_reg, 0u, m_reg->m_alive, allMatch};
            }

            [[nodiscard]] constexpr sentinel end() const noexcept { return {}; }

            /// @brief Iterate all matching entities with a callback.
            ///
            /// Callback signature: `void(ViewCs&...)` or `void(entity_id, ViewCs&...)`.
            /// The entity_id overload is auto-detected at compile time.
            ///
            /// Three runtime paths selected by per-component count metadata:
            ///   1. Dense + all-match: slot == j, no mask check (fastest).
            ///   2. All-match: alive_list indirection, no mask check.
            ///   3. Mixed: alive_list + mask check (general fallback).
            template<typename Fn>
            [[gnu::always_inline]] constexpr void each(Fn&& fn) const {
                const unsigned int count = m_reg->m_alive;
                const bool allMatch = ((m_reg->m_component_count[index_of<ViewCs>] == m_reg->m_alive) && ...);

                auto invoke = [&](unsigned int slot) {
                    if constexpr (std::is_invocable_v<Fn, entity_id, ViewCs&...>) {
                        fn(entity_id::make(static_cast<std::uint8_t>(slot), m_reg->m_gen[slot]),
                           m_reg->template pool_ref<ViewCs>(slot)...);
                    } else {
                        fn(m_reg->template pool_ref<ViewCs>(slot)...);
                    }
                };

                if (allMatch) {
                    if (m_reg->m_dense_prefix) {
                        for (unsigned int slot = 0; slot < count; ++slot) invoke(slot);
                    } else {
                        for (unsigned int j = 0; j < count; ++j) invoke(m_reg->m_alive_list[j]);
                    }
                } else {
                    for (unsigned int j = 0; j < count; ++j) {
                        const unsigned int slot = m_reg->m_alive_list[j];
                        if ((m_reg->m_mask[slot] & required) == required) invoke(slot);
                    }
                }
            }

            /// @brief ARM-mode IWRAM iteration for maximum runtime performance.
            ///
            /// Identical semantics to each() but compiled in ARM mode and placed
            /// in IWRAM. Runs at full 32-bit bus speed (1-cycle instruction
            /// fetch) with access to all 16 ARM registers and barrel shifter.
            /// Not constexpr. Each instantiation uses ~100-300 bytes of IWRAM.
            ///
            /// Three runtime paths (same as each()):
            ///   1. Dense + all-match: slot == j, no mask check (fastest).
            ///   2. All-match: alive_list indirection, no mask check.
            ///   3. Mixed: alive_list + mask check (general fallback).
            ///
            /// @code{.cpp}
            /// world.view<position, velocity>().each_arm([](position& p, velocity& v) {
            ///     p.x += v.vx;
            /// });
            /// @endcode
            template<typename Fn>
            [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_each"), gnu::noinline, gnu::flatten]]
            void each_arm(Fn&& fn) const {
                const unsigned int count = m_reg->m_alive;
                const bool allMatch = ((m_reg->m_component_count[index_of<ViewCs>] == m_reg->m_alive) && ...);

                auto invoke = [&](unsigned int slot) {
                    if constexpr (std::is_invocable_v<Fn, entity_id, ViewCs&...>) {
                        fn(entity_id::make(static_cast<std::uint8_t>(slot), m_reg->m_gen[slot]),
                           m_reg->template pool_ref<ViewCs>(slot)...);
                    } else {
                        fn(m_reg->template pool_ref<ViewCs>(slot)...);
                    }
                };

                if (allMatch) {
                    if (m_reg->m_dense_prefix) {
                        for (unsigned int slot = 0; slot < count; ++slot) invoke(slot);
                    } else {
                        for (unsigned int j = 0; j < count; ++j) invoke(m_reg->m_alive_list[j]);
                    }
                } else {
                    for (unsigned int j = 0; j < count; ++j) {
                        const unsigned int slot = m_reg->m_alive_list[j];
                        if ((m_reg->m_mask[slot] & required) == required) invoke(slot);
                    }
                }
            }
        };

        // -- Entity lifecycle ---------------------------------------------

        /// @brief Create a new entity. Returns its id.
        ///
        /// Consteval: throws if capacity exceeded. Runtime: UB if exceeded
        /// (no runtime capacity check).
        [[nodiscard]] constexpr entity_id create() {
            if consteval {
                if (m_alive >= static_cast<std::uint8_t>(Capacity)) throw "registry::create: capacity exceeded";
            }
            const auto slot = allocate_slot();
            return entity_id::make(slot, m_gen[slot]);
        }

        /// @brief Create and attach a fixed component set in a single pass.
        template<typename... Cs, typename... Values>
        [[nodiscard]] constexpr entity_id create_emplace(Values&&... values) {
            static_assert(sizeof...(Cs) > 0, "create_emplace requires at least one component");
            static_assert(sizeof...(Cs) == sizeof...(Values), "create_emplace requires one value per component");
            if consteval {
                if (m_alive >= static_cast<std::uint8_t>(Capacity)) throw "registry::create_emplace: capacity exceeded";
            }

            const auto slot = allocate_slot();
            m_mask[slot] = alive_bit | (bit_of<Cs> | ...);
            (++m_component_count[index_of<Cs>], ...);
            ((std::get<index_of<Cs>>(m_pools)[slot] = Cs{std::forward<Values>(values)}), ...);
            return entity_id::make(slot, m_gen[slot]);
        }

        /// @brief Destroy an entity, freeing its slot for reuse.
        constexpr void destroy(entity_id e) {
            if consteval {
                if (!valid(e)) throw "registry::destroy: invalid entity";
            }
            const auto slot = e.slot();
            // For small component sets, unrolled fold keeps destroy hot-path tight.
            if constexpr (sizeof...(Components) <= 8) {
                ((m_mask[slot] & bit_of<Components> ? --m_component_count[index_of<Components>] : 0), ...);
            } else {
                // For wider registries, visit only present bits to avoid branch fanout.
                std::uint32_t present = m_mask[slot] & ~alive_bit;
                while (present != 0) {
                    unsigned int idx = 0;
                    while ((present & (std::uint32_t{1} << idx)) == 0) ++idx;
                    --m_component_count[idx];
                    present &= (present - 1u);
                }
            }
            m_mask[slot] = 0;
            ++m_gen[slot];
            m_free_stack[m_free_top++] = slot;
            // Swap-and-pop from alive list (O(1))
            --m_alive;
            const auto idx = m_alive_index[slot];
            const auto lastSlot = m_alive_list[m_alive];
            m_alive_list[idx] = lastSlot;
            m_alive_index[lastSlot] = idx;

            if (m_dense_prefix) {
                if (slot != m_alive) m_dense_prefix = 0;
            }
        }

        /// @brief Check whether an entity id still refers to a live entity.
        [[nodiscard]] constexpr bool valid(entity_id e) const noexcept {
            const auto slot = e.slot();
            return slot < Capacity && (m_mask[slot] & alive_bit) && m_gen[slot] == e.generation();
        }

        /// @brief Number of currently alive entities.
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_alive; }

        /// @brief Destroy all entities, incrementing their generations.
        constexpr void clear() {
            for (unsigned int j = 0; j < m_alive; ++j) {
                const auto slot = m_alive_list[j];
                ++m_gen[slot];
                m_mask[slot] = 0;
            }
            for (unsigned int i = 0; i < sizeof...(Components); ++i) m_component_count[i] = 0;
            m_free_top = 0;
            m_next_slot = 0;
            m_alive = 0;
            m_dense_prefix = 1;
        }

        // -- Component operations -----------------------------------------

        /// @brief Attach a component to an entity, constructed from @p args.
        /// @return Reference to the emplaced component.
        template<typename C, typename... Args>
        constexpr C& emplace(entity_id e, Args&&... args) {
            if consteval {
                if (!valid(e)) throw "registry::emplace: invalid entity";
                if (m_mask[e.slot()] & bit_of<C>) throw "registry::emplace: component already exists";
            }
            const auto slot = e.slot();
            m_mask[slot] |= bit_of<C>;
            ++m_component_count[index_of<C>];
            auto& comp = std::get<index_of<C>>(m_pools)[slot];
            comp = C{std::forward<Args>(args)...};
            return comp;
        }

        /// @brief Remove a component from an entity.
        template<typename C>
        constexpr void remove(entity_id e) {
            if consteval {
                if (!valid(e)) throw "registry::remove: invalid entity";
            }
            m_mask[e.slot()] &= ~bit_of<C>;
            --m_component_count[index_of<C>];
        }

        /// @brief Remove one or more components without validity/presence checks.
        template<typename... Cs>
        constexpr void remove_unchecked(entity_id e) noexcept {
            static_assert(sizeof...(Cs) > 0, "remove_unchecked requires at least one component");
            const auto slot = e.slot();
            constexpr std::uint32_t clear_mask = (bit_of<Cs> | ...);
            m_mask[slot] &= ~clear_mask;
            (--m_component_count[index_of<Cs>], ...);
        }

        /// @brief Remove a component using a direct pool reference without checks.
        template<typename C>
        constexpr void remove_unchecked(C& component) noexcept {
            auto* base = std::get<index_of<C>>(m_pools).data();
            auto* ptr = std::addressof(component);
            const auto slot = static_cast<unsigned int>(ptr - base);
            m_mask[slot] &= ~bit_of<C>;
            --m_component_count[index_of<C>];
        }

        /// @brief Get a mutable reference to an entity's component.
        template<typename C>
        [[nodiscard]] constexpr C& get(entity_id e) noexcept {
            return std::get<index_of<C>>(m_pools)[e.slot()];
        }

        /// @brief Get a const reference to an entity's component.
        template<typename C>
        [[nodiscard]] constexpr const C& get(entity_id e) const noexcept {
            return std::get<index_of<C>>(m_pools)[e.slot()];
        }

        /// @brief Try to get a mutable component pointer. Returns nullptr if missing/invalid.
        template<typename C>
        [[nodiscard]] constexpr C* try_get(entity_id e) noexcept {
            if (!valid(e)) return nullptr;
            const auto slot = e.slot();
            if ((m_mask[slot] & bit_of<C>) == 0) return nullptr;
            return &std::get<index_of<C>>(m_pools)[slot];
        }

        /// @brief Try to get a const component pointer. Returns nullptr if missing/invalid.
        template<typename C>
        [[nodiscard]] constexpr const C* try_get(entity_id e) const noexcept {
            if (!valid(e)) return nullptr;
            const auto slot = e.slot();
            if ((m_mask[slot] & bit_of<C>) == 0) return nullptr;
            return &std::get<index_of<C>>(m_pools)[slot];
        }

        /// @brief Run callback with mutable component reference if present.
        /// @return true if callback executed, false if component missing/invalid entity.
        template<typename C, typename Fn>
        constexpr bool with(entity_id e, Fn&& fn) {
            if (auto* c = try_get<C>(e)) {
                if constexpr (std::is_invocable_v<Fn, entity_id, C&>) {
                    fn(e, *c);
                } else {
                    fn(*c);
                }
                return true;
            }
            return false;
        }

        /// @brief Run callback with const component reference if present.
        /// @return true if callback executed, false if component missing/invalid entity.
        template<typename C, typename Fn>
        constexpr bool with(entity_id e, Fn&& fn) const {
            if (auto* c = try_get<C>(e)) {
                if constexpr (std::is_invocable_v<Fn, entity_id, const C&>) {
                    fn(e, *c);
                } else {
                    fn(*c);
                }
                return true;
            }
            return false;
        }

        /// @brief Check if an entity has ALL of the listed components.
        template<typename... Cs>
        [[nodiscard]] constexpr bool all_of(entity_id e) const noexcept {
            constexpr auto req = (bit_of<Cs> | ...);
            return (m_mask[e.slot()] & req) == req;
        }

        /// @brief Check if an entity has ANY of the listed components.
        template<typename... Cs>
        [[nodiscard]] constexpr bool any_of(entity_id e) const noexcept {
            constexpr auto req = (bit_of<Cs> | ...);
            return (m_mask[e.slot()] & req) != 0;
        }

        // -- View creation ------------------------------------------------

        /// @brief Create a view over entities that have all listed components.
        template<typename... ViewCs>
        [[nodiscard]] constexpr basic_view<ViewCs...> view() noexcept {
            return basic_view<ViewCs...>{this};
        }
    };

    /// @brief Static ECS registry with compile-time component list and fixed capacity.
    ///
    /// All storage is inline (zero heap allocation). Component pools are flat
    /// arrays indexed by entity slot, enabling O(1) random access with no
    /// indirection. Masks track alive state and component presence per slot.
    ///
    /// Supports component groups for better organization:
    ///
    /// Registry method names: `create`, `destroy`, `valid`, `emplace`,
    /// `remove`, `remove_unchecked`, `get`, `try_get`, `with`, `match`, `match_arm`, `all_of`, `any_of`, `view`, `size`, `clear`.
    ///
    /// @tparam Capacity Maximum number of live entities (1-255).
    /// @tparam ComponentsAndGroups All component types and/or component groups.
    ///         Each component must have power-of-two size.
    ///
    /// @code{.cpp}
    /// struct position { int x, y; };   // 8 bytes (ok)
    /// struct velocity { int vx, vy; }; // 8 bytes (ok)
    /// struct health { int hp; };       // 4 bytes (ok)
    ///
    /// // Direct component list (traditional style)
    /// gba::ecs::registry<128, position, velocity, health> world1;
    ///
    /// // With component groups (recommended)
    /// using physics = gba::ecs::group<position, velocity>;
    /// using game = gba::ecs::group<health>;
    /// gba::ecs::registry<128, physics, game> world2;
    ///
    /// // Both work identically - groups are flattened at compile time
    /// auto e = world2.create();
    /// world2.emplace<position>(e, 10, 20);
    /// world2.emplace<velocity>(e, 1, 0);
    /// world2.emplace<health>(e, 100);
    ///
    /// world2.view<position, velocity>().each([](position& pos, const velocity& vel) {
    ///     pos.x += vel.vx;
    ///     pos.y += vel.vy;
    /// });
    /// @endcode
    template<std::size_t Capacity, typename... ComponentsAndGroups>
    class registry {
        // Flatten any component-group mix into a single component list.
        using flattened_components = flatten_groups_t<ComponentsAndGroups...>;

        // Extract the actual component types from the flattened list.
        using components_tuple = typename detail::extract_components<flattened_components>::type;

        // Helper to instantiate registry_impl with the flattened components
        template<typename Tuple, std::size_t... Is>
        static auto make_impl(std::index_sequence<Is...>)
            -> registry_impl<Capacity, std::tuple_element_t<Is, Tuple>...>;

        // The actual implementation type
        using impl_type = decltype(make_impl<components_tuple>(std::make_index_sequence<std::tuple_size_v<components_tuple>>{}));

        impl_type m_impl;

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr auto view_flat_impl(std::index_sequence<Is...>*, std::tuple<Cs...>*) noexcept {
            return m_impl.template view<std::tuple_element_t<Is, std::tuple<Cs...>>...>();
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr auto view_flat_impl(std::index_sequence<Is...>*, std::tuple<Cs...>*) const noexcept {
            return m_impl.template view<std::tuple_element_t<Is, std::tuple<Cs...>>...>();
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr bool all_of_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) const noexcept {
            return m_impl.template all_of<std::tuple_element_t<Is, std::tuple<Cs...>>...>(e);
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr bool any_of_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) const noexcept {
            return m_impl.template any_of<std::tuple_element_t<Is, std::tuple<Cs...>>...>(e);
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr decltype(auto) get_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) noexcept {
            if constexpr (sizeof...(Is) == 1) {
                return (m_impl.template get<std::tuple_element_t<0, std::tuple<Cs...>>>(e));
            } else {
                return std::tie(m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            }
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr decltype(auto) get_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) const noexcept {
            if constexpr (sizeof...(Is) == 1) {
                return (m_impl.template get<std::tuple_element_t<0, std::tuple<Cs...>>>(e));
            } else {
                return std::tie(m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            }
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr decltype(auto) try_get_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) noexcept {
            if constexpr (sizeof...(Is) == 1) {
                return (m_impl.template try_get<std::tuple_element_t<0, std::tuple<Cs...>>>(e));
            } else {
                return std::tuple{m_impl.template try_get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...};
            }
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr decltype(auto) try_get_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) const noexcept {
            if constexpr (sizeof...(Is) == 1) {
                return (m_impl.template try_get<std::tuple_element_t<0, std::tuple<Cs...>>>(e));
            } else {
                return std::tuple{m_impl.template try_get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...};
            }
        }

        template<typename... Query, typename... Cs, std::size_t... Is, typename Fn>
        constexpr bool with_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*, Fn&& fn) {
            auto ptrs = std::tuple{m_impl.template try_get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...};
            if (!((std::get<Is>(ptrs) != nullptr) && ...)) return false;
            if constexpr (std::is_invocable_v<Fn, entity_id, std::tuple_element_t<Is, std::tuple<Cs...>>&...>) {
                fn(e, (*std::get<Is>(ptrs))...);
            } else {
                fn((*std::get<Is>(ptrs))...);
            }
            return true;
        }

        template<typename... Query, typename... Cs, std::size_t... Is, typename Fn>
        constexpr bool with_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*, Fn&& fn) const {
            auto ptrs = std::tuple{m_impl.template try_get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...};
            if (!((std::get<Is>(ptrs) != nullptr) && ...)) return false;
            if constexpr (std::is_invocable_v<Fn, entity_id, const std::tuple_element_t<Is, std::tuple<Cs...>>&...>) {
                fn(e, (*std::get<Is>(ptrs))...);
            } else {
                fn((*std::get<Is>(ptrs))...);
            }
            return true;
        }

        template<typename... Query, typename... Cs, std::size_t... Is, typename... Values>
        [[nodiscard]] constexpr entity_id create_emplace_flat_impl(std::index_sequence<Is...>*, std::tuple<Cs...>*, Values&&... values) {
            return m_impl.template create_emplace<std::tuple_element_t<Is, std::tuple<Cs...>>...>(std::forward<Values>(values)...);
        }

        template<typename... Query, typename... Cs, std::size_t... Is>
        constexpr void remove_unchecked_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) noexcept {
            m_impl.template remove_unchecked<std::tuple_element_t<Is, std::tuple<Cs...>>...>(e);
        }

        template<typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr bool case_matches_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) {
            return m_impl.template all_of<std::tuple_element_t<Is, std::tuple<Cs...>>...>(e);
        }

        template<typename... Cs, std::size_t... Is>
        [[nodiscard]] constexpr bool case_matches_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*) const {
            return m_impl.template all_of<std::tuple_element_t<Is, std::tuple<Cs...>>...>(e);
        }

        template<typename Case>
        [[nodiscard]] constexpr bool case_matches(entity_id e) {
            using flattened = flatten_groups_t<Case>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return case_matches_flat_impl(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        template<typename Case>
        [[nodiscard]] constexpr bool case_matches(entity_id e) const {
            using flattened = flatten_groups_t<Case>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return case_matches_flat_impl(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        template<typename... Cs, std::size_t... Is, typename Fn>
        constexpr void invoke_case_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*, Fn&& fn) {
            if constexpr (std::is_invocable_v<Fn, entity_id, std::tuple_element_t<Is, std::tuple<Cs...>>&...>) {
                fn(e, m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            } else {
                fn(m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            }
        }

        template<typename... Cs, std::size_t... Is, typename Fn>
        constexpr void invoke_case_flat_impl(entity_id e, std::index_sequence<Is...>*, std::tuple<Cs...>*, Fn&& fn) const {
            if constexpr (std::is_invocable_v<Fn, entity_id, const std::tuple_element_t<Is, std::tuple<Cs...>>&...>) {
                fn(e, m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            } else {
                fn(m_impl.template get<std::tuple_element_t<Is, std::tuple<Cs...>>>(e)...);
            }
        }

        template<typename Case, typename Fn>
        constexpr void invoke_case(entity_id e, Fn&& fn) {
            using flattened = flatten_groups_t<Case>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            invoke_case_flat_impl(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr,
                std::forward<Fn>(fn)
            );
        }

        template<typename Case, typename Fn>
        constexpr void invoke_case(entity_id e, Fn&& fn) const {
            using flattened = flatten_groups_t<Case>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            invoke_case_flat_impl(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr,
                std::forward<Fn>(fn)
            );
        }

        template<typename... Cases, typename FnTuple, std::size_t... Is>
        [[nodiscard]] constexpr bool invoke_matched_cases(entity_id e,
                                                          const std::array<bool, sizeof...(Cases)>& matched,
                                                          FnTuple&& fns,
                                                          std::index_sequence<Is...>) {
            bool any = false;
            ((matched[Is] ? (invoke_case<Cases>(e, std::get<Is>(fns)), any = true, void()) : void()), ...);
            return any;
        }

        template<typename... Cases, typename FnTuple, std::size_t... Is>
        [[nodiscard]] constexpr bool invoke_matched_cases(entity_id e,
                                                          const std::array<bool, sizeof...(Cases)>& matched,
                                                          FnTuple&& fns,
                                                          std::index_sequence<Is...>) const {
            bool any = false;
            ((matched[Is] ? (invoke_case<Cases>(e, std::get<Is>(fns)), any = true, void()) : void()), ...);
            return any;
        }

    public:
        // Forward all registry methods to the implementation

        /// @brief Create a new entity.
        [[nodiscard]] constexpr entity_id create() { return m_impl.create(); }

        /// @brief Create and attach a fixed component set in one call.
        ///
        /// Supports mixed component/group query packs, e.g.
        /// `create_emplace<physics, health>(...)`.
        template<typename... Query, typename... Values>
        [[nodiscard]] constexpr entity_id create_emplace(Values&&... values) {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return create_emplace_flat_impl<Query...>(
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr,
                std::forward<Values>(values)...
            );
        }

        /// @brief Destroy an entity.
        constexpr void destroy(entity_id e) { m_impl.destroy(e); }

        /// @brief Check if entity is valid.
        [[nodiscard]] constexpr bool valid(entity_id e) const noexcept { return m_impl.valid(e); }

        /// @brief Number of alive entities.
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_impl.size(); }

        /// @brief Clear all entities.
        constexpr void clear() { m_impl.clear(); }

        /// @brief Attach a component to an entity.
        template<typename C, typename... Args>
        constexpr C& emplace(entity_id e, Args&&... args) {
            return m_impl.template emplace<C>(e, std::forward<Args>(args)...);
        }

        /// @brief Remove a component from an entity.
        template<typename C>
        constexpr void remove(entity_id e) {
            m_impl.template remove<C>(e);
        }

        /// @brief Remove one or more components from an entity without checks.
        ///
        /// Supports mixed component/group query packs.
        template<typename... Query>
        constexpr void remove_unchecked(entity_id e) noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            remove_unchecked_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Remove a component by reference without checks.
        template<typename C>
        constexpr void remove_unchecked(C& component) noexcept {
            m_impl.template remove_unchecked<C>(component);
        }

        /// @brief Get one or more components using a mixed component/group query pack.
        ///
        /// - Single resolved component -> returns `C&`
        /// - Multiple resolved components -> returns `std::tuple<...&>`
        template<typename... Query>
        [[nodiscard]] constexpr decltype(auto) get(entity_id e) noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return get_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Get one or more components using a mixed component/group query pack. Const overload.
        template<typename... Query>
        [[nodiscard]] constexpr decltype(auto) get(entity_id e) const noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return get_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Try to get one or more components using a mixed component/group query pack.
        ///
        /// - Single resolved component -> returns `C*`
        /// - Multiple resolved components -> returns `std::tuple<...*>`
        template<typename... Query>
        [[nodiscard]] constexpr decltype(auto) try_get(entity_id e) noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return try_get_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Try to get one or more components using a mixed component/group query pack. Const overload.
        template<typename... Query>
        [[nodiscard]] constexpr decltype(auto) try_get(entity_id e) const noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return try_get_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Run callback if all components from a mixed component/group query pack are present.
        ///
        /// Callback signature: `void(QueryComponents&...)` or `void(entity_id, QueryComponents&...)`.
        /// Returns false when entity is invalid or any requested component is missing.
        template<typename... Query, typename Fn>
        constexpr bool with(entity_id e, Fn&& fn) {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return with_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr,
                std::forward<Fn>(fn)
            );
        }

        /// @brief Run callback if all components from a mixed component/group query pack are present. Const overload.
        template<typename... Query, typename Fn>
        constexpr bool with(entity_id e, Fn&& fn) const {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return with_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr,
                std::forward<Fn>(fn)
            );
        }

        /// @brief Match an entity against ordered component/group query cases.
        ///
        /// Usage: `match<Case1, Case2, ...>(entity, fn1, fn2, ...)`.
        /// All cases that match at entry execute in order and return true.
        /// Matching is snapshotted before callbacks run.
        template<typename... Cases, typename... Fns>
        constexpr bool match(entity_id e, Fns&&... fns) {
            static_assert(sizeof...(Cases) > 0, "match requires at least one case query");
            static_assert(sizeof...(Cases) == sizeof...(Fns), "match requires one lambda per case query");
            const std::array<bool, sizeof...(Cases)> matched{case_matches<Cases>(e)...};
            auto fnTuple = std::forward_as_tuple(std::forward<Fns>(fns)...);
            return invoke_matched_cases<Cases...>(e, matched, fnTuple, std::index_sequence_for<Cases...>{});
        }

        /// @brief Match an entity against ordered component/group query cases. Const overload.
        template<typename... Cases, typename... Fns>
        constexpr bool match(entity_id e, Fns&&... fns) const {
            static_assert(sizeof...(Cases) > 0, "match requires at least one case query");
            static_assert(sizeof...(Cases) == sizeof...(Fns), "match requires one lambda per case query");
            const std::array<bool, sizeof...(Cases)> matched{case_matches<Cases>(e)...};
            auto fnTuple = std::forward_as_tuple(std::forward<Fns>(fns)...);
            return invoke_matched_cases<Cases...>(e, matched, fnTuple, std::index_sequence_for<Cases...>{});
        }

        /// @brief ARM/IWRAM version of match() for hot conditional dispatch paths.
        template<typename... Cases, typename... Fns>
        [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_match"), gnu::noinline, gnu::flatten]]
        bool match_arm(entity_id e, Fns&&... fns) {
            static_assert(sizeof...(Cases) > 0, "match_arm requires at least one case query");
            static_assert(sizeof...(Cases) == sizeof...(Fns), "match_arm requires one lambda per case query");
            const std::array<bool, sizeof...(Cases)> matched{case_matches<Cases>(e)...};
            auto fnTuple = std::forward_as_tuple(std::forward<Fns>(fns)...);
            return invoke_matched_cases<Cases...>(e, matched, fnTuple, std::index_sequence_for<Cases...>{});
        }

        /// @brief ARM/IWRAM version of match() for const registries.
        template<typename... Cases, typename... Fns>
        [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_match"), gnu::noinline, gnu::flatten]]
        bool match_arm(entity_id e, Fns&&... fns) const {
            static_assert(sizeof...(Cases) > 0, "match_arm requires at least one case query");
            static_assert(sizeof...(Cases) == sizeof...(Fns), "match_arm requires one lambda per case query");
            const std::array<bool, sizeof...(Cases)> matched{case_matches<Cases>(e)...};
            auto fnTuple = std::forward_as_tuple(std::forward<Fns>(fns)...);
            return invoke_matched_cases<Cases...>(e, matched, fnTuple, std::index_sequence_for<Cases...>{});
        }

        /// @brief Check if entity has ALL components from a mixed component/group query pack.
        template<typename... Query>
        [[nodiscard]] constexpr bool all_of(entity_id e) const noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return all_of_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Check if entity has ANY component from a mixed component/group query pack.
        template<typename... Query>
        [[nodiscard]] constexpr bool any_of(entity_id e) const noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return any_of_flat_impl<Query...>(
                e,
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Create a view over matching entities from a mixed component/group query pack.
        template<typename... Query>
        [[nodiscard]] constexpr auto view() noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return view_flat_impl<Query...>(
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

        /// @brief Create a view over matching entities from a mixed component/group query pack. Const overload.
        template<typename... Query>
        [[nodiscard]] constexpr auto view() const noexcept {
            using flattened = flatten_groups_t<Query...>;
            using components_tpl = typename detail::extract_components<flattened>::type;
            constexpr std::size_t count = std::tuple_size_v<components_tpl>;
            return view_flat_impl<Query...>(
                (std::make_index_sequence<count>*) nullptr,
                (components_tpl*) nullptr
            );
        }

    };

} // namespace gba::ecs
