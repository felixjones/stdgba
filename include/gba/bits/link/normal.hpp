/// @file normal.hpp
/// @brief Typed Normal mode link cable communication for two units.
#pragma once

#include <gba/bits/link/irq_snapshot_queue.hpp>
#include <gba/bits/link/typed_stream.hpp>
#include <gba/peripherals>

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace gba::bits::link {

    template<const auto& Codec, std::size_t Capacity, typename Word>
        requires(std::is_unsigned_v<Word> && (sizeof(Word) == 1 || sizeof(Word) == 4))
    struct link_normal {
        using stream_type = typed_stream<Codec, Capacity, 1>;
        using value_type = stream_type::value_type;
        using error_handler = stream_type::error_handler;

        static constexpr sio_mode mode = sizeof(Word) == 1 ? sio_mode_normal_8bit : sio_mode_normal_32bit;

        link_normal() noexcept = default;

        explicit link_normal(const bool parent) noexcept { configure(parent); }

        static void configure(const bool parent) noexcept {
            reg_rcnt = {};
            reg_siocnt = {
                .shift_clock = static_cast<unsigned short>(parent),
                .internal_clock = 0u,
                .mode = mode,
                .irq_enable = true,
            };
        }

        [[nodiscard]] bool send(const value_type& value) { return m_stream.send(value); }

        [[nodiscard]] bool sending() const noexcept { return m_stream.sending(); }

        [[nodiscard]] std::optional<value_type> read() { return m_stream.read(0); }

        [[nodiscard]] std::size_t pending_wire() const noexcept { return m_stream.pending_wire(0); }

        [[nodiscard]] link_error errors() const noexcept { return m_stream.errors(0); }

        [[nodiscard]] bool has_error(const link_error error) const noexcept { return m_stream.has_error(0, error); }

        void clear_error(const link_error error) noexcept { m_stream.clear_error(0, error); }

        void clear_errors() noexcept { m_stream.clear_errors(0); }

        void reset_peer() { m_stream.reset_peer(0); }

        void reset() {
            const critical_section section;
            auto cnt = static_cast<sio_control>(reg_siocnt);
            cnt.start = false;
            reg_siocnt = cnt;
            m_stream.reset_send();
            m_stream.reset_peer(0);
            m_busy = false;
            m_wordStaged = false;
            m_completedWords.clear();
        }

        void on_error(error_handler handler) noexcept { m_stream.on_error(std::move(handler)); }

        [[nodiscard]] bool busy() const noexcept { return m_busy; }

        void irq() {
            static_cast<void>(m_completedWords.push_from_irq(read_word()));
            write_word(m_stream.template next_wire_word<Word>());
            m_wordStaged = true;
            m_busy = false;
        }

        void poll() {
            while (const auto word = m_completedWords.pop()) {
                m_stream.receive_word(0, *word);
            }
            if (m_completedWords.consume_overflow()) {
                m_stream.report_error(0, link_error::receive_overflow);
            }
            if (!m_busy) {
                begin_round();
            }
            m_stream.dispatch_errors();
        }

    private:
        void begin_round() {
            const critical_section section;
            m_busy = true;
            if (!m_wordStaged) {
                write_word(m_stream.template next_wire_word<Word>());
            }
            m_wordStaged = false;
            auto cnt = static_cast<sio_control>(reg_siocnt);
            cnt.start = true;
            reg_siocnt = cnt;
        }

        [[nodiscard]] static Word read_word() noexcept {
            if constexpr (sizeof(Word) == 1) {
                return static_cast<Word>(static_cast<unsigned char>(reg_siodata8));
            } else {
                return static_cast<Word>(static_cast<unsigned int>(reg_siodata32));
            }
        }

        static void write_word(const Word word) noexcept {
            if constexpr (sizeof(Word) == 1) {
                reg_siodata8 = static_cast<unsigned char>(word);
            } else {
                reg_siodata32 = static_cast<unsigned int>(word);
            }
        }

        stream_type m_stream{};
        irq_snapshot_queue<Word, 4> m_completedWords{};
        volatile bool m_busy{};
        volatile bool m_wordStaged{};
    };

} // namespace gba::bits::link
