/// @file frame.hpp
/// @brief Link stream framing, escaping, integrity, and resynchronization.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace gba::bits::link {

    inline constexpr std::byte frame_idle{0x00};
    inline constexpr std::byte frame_end{0xC0};
    inline constexpr std::byte frame_escape{0xDB};
    inline constexpr std::byte frame_escaped_end{0xDC};
    inline constexpr std::byte frame_escaped_escape{0xDD};
    inline constexpr std::byte frame_escaped_idle{0xDE};
    inline constexpr std::byte frame_escaped_disconnected{0xDF};

    inline constexpr std::uint16_t frame_crc_initial = 0xFFFF;

    [[nodiscard]] constexpr std::uint16_t frame_crc_update(std::uint16_t crc, const std::byte byte) noexcept {
        crc ^= static_cast<std::uint16_t>(std::to_integer<unsigned char>(byte)) << 8u;
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint16_t>((crc << 1u) ^ ((crc & 0x8000u) != 0 ? 0x1021 : 0));
        }
        return crc;
    }

    struct escaped_frame_byte {
        std::array<std::byte, 2> bytes{};
        std::size_t size = 1;
    };

    [[nodiscard]] constexpr escaped_frame_byte escape_frame_byte(const std::byte byte) noexcept {
        if (byte == frame_end) {
            return {
                .bytes = {frame_escape, frame_escaped_end},
                .size = 2,
            };
        }
        if (byte == frame_escape) {
            return {
                .bytes = {frame_escape, frame_escaped_escape},
                .size = 2,
            };
        }
        if (byte == frame_idle) {
            return {
                .bytes = {frame_escape, frame_escaped_idle},
                .size = 2,
            };
        }
        if (byte == std::byte{0xFF}) {
            return {
                .bytes = {frame_escape, frame_escaped_disconnected},
                .size = 2,
            };
        }
        return {
            .bytes = {byte, {}},
            .size = 1,
        };
    }

    enum class frame_event_kind : unsigned char {
        none,
        payload,
        complete,
        malformed,
        integrity_error,
    };

    struct frame_event {
        frame_event_kind kind = frame_event_kind::none;
        std::byte byte{};
    };

    struct frame_encoder {
        [[nodiscard]] constexpr bool start() noexcept {
            if (active()) {
                return false;
            }
            m_phase = phase::opening;
            m_crc = frame_crc_initial;
            m_pendingSize = 0;
            m_pendingIndex = 0;
            return true;
        }

        [[nodiscard]] constexpr bool write(const std::byte payload) noexcept {
            if (m_phase != phase::payload || has_pending()) {
                return false;
            }
            m_crc = frame_crc_update(m_crc, payload);
            queue_escaped(payload);
            return true;
        }

        [[nodiscard]] constexpr bool finish() noexcept {
            if (m_phase != phase::payload || has_pending()) {
                return false;
            }
            m_phase = phase::crc_low;
            return true;
        }

        [[nodiscard]] constexpr std::optional<std::byte> next() noexcept {
            if (has_pending()) {
                return pop_pending();
            }

            switch (m_phase) {
                case phase::idle:
                case phase::payload: return std::nullopt;
                case phase::opening: m_phase = phase::payload; return frame_end;
                case phase::crc_low:
                    queue_escaped(std::byte{static_cast<unsigned char>(m_crc)});
                    m_phase = phase::crc_high;
                    return pop_pending();
                case phase::crc_high:
                    queue_escaped(std::byte{static_cast<unsigned char>(m_crc >> 8u)});
                    m_phase = phase::closing;
                    return pop_pending();
                case phase::closing: m_phase = phase::idle; return frame_end;
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr bool needs_payload() const noexcept {
            return m_phase == phase::payload && !has_pending();
        }

        [[nodiscard]] constexpr bool active() const noexcept { return m_phase != phase::idle; }

        constexpr void reset() noexcept {
            m_phase = phase::idle;
            m_crc = frame_crc_initial;
            m_pendingSize = 0;
            m_pendingIndex = 0;
        }

    private:
        enum class phase : unsigned char {
            idle,
            opening,
            payload,
            crc_low,
            crc_high,
            closing,
        };

        [[nodiscard]] constexpr bool has_pending() const noexcept { return m_pendingIndex < m_pendingSize; }

        constexpr void queue_escaped(const std::byte byte) noexcept {
            const auto escaped = escape_frame_byte(byte);
            m_pending = escaped.bytes;
            m_pendingSize = escaped.size;
            m_pendingIndex = 0;
        }

        [[nodiscard]] constexpr std::byte pop_pending() noexcept { return m_pending[m_pendingIndex++]; }

        std::array<std::byte, 2> m_pending{};
        std::uint16_t m_crc = frame_crc_initial;
        std::size_t m_pendingSize{};
        std::size_t m_pendingIndex{};
        phase m_phase = phase::idle;
    };

    struct frame_parser {
        [[nodiscard]] constexpr frame_event push(const std::byte wireByte) noexcept {
            if (wireByte == frame_idle) {
                if (m_escaped) {
                    m_escaped = false;
                    m_failed = true;
                }
                return {};
            }

            if (wireByte == frame_end) {
                return finish_frame();
            }

            if (!m_synchronized || m_failed) {
                return {};
            }

            if (m_escaped) {
                m_escaped = false;
                if (wireByte == frame_escaped_end) {
                    return push_decoded(frame_end);
                }
                if (wireByte == frame_escaped_escape) {
                    return push_decoded(frame_escape);
                }
                if (wireByte == frame_escaped_idle) {
                    return push_decoded(frame_idle);
                }
                if (wireByte == frame_escaped_disconnected) {
                    return push_decoded(std::byte{0xFF});
                }
                m_failed = true;
                return {};
            }

            if (wireByte == frame_escape) {
                m_escaped = true;
                return {};
            }

            return push_decoded(wireByte);
        }

        constexpr void reset() noexcept {
            m_synchronized = false;
            reset_frame();
        }

        [[nodiscard]] constexpr bool synchronized() const noexcept { return m_synchronized; }

    private:
        [[nodiscard]] constexpr frame_event push_decoded(const std::byte byte) noexcept {
            ++m_size;
            if (m_size == 1) {
                m_tail[0] = byte;
                return {};
            }
            if (m_size == 2) {
                m_tail[1] = byte;
                return {};
            }

            const std::byte payload = m_tail[0];
            m_tail[0] = m_tail[1];
            m_tail[1] = byte;
            m_crc = frame_crc_update(m_crc, payload);
            return {.kind = frame_event_kind::payload, .byte = payload};
        }

        [[nodiscard]] constexpr frame_event finish_frame() noexcept {
            if (!m_synchronized) {
                m_synchronized = true;
                reset_frame();
                return {};
            }

            if (m_size == 0 && !m_escaped && !m_failed) {
                return {};
            }

            frame_event result{};
            if (m_failed || m_escaped || m_size < 2) {
                result.kind = frame_event_kind::malformed;
            } else {
                const auto received = static_cast<std::uint16_t>(
                    std::to_integer<unsigned char>(m_tail[0]) |
                    (static_cast<std::uint16_t>(std::to_integer<unsigned char>(m_tail[1])) << 8u));
                result.kind = received == m_crc ? frame_event_kind::complete : frame_event_kind::integrity_error;
            }
            reset_frame();
            return result;
        }

        constexpr void reset_frame() noexcept {
            m_tail = {};
            m_crc = frame_crc_initial;
            m_size = 0;
            m_escaped = false;
            m_failed = false;
        }

        std::array<std::byte, 2> m_tail{};
        std::uint16_t m_crc = frame_crc_initial;
        std::size_t m_size{};
        bool m_synchronized{};
        bool m_escaped{};
        bool m_failed{};
    };

} // namespace gba::bits::link
