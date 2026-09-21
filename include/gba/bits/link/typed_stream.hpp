/// @file typed_stream.hpp
/// @brief Codec-driven framed streams shared by GBA link modes.
#pragma once

#include <gba/bits/link/byte_ring.hpp>
#include <gba/bits/link/error.hpp>
#include <gba/bits/link/frame.hpp>
#include <gba/bits/link/wire_word.hpp>
#include <gba/codec>
#include <gba/critical_section>
#include <gba/functional>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace gba::bits::link {

    template<const auto& Codec, std::size_t Capacity, std::size_t PeerCount>
        requires gba::codec::Codec<std::remove_cvref_t<decltype(Codec)>>
    struct typed_stream {
        using send_index_type = std::conditional_t<
            (Capacity <= std::numeric_limits<std::uint8_t>::max()), std::uint8_t,
            std::conditional_t<(Capacity <= std::numeric_limits<std::uint16_t>::max()), std::uint16_t, std::size_t>>;

        using codec_type = std::remove_cvref_t<decltype(Codec)>;
        using value_type = codec_type::value_type;
        using decoder_type = decltype(Codec.make_decoder());
        using error_handler = gba::handler<std::size_t, link_error>;

        [[nodiscard]] bool send(const value_type& value) {
            if (sending()) {
                return false;
            }

            auto encoder = Codec.make_encoder(value);
            frame_encoder frame;
            static_cast<void>(frame.start());
            std::size_t size = 0;
            while (frame.active()) {
                if (frame.needs_payload()) {
                    if (const auto payload = encoder.next()) {
                        static_cast<void>(frame.write(*payload));
                    } else {
                        static_cast<void>(frame.finish());
                    }
                }
                if (const auto wireByte = frame.next()) {
                    if (size == Capacity) {
                        return false;
                    }
                    m_sendWire[size++] = *wireByte;
                }
            }

            const gba::critical_section section;
            m_sendIndex = 0;
            m_sendSize = static_cast<send_index_type>(size);
            return true;
        }

        [[nodiscard]] bool sending() const noexcept { return m_sendIndex < m_sendSize; }

        [[nodiscard]] std::optional<std::byte> next_wire_byte() {
            if (!sending()) {
                return std::nullopt;
            }
            const send_index_type index = m_sendIndex;
            m_sendIndex = static_cast<send_index_type>(index + 1);
            return m_sendWire[index];
        }

        template<typename Word>
        [[nodiscard]] Word next_wire_word() {
            return pack_wire_word<Word>([this] { return next_wire_byte(); });
        }

        template<typename Word>
        void receive_word(const std::size_t peer, const Word word) {
            if (peer >= PeerCount) {
                return;
            }
            for (const std::byte byte : unpack_wire_word(word)) {
                if (byte != frame_idle && byte != std::byte{0xFF} && !m_peers[peer].wire.push(byte)) {
                    report(peer, link_error::receive_overflow);
                }
            }
        }

        [[nodiscard]] std::optional<value_type> read(const std::size_t peer) {
            if (peer >= PeerCount) {
                return std::nullopt;
            }

            auto& state = m_peers[peer];
            std::byte wireByte{};
            while (state.wire.pop(wireByte)) {
                const frame_event event = state.frame.push(wireByte);
                switch (event.kind) {
                    case frame_event_kind::none: break;
                    case frame_event_kind::payload: consume_payload(peer, event.byte); break;
                    case frame_event_kind::complete:
                        if (!state.decoder_failed && state.decoder.state() == codec::step::done) {
                            auto value = state.decoder.value();
                            state.reset_decoder();
                            return value;
                        }
                        report(peer, link_error::decode_failure);
                        state.reset_decoder();
                        break;
                    case frame_event_kind::malformed:
                        report(peer, link_error::malformed_frame);
                        state.reset_decoder();
                        break;
                    case frame_event_kind::integrity_error:
                        report(peer, link_error::integrity_failure);
                        state.reset_decoder();
                        break;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::size_t pending_wire(const std::size_t peer) const noexcept {
            return peer < PeerCount ? m_peers[peer].wire.size() : 0;
        }

        [[nodiscard]] link_error errors(const std::size_t peer) const noexcept {
            return peer < PeerCount ? m_peers[peer].errors.value() : link_error::none;
        }

        [[nodiscard]] bool has_error(const std::size_t peer, const link_error error) const noexcept {
            return peer < PeerCount && m_peers[peer].errors.has(error);
        }

        void clear_error(const std::size_t peer, const link_error error) noexcept {
            if (peer < PeerCount) {
                m_peers[peer].errors.clear(error);
                m_peers[peer].pending_errors.clear(error);
            }
        }

        void clear_errors(const std::size_t peer) noexcept {
            if (peer < PeerCount) {
                m_peers[peer].errors.clear();
                m_peers[peer].pending_errors.clear();
            }
        }

        void reset_peer(const std::size_t peer, const bool topologyChanged = false) {
            if (peer >= PeerCount) {
                return;
            }
            m_peers[peer].reset();
            if (topologyChanged) {
                report(peer, link_error::topology_changed);
            }
        }

        void reset_send() noexcept {
            const gba::critical_section section;
            m_sendIndex = 0;
            m_sendSize = 0;
        }

        void report_error(const std::size_t peer, const link_error error) {
            if (peer < PeerCount) {
                report(peer, error);
            }
        }

        void on_error(error_handler handler) noexcept { m_errorHandler = std::move(handler); }

        void dispatch_errors() {
            if (!m_errorHandler) {
                return;
            }
            constexpr std::array error_values{
                link_error::receive_overflow, link_error::malformed_frame,  link_error::integrity_failure,
                link_error::decode_failure,   link_error::hardware_failure, link_error::topology_changed,
            };
            for (std::size_t peer = 0; peer < PeerCount; ++peer) {
                for (const link_error error : error_values) {
                    if (m_peers[peer].pending_errors.has(error)) {
                        m_peers[peer].pending_errors.clear(error);
                        m_errorHandler(peer, error);
                    }
                }
            }
        }

    private:
        struct peer_state {
            byte_ring<Capacity> wire{};
            frame_parser frame{};
            decoder_type decoder = Codec.make_decoder();
            link_error_state errors{};
            link_error_state pending_errors{};
            bool decoder_failed{};

            void reset_decoder() {
                decoder = Codec.make_decoder();
                decoder_failed = false;
            }

            void reset() {
                wire.clear();
                frame.reset();
                reset_decoder();
            }
        };

        void consume_payload(const std::size_t peer, const std::byte byte) {
            auto& state = m_peers[peer];
            if (state.decoder_failed || state.decoder.state() != codec::step::more) {
                state.decoder_failed = true;
                return;
            }
            if (state.decoder.feed(byte) == codec::step::error) {
                state.decoder_failed = true;
            }
        }

        void report(const std::size_t peer, const link_error error) {
            auto& errors = m_peers[peer].errors;
            errors.set(error);
            m_peers[peer].pending_errors.set(error);
        }

        std::array<std::byte, Capacity> m_sendWire{};
        volatile send_index_type m_sendIndex{};
        volatile send_index_type m_sendSize{};
        std::array<peer_state, PeerCount> m_peers{};
        error_handler m_errorHandler;
    };

} // namespace gba::bits::link
