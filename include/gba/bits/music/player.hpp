/// @file bits/music/player.hpp
/// @brief Compile-time-specialized music player - events become direct ARM code.
///
/// The player is templated on a `static constexpr compiled_music&` NTTP,
/// making every event a compile-time constant. Each event's fire logic
/// is specialized via `if constexpr` into direct register writes with
/// immediate values - zero interpretation, zero dispatch overhead.
///
/// Dispatch strategy:
///   1. `operator()` computes the current time and enters `dispatch_from()`.
///   2. `dispatch_from()` uses a generated `switch(m_timepointIdx)` to jump
///      directly to the correct depth in the `dispatch_timepoint<I>` chain,
///      skipping all already-processed timepoints in O(1).
///   3. Each `dispatch_timepoint<I>` checks one compile-time-known batch
///      time, fires events if due, then tail-calls `<I+1>`.
///   4. With `-O3`, GCC applies tail-call optimisation to the recursive
///      chain, producing a sequence of conditional branches to inlined
///      register-write stubs.
#pragma once

#include <gba/bits/music/compiler.hpp>
#include <gba/peripherals>

namespace gba::music {

    /// @brief Compile-time-specialized music player.
    ///
    /// Templated on a `static constexpr compiled_music` reference (NTTP).
    /// Every event field is a compile-time constant - the optimizer compiles
    /// playback into direct ARM register writes with immediates.
    ///
    /// Usage:
    /// @code{.cpp}
    /// static constexpr auto music = compile<120_bpm>(note("c4 e4 g4 c5"));
    /// auto player = music_player<music>{};
    /// while (player()) { gba::VBlankIntrWait(); }
    /// @endcode
    ///
    /// @tparam Music Reference to a `static constexpr compiled_music`.
    template<const compiled_music& Music>
    struct music_player {
        /// @brief Current frame counter (incremented each `operator()` call).
        std::uint32_t m_frame{};
        /// @brief Index of the next timepoint to process.
        std::uint16_t m_timepointIdx{};
        /// @brief True once all events have fired and the piece has ended.
        bool m_finished{};

        // -- Current instrument state -------------------------------------
        // Runtime state updated by `instrument_change` events. `note_on`
        // re-applies these before triggering so notes after rests are audible.

        /// @brief Current SQ1 instrument (sweep + duty + envelope).
        sq1_instrument m_sq1Inst{};
        /// @brief Current SQ2 instrument (duty + envelope).
        sq2_instrument m_sq2Inst{};
        /// @brief Current WAV instrument (volume + waveform data).
        wav_instrument m_wavInst{};
        /// @brief Current NOISE instrument (envelope).
        noise_instrument m_noiseInst{};

        /// @brief Advance one frame and fire all pending events.
        ///
        /// Call once per VBlank. Fires every event whose `time_num` has been
        /// reached, then increments the frame counter. Returns `false` when
        /// the piece is finished (non-looping) or if already finished.
        ///
        /// @return `true` if playback continues, `false` if done.
        bool operator()() {
            if (m_finished) [[unlikely]]
                return false;

            auto currentTime = static_cast<std::int64_t>(m_frame) * Music.time_den;

            // Dispatch due same-time batches via the template chain.
            dispatch_from(currentTime);

            ++m_frame;

            // Check if we've passed the end
            auto endTime = static_cast<std::int64_t>(m_frame) * Music.time_den;

            if (m_timepointIdx >= Music.timepoint_count && endTime >= Music.total_time_num) {
                if constexpr (Music.looping) {
                    m_frame = 0;
                    m_timepointIdx = 0;
                    return true;
                } else {
                    m_finished = true;
                    return false;
                }
            }

            return true;
        }

        /// @brief Reset the player to the beginning.
        void reset() {
            m_frame = 0;
            m_timepointIdx = 0;
            m_finished = false;
        }

    private:
        // -- Switch-based dispatch entry ----------------------------------
        //
        // The switch generates a jump table that enters the template chain
        // at the exact depth matching m_timepointIdx, eliminating O(n)
        // skip checks for already-processed timepoints.

        void dispatch_from(std::int64_t currentTime) { dispatch_timepoint<0>(currentTime); }

        // -- Template chain -----------------------------------------------
        //
        // Each dispatch_timepoint<I> checks one compile-time-known batch
        // time, fires events if due, then tail-calls dispatch_timepoint<I+1>.
        // With -O3, GCC compiles this into compare-and-branch to inlined
        // register writes.

