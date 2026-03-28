/// @file bits/ecs/registry.hpp
/// @brief Static ECS registry with compile-time component list.
#pragma once

#include <gba/bits/ecs/entity.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

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

    } // namespace detail

    /// @brief Static ECS registry with compile-time component list and fixed capacity.
    ///
    /// All storage is inline (zero heap allocation). Component pools are flat
    /// arrays indexed by entity slot, enabling O(1) random access with no
    /// indirection. Masks track alive state and component presence per slot.
    ///
    /// Registry method names: `create`, `destroy`, `valid`, `emplace`,
    /// `remove`, `get`, `all_of`, `any_of`, `view`, `size`, `clear`.
    ///
    /// @tparam Capacity Maximum number of live entities (1-255).
    /// @tparam Components All component types. Each must have power-of-two size.
    ///
    /// @code{.cpp}
    /// struct position { int x, y; };   // 8 bytes (ok)
    /// struct velocity { int vx, vy; }; // 8 bytes (ok)
    ///
    /// gba::ecs::registry<128, position, velocity> world;
    /// auto e = world.create();
    /// world.emplace<position>(e, 10, 20);
    /// world.emplace<velocity>(e, 1, 0);
    ///
    /// world.view<position, velocity>().each([](position& pos, const velocity& vel) {
    ///     pos.x += vel.vx;
    ///     pos.y += vel.vy;
    /// });
    /// @endcode
    template<std::size_t Capacity, typename... Components>
    class registry {
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
        // -- View ---------------------------------------------------------

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

            registry* m_reg;

        public:
            explicit constexpr basic_view(registry* r) noexcept : m_reg(r) {}

            /// Sentinel type for range-based for.
            struct sentinel {};

            /// Forward iterator over matching entity slots.
            class iterator {
                registry* m_reg;
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
                constexpr iterator(registry* r, unsigned int start, unsigned int end, bool allMatch) noexcept
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
                    if (m_reg->m_alive == m_reg->m_next_slot) {
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
                    if (m_reg->m_alive == m_reg->m_next_slot) {
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
            std::uint8_t slot;
            if (m_free_top > 0) {
                slot = m_free_stack[--m_free_top];
            } else {
                slot = m_next_slot++;
            }
            m_mask[slot] = alive_bit;
            m_alive_list[m_alive] = slot;
            m_alive_index[slot] = m_alive;
            ++m_alive;
            return entity_id::make(slot, m_gen[slot]);
        }

        /// @brief Destroy an entity, freeing its slot for reuse.
        constexpr void destroy(entity_id e) {
            if consteval {
                if (!valid(e)) throw "registry::destroy: invalid entity";
            }
            const auto slot = e.slot();
            // Decrement per-component counts before clearing mask
            ((m_mask[slot] & bit_of<Components> ? --m_component_count[index_of<Components>] : 0), ...);
            m_mask[slot] = 0;
            ++m_gen[slot];
            m_free_stack[m_free_top++] = slot;
            // Swap-and-pop from alive list (O(1))
            --m_alive;
            const auto idx = m_alive_index[slot];
            const auto lastSlot = m_alive_list[m_alive];
            m_alive_list[idx] = lastSlot;
            m_alive_index[lastSlot] = idx;
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

} // namespace gba::ecs
