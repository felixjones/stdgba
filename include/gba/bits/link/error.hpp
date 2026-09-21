/// @file error.hpp
/// @brief Sticky error state shared by typed link transports.
#pragma once

#include <cstdint>

namespace gba::bits::link {

    enum class link_error : std::uint16_t {
        none = 0,
        receive_overflow = 1u << 0u,
        malformed_frame = 1u << 1u,
        integrity_failure = 1u << 2u,
        decode_failure = 1u << 3u,
        hardware_failure = 1u << 4u,
        topology_changed = 1u << 5u,
    };

    [[nodiscard]] constexpr link_error operator|(const link_error lhs, const link_error rhs) noexcept {
        return static_cast<link_error>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
    }

    [[nodiscard]] constexpr link_error operator&(const link_error lhs, const link_error rhs) noexcept {
        return static_cast<link_error>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
    }

    constexpr link_error& operator|=(link_error& lhs, const link_error rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    struct link_error_state {
        constexpr void set(const link_error error) noexcept { m_errors |= error; }

        constexpr void clear(const link_error error) noexcept {
            m_errors =
                static_cast<link_error>(static_cast<std::uint16_t>(m_errors) & ~static_cast<std::uint16_t>(error));
        }

        constexpr void clear() noexcept { m_errors = link_error::none; }

        [[nodiscard]] constexpr bool has(const link_error error) const noexcept {
            return (m_errors & error) != link_error::none;
        }

        [[nodiscard]] constexpr bool any() const noexcept { return m_errors != link_error::none; }

        [[nodiscard]] constexpr link_error value() const noexcept { return m_errors; }

    private:
        link_error m_errors = link_error::none;
    };

} // namespace gba::bits::link