        /// @brief Process timepoint `I` and continue to `I+1`.
        /// @tparam I Compile-time timepoint index.
        template<std::size_t I>
        void dispatch_timepoint(std::int64_t currentTime) {
            if constexpr (I < Music.timepoint_count) {
                if (I < m_timepointIdx) return dispatch_timepoint<I + 1>(currentTime);

                constexpr auto tp = Music.timepoints[I];

                if (tp.time_num > currentTime) return; // Not yet due - done for this frame.

                fire_batch<static_cast<std::size_t>(tp.event_begin), static_cast<std::size_t>(tp.event_end)>();
                m_timepointIdx++;
                return dispatch_timepoint<I + 1>(currentTime);
            }
        }

        /// @brief Fire all events in the batch [Begin, End).
        /// @tparam Begin First event index (inclusive).
        /// @tparam End   Past-the-end event index.
        template<std::size_t Begin, std::size_t End>
        [[gnu::always_inline]] void fire_batch() {
            if constexpr (Begin < End) {
                constexpr auto& ev = Music.events[Begin];
                fire<ev>();
                return fire_batch<Begin + 1, End>();
            }
        }

        // -- Compile-time-specialized event firing ------------------------
        //
        // Every field of `Ev` is a compile-time constant. `if constexpr`
        // eliminates dead branches, leaving only the specific register
        // write for this event - with rate/envelope/etc as ARM immediates.

        /// @brief Route event to the appropriate handler.
        /// @tparam Ev Compile-time event constant.
        template<auto Ev>
        [[gnu::always_inline]] void fire() {
            if constexpr (Ev.type == event_type::note_on) {
                fire_note_on<Ev>();
            } else if constexpr (Ev.type == event_type::note_off) {
                fire_note_off<Ev>();
            } else if constexpr (Ev.type == event_type::instrument_change) {
                fire_instrument_change<Ev>();
            }
        }

        /// @brief Trigger a note - write envelope, frequency, and trigger bit.
        ///
        /// For SQ1/SQ2: zeros the envelope first for clean articulation of
        /// repeated notes, then writes the instrument envelope and triggers.
        /// For WAV: sets bank mode and triggers.
        /// For NOISE: writes per-drum envelope + frequency params and triggers.
        ///
        /// @tparam Ev Compile-time `note_on` event.
        template<auto Ev>
        [[gnu::always_inline]] void fire_note_on() {
            if constexpr (Ev.chan == channel::sq1) {
                // Zero envelope for clean articulation, then reconfigure and trigger.
                gba::reg_sound1cnt_h = {.env_volume = 0};
                gba::reg_sound1cnt_l = {.shift = m_sq1Inst.sweep_shift,
                                        .direction = m_sq1Inst.sweep_direction,
                                        .time = m_sq1Inst.sweep_time};
                gba::reg_sound1cnt_h = {.duty = m_sq1Inst.duty,
                                        .env_step = m_sq1Inst.env_step,
                                        .env_direction = m_sq1Inst.env_direction,
                                        .env_volume = m_sq1Inst.env_volume};
                gba::reg_sound1cnt_x = {.rate = Ev.rate, .trigger = true};
            } else if constexpr (Ev.chan == channel::sq2) {
                gba::reg_sound2cnt_l = {.env_volume = 0};
                gba::reg_sound2cnt_l = {.duty = m_sq2Inst.duty,
                                        .env_step = m_sq2Inst.env_step,
                                        .env_direction = m_sq2Inst.env_direction,
                                        .env_volume = m_sq2Inst.env_volume};
                gba::reg_sound2cnt_h = {.rate = Ev.rate, .trigger = true};
            } else if constexpr (Ev.chan == channel::wav) {
                gba::reg_sound3cnt_l = {.bank_mode = true, .enable = true};
                gba::reg_sound3cnt_x = {.rate = Ev.rate, .trigger = true};
            } else if constexpr (Ev.chan == channel::noise) {
                gba::reg_sound4cnt_l = {.env_step = Ev.noise_env_step,
                                        .env_direction = Ev.noise_env_direction,
                                        .env_volume = Ev.noise_env_volume};
                gba::reg_sound4cnt_h = {
                    .div_ratio = Ev.noise_div_ratio, .width = Ev.noise_width, .shift = Ev.noise_shift, .trigger = true};
            }
        }

