/// @file byte_ring.hpp
/// @brief Fixed-capacity, non-overwriting byte ring for incoming link data.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace gba::bits::link {

    template<std::size_t Capacity>
        requires(Capacity > 0)
    struct byte_ring {
        using index_type = std::conditional_t<
            (Capacity <= std::numeric_limits<std::uint8_t>::max()), std::uint8_t,
            std::conditional_t<(Capacity <= std::numeric_limits<std::uint16_t>::max()), std::uint16_t, std::size_t>>;

        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_size; }

        [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

        [[nodiscard]] constexpr std::size_t free_space() const noexcept { return Capacity - m_size; }

        [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

        [[nodiscard]] constexpr bool full() const noexcept { return m_size == Capacity; }

        [[nodiscard]] constexpr bool push(const std::byte byte) noexcept {
            if (full()) {
                return false;
            }
            m_data[advance(m_head, m_size)] = byte;
            ++m_size;
            return true;
        }

        [[nodiscard]] constexpr std::size_t push(const std::span<const std::byte> data) noexcept {
            const std::size_t count = data.size() < free_space() ? data.size() : free_space();
            for (std::size_t i = 0; i < count; ++i) {
                m_data[advance(m_head, m_size)] = data[i];
                ++m_size;
            }
            return count;
        }

        [[nodiscard]] constexpr bool pop(std::byte& out) noexcept {
            if (empty()) {
                return false;
            }
            out = m_data[m_head];
            m_head = advance(m_head, 1);
            --m_size;
            return true;
        }

        [[nodiscard]] constexpr std::size_t pop(const std::span<std::byte> out) noexcept {
            const std::size_t count = out.size() < m_size ? out.size() : m_size;
            for (std::size_t i = 0; i < count; ++i) {
                out[i] = m_data[advance(m_head, i)];
            }
            m_head = advance(m_head, count);
            m_size -= count;
            return count;
        }

        constexpr void clear() noexcept {
            m_head = 0;
            m_size = 0;
        }

    private:
        [[nodiscard]] static constexpr index_type advance(const index_type index, const index_type by) noexcept {
            return static_cast<index_type>((static_cast<std::size_t>(index) + by) % Capacity);
        }

        std::array<std::byte, Capacity> m_data{};
        index_type m_head{};
        index_type m_size{};
    };

} // namespace gba::bits::link
