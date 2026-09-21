/// @file irq_snapshot_queue.hpp
/// @brief Fixed-capacity IRQ-to-main-context snapshot handoff.
#pragma once

#include <gba/critical_section>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace gba::bits::link {

    template<typename T, std::size_t Capacity>
    struct irq_snapshot_queue {
        static_assert(Capacity > 0);

        using index_type = std::conditional_t<
            (Capacity <= std::numeric_limits<std::uint8_t>::max()), std::uint8_t,
            std::conditional_t<(Capacity <= std::numeric_limits<std::uint16_t>::max()), std::uint16_t, std::size_t>>;

        [[nodiscard]] bool push_from_irq(const T& value) noexcept {
            if (m_size == Capacity) {
                m_overflow = true;
                return false;
            }
            const auto tail = static_cast<index_type>((static_cast<std::size_t>(m_head) + m_size) % Capacity);
            m_values[tail] = value;
            m_size = static_cast<index_type>(m_size + 1);
            return true;
        }

        [[nodiscard]] std::optional<T> pop() noexcept {
            if (m_size == 0) {
                return std::nullopt;
            }
            const gba::critical_section section;
            if (m_size == 0) {
                return std::nullopt;
            }
            T value = m_values[m_head];
            m_head = static_cast<index_type>((static_cast<std::size_t>(m_head) + 1) % Capacity);
            m_size = static_cast<index_type>(m_size - 1);
            return value;
        }

        [[nodiscard]] bool consume_overflow() noexcept {
            if (!m_overflow) {
                return false;
            }
            const gba::critical_section section;
            const bool overflow = m_overflow;
            m_overflow = false;
            return overflow;
        }

        void clear() noexcept {
            const gba::critical_section section;
            m_head = 0;
            m_size = 0;
            m_overflow = false;
        }

    private:
        std::array<T, Capacity> m_values{};
        volatile index_type m_head{};
        volatile index_type m_size{};
        volatile bool m_overflow{};
    };

} // namespace gba::bits::link