        /// @brief Silence a channel - write zero envelope and trigger.
        /// @tparam Ev Compile-time `note_off` event.
        template<auto Ev>
        [[gnu::always_inline]] static void fire_note_off() {
            if constexpr (Ev.chan == channel::sq1) {
                gba::reg_sound1cnt_h = {.env_volume = 0};
                gba::reg_sound1cnt_x = {.trigger = true};
            } else if constexpr (Ev.chan == channel::sq2) {
                gba::reg_sound2cnt_l = {.env_volume = 0};
                gba::reg_sound2cnt_h = {.trigger = true};
            } else if constexpr (Ev.chan == channel::wav) {
                gba::reg_sound3cnt_l = {.enable = false};
            } else if constexpr (Ev.chan == channel::noise) {
                gba::reg_sound4cnt_l = {.env_volume = 0};
                gba::reg_sound4cnt_h = {.trigger = true};
            }
        }

        /// @brief Apply instrument configuration - write registers and persist state.
        ///
        /// For WAV: uploads the 64-sample waveform to both wave RAM banks
        /// using the bank-select technique (1x64 mode).
        ///
        /// @tparam Ev Compile-time `instrument_change` event.
        template<auto Ev>
        [[gnu::always_inline]] void fire_instrument_change() {
            // Persist instruments so note_on can restore them.
            m_sq1Inst = Ev.sq1_inst;
            m_sq2Inst = Ev.sq2_inst;
            m_wavInst = Music.wav_instruments[Ev.wav_inst_idx];
            m_noiseInst = Ev.noise_inst;

            if constexpr (Ev.chan == channel::sq1) {
                gba::reg_sound1cnt_l = {.shift = Ev.sq1_inst.sweep_shift,
                                        .direction = Ev.sq1_inst.sweep_direction,
                                        .time = Ev.sq1_inst.sweep_time};
                gba::reg_sound1cnt_h = {.duty = Ev.sq1_inst.duty,
                                        .env_step = Ev.sq1_inst.env_step,
                                        .env_direction = Ev.sq1_inst.env_direction,
                                        .env_volume = Ev.sq1_inst.env_volume};
            } else if constexpr (Ev.chan == channel::sq2) {
                gba::reg_sound2cnt_l = {.duty = Ev.sq2_inst.duty,
                                        .env_step = Ev.sq2_inst.env_step,
                                        .env_direction = Ev.sq2_inst.env_direction,
                                        .env_volume = Ev.sq2_inst.env_volume};
            } else if constexpr (Ev.chan == channel::wav) {
                // 1x64 mode: both banks form a single 64-sample waveform.
                constexpr auto wavInst = Music.wav_instruments[Ev.wav_inst_idx];
                gba::reg_sound3cnt_l = {.bank_mode = true, .enable = false};
                gba::reg_sound3cnt_h = {.volume = wavInst.volume, .force_75 = wavInst.force_75};
                // Write bank 0 (samples 0-31)
                gba::reg_sound3cnt_l = {.bank_mode = true, .bank_select = false, .enable = false};
                gba::reg_wave_ram[0] = wavInst.wave_data[0];
                gba::reg_wave_ram[1] = wavInst.wave_data[1];
                gba::reg_wave_ram[2] = wavInst.wave_data[2];
                gba::reg_wave_ram[3] = wavInst.wave_data[3];
                // Write bank 1 (samples 32-63)
                gba::reg_sound3cnt_l = {.bank_mode = true, .bank_select = true, .enable = false};
                gba::reg_wave_ram[0] = wavInst.wave_data[4];
                gba::reg_wave_ram[1] = wavInst.wave_data[5];
                gba::reg_wave_ram[2] = wavInst.wave_data[6];
                gba::reg_wave_ram[3] = wavInst.wave_data[7];
                // Enable playback (bank_mode=true for 1x64)
                gba::reg_sound3cnt_l = {.bank_mode = true, .enable = true};
            } else if constexpr (Ev.chan == channel::noise) {
                gba::reg_sound4cnt_l = {.env_step = Ev.noise_inst.env_step,
                                        .env_direction = Ev.noise_inst.env_direction,
                                        .env_volume = Ev.noise_inst.env_volume};
            }
        }
    };

} // namespace gba::music
