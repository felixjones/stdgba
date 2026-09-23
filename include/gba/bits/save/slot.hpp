/// @file bits/save/slot.hpp
/// @brief Type-erased global slot holding the active save table, mirroring `gba::link`'s slot.
#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace gba::bits::backup {

    /// @brief One process-wide save table slot, type-erased the same way as `gba::link`'s
    /// slot: assigning a table (typically the result of a `gba::backup_*` factory) determines
    /// `backup_slot`'s held type until the next assignment. `get<Held>()`, `emplace<Held>(...)`
    /// then reach it directly, by naming the concrete table type (as `gba::link` names a
    /// concrete link type).
    ///
    /// @code{.cpp}
    /// using save_table = decltype(gba::backup_sram<player_codec, settings_codec>)::table_type;
    ///
    /// auto& backup = gba::backup.emplace(gba::backup_sram<player_codec, settings_codec>);
    /// if (!backup.get<settings_save>().has_value()) {
    ///     backup.emplace<settings_save>(); // default construct settings_save
    /// }
    /// // ...elsewhere, without the captured reference:
    /// gba::backup.get<save_table>().emplace<player_save>(100u, 2);
    /// @endcode
    struct backup_slot {
    public:
        ~backup_slot() = default;

        [[nodiscard]] static backup_slot& instance() noexcept { return s_instance; }

        backup_slot(const backup_slot&) = delete;
        backup_slot& operator=(const backup_slot&) = delete;

        /// @brief Replace the held table, e.g. `backup = backup_sram<...>`.
        template<typename Held>
            requires(!std::same_as<std::remove_cvref_t<Held>, backup_slot>)
        decltype(auto) operator=(Held&& value) {
            return emplace(std::forward<Held>(value));
        }

        /// @brief Replace the held table, e.g. `backup.emplace(backup_sram<...>)`. A zero-argument
        /// callable `Held` (a `backup_*` factory) is invoked here to build the actual table.
        template<typename Held>
            requires(!std::same_as<std::remove_cvref_t<Held>, backup_slot>)
        decltype(auto) emplace(Held&& value) {
            if constexpr (requires(const std::remove_cvref_t<Held>& factory) { factory(); }) {
                return emplace(value());
            } else {
                return emplace_held<std::remove_cvref_t<Held>>(std::forward<Held>(value));
            }
        }

        /// @brief Construct a `Held` from `args` (default: none), replacing the held table.
        template<typename Held, typename... Args>
            requires std::constructible_from<Held, Args...>
        Held& emplace(Args&&... args) {
            return emplace_held<Held>(std::forward<Args>(args)...);
        }

        /// @brief The held table; its type must be named explicitly, as with `gba::link::get()`.
        template<typename Held>
        [[nodiscard]] Held* get_if() const noexcept {
            return holds<Held>() ? static_cast<Held*>(m_held) : nullptr;
        }

        /// @brief The held table; traps if its type is not `Held`.
        template<typename Held>
        [[nodiscard]] Held& get() const noexcept {
            if (auto* held = get_if<Held>()) {
                return *held;
            }
            __builtin_trap();
        }

        /// @brief Whether the currently held table's type is `Held`.
        template<typename Held>
        [[nodiscard]] bool holds() const noexcept {
            return m_type == type_tag<Held>();
        }

        void reset() noexcept {
            if (m_destroy) {
                m_destroy();
            }
            m_held = nullptr;
            m_type = nullptr;
            m_destroy = nullptr;
        }

        /// @brief Whether a table is currently held.
        [[nodiscard]] explicit operator bool() const noexcept { return m_held != nullptr; }

    private:
        backup_slot() = default;

        template<typename Held, typename... Args>
        Held& emplace_held(Args&&... args) {
            reset();
            storage<Held>.emplace(std::forward<Args>(args)...);
            m_held = std::addressof(*storage<Held>);
            m_type = type_tag<Held>();
            m_destroy = [] {
                storage<Held>.reset();
            };
            return *static_cast<Held*>(m_held);
        }

        static backup_slot s_instance;

        template<typename Held>
        static inline std::optional<Held> storage{};

        template<typename Held>
        [[nodiscard]] static const void* type_tag() noexcept {
            return std::addressof(storage<Held>);
        }

        void* m_held{};
        const void* m_type{};
        void (*m_destroy)(){};
    };

    inline backup_slot backup_slot::s_instance{};

} // namespace gba::bits::backup
