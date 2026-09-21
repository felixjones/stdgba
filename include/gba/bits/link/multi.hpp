/// @file multi.hpp
/// @brief Typed Multi-Player mode link cable communication for 2-4 units.
#pragma once

#include <gba/bits/link/irq_snapshot_queue.hpp>
#include <gba/bits/link/typed_stream.hpp>
#include <gba/peripherals>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace gba::bits::link {

    /// @brief One of the four Multi-Player link endpoints.
    enum class multi_player : unsigned short {
        player_0,
        player_1,
        player_2,
        player_3,
    };

    inline constexpr std::array multi_players{
        multi_player::player_0,
        multi_player::player_1,
        multi_player::player_2,
        multi_player::player_3,
    };

    [[nodiscard]] constexpr std::size_t multi_player_index(const multi_player player) noexcept {
        return static_cast<std::size_t>(player);
    }

    template<const auto& Codec, std::size_t Capacity>
    struct link_multi {
        using stream_type = typed_stream<Codec, Capacity, 4>;
        using value_type = stream_type::value_type;
        using error_handler = stream_type::error_handler;

        link_multi() noexcept = default;

        explicit link_multi(const sio_baud baud) noexcept { configure(baud); }

        static void configure(const sio_baud baud = sio_baud_115200) noexcept {
            reg_rcnt = {};
            reg_siocnt_multi = {.baud = baud, .irq_enable = true};
        }

        [[nodiscard]] static bool is_parent() noexcept { return player() == multi_player::player_0; }

        [[nodiscard]] static bool ready() noexcept { return static_cast<sio_multi_control>(reg_siocnt_multi).sd_state; }

        [[nodiscard]] static multi_player player() noexcept {
            return static_cast<multi_player>(static_cast<sio_multi_control>(reg_siocnt_multi).id);
        }

        [[nodiscard]] static bool hardware_error() noexcept {
            return static_cast<sio_multi_control>(reg_siocnt_multi).error;
        }

        bool send(const value_type& value) { return m_stream.send(value); }

        [[nodiscard]] bool sending() const noexcept { return m_stream.sending(); }

        [[nodiscard]] std::optional<value_type> read(const multi_player player) {
            return m_stream.read(multi_player_index(player));
        }

        [[nodiscard]] std::size_t pending_wire(const multi_player player) const noexcept {
            return m_stream.pending_wire(multi_player_index(player));
        }

        [[nodiscard]] link_error errors(const multi_player player) const noexcept {
            return m_stream.errors(multi_player_index(player));
        }

        [[nodiscard]] bool has_error(const multi_player player, const link_error error) const noexcept {
            return m_stream.has_error(multi_player_index(player), error);
        }

        void clear_error(const multi_player player, const link_error error) noexcept {
            m_stream.clear_error(multi_player_index(player), error);
        }

        void clear_errors(const multi_player player) noexcept { m_stream.clear_errors(multi_player_index(player)); }

        void reset_peer(const multi_player player) { m_stream.reset_peer(multi_player_index(player)); }

        void on_error(error_handler handler) noexcept { m_stream.on_error(std::move(handler)); }

        [[nodiscard]] bool busy() const noexcept { return m_busy; }

        void irq() {
            completed_round round{
                .player = player(),
                .hardware_error = hardware_error(),
            };
            for (std::size_t player = 0; player < 4; ++player) {
                round.words[player] = reg_siomulti[player];
            }
            static_cast<void>(m_completedRounds.push_from_irq(round));
            reg_siomlt_send = m_stream.template next_wire_word<unsigned short>();
            m_wordStaged = true;
            m_busy = false;
        }

        void poll() {
            while (const auto round = m_completedRounds.pop()) {
                complete_round(*round);
            }
            if (m_completedRounds.consume_overflow()) {
                for (std::size_t player = 0; player < 4; ++player) {
                    if (m_connected[player]) {
                        m_stream.report_error(player, link_error::receive_overflow);
                    }
                }
            }

            if (!ready()) {
                if (m_wasReady) {
                    reset_topology(true, *m_lastPlayer);
                }
                m_wasReady = false;
                m_stream.dispatch_errors();
                return;
            }

            const multi_player currentPlayer = player();
            if (!m_wasReady || currentPlayer != m_lastPlayer) {
                reset_topology(m_lastPlayer.has_value(), currentPlayer);
                m_lastPlayer = currentPlayer;
            }
            m_wasReady = true;

            if (!m_busy) {
                begin_round();
            }
            m_stream.dispatch_errors();
        }

        [[nodiscard]] std::array<bool, 4> connected() const noexcept { return m_connected; }

        [[nodiscard]] std::size_t connected_count() const noexcept {
            std::size_t count = 0;
            for (const bool connected : m_connected) {
                count += connected ? 1 : 0;
            }
            return count;
        }

    private:
        struct completed_round {
            std::array<unsigned short, 4> words{};
            multi_player player{};
            bool hardware_error{};
        };

        void begin_round() {
            const critical_section section;
            m_busy = true;
            if (!m_wordStaged) {
                reg_siomlt_send = m_stream.template next_wire_word<unsigned short>();
            }
            m_wordStaged = false;
            if (is_parent()) {
                auto cnt = static_cast<sio_multi_control>(reg_siocnt_multi);
                cnt.start = true;
                reg_siocnt_multi = cnt;
            }
        }

        void complete_round(const completed_round& round) {
            const multi_player selfPlayer = round.player;
            for (std::size_t player = 0; player < 4; ++player) {
                const unsigned short word = round.words[player];
                const bool nowConnected = player == multi_player_index(selfPlayer) || word != 0xFFFF;
                const bool changed = nowConnected != m_connected[player];
                m_connected[player] = nowConnected;
                if (player != multi_player_index(selfPlayer) && changed) {
                    m_stream.reset_peer(player, true);
                }
                if (player != multi_player_index(selfPlayer) && nowConnected) {
                    m_stream.receive_word(player, word);
                }
            }
            if (round.hardware_error) {
                for (std::size_t player = 0; player < 4; ++player) {
                    if (player != multi_player_index(selfPlayer) && m_connected[player]) {
                        m_stream.report_error(player, link_error::hardware_failure);
                        m_stream.reset_peer(player);
                    }
                }
            }
        }

        void reset_topology(const bool reportChange, const multi_player selfPlayer) {
            const critical_section section;
            m_stream.reset_send();
            for (std::size_t player = 0; player < 4; ++player) {
                m_stream.reset_peer(player, reportChange && player != multi_player_index(selfPlayer));
            }
            m_connected = {};
            m_busy = false;
            m_wordStaged = false;
            m_completedRounds.clear();
        }

        stream_type m_stream{};
        std::array<bool, 4> m_connected{};
        irq_snapshot_queue<completed_round, 4> m_completedRounds{};
        std::optional<multi_player> m_lastPlayer{};
        volatile bool m_busy{};
        volatile bool m_wordStaged{};
        bool m_wasReady{};
    };

} // namespace gba::bits::link
