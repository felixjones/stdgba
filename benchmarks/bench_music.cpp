/// @file benchmarks/bench_music.cpp
/// @brief Benchmark measuring music_player per-frame dispatch cost.
///
/// Measures three scenarios on GBA hardware via cycle-accurate timers:
///   1. Skip-frame: player tick when no events are due (dispatch overhead).
///   2. Single note_on: one event fires (register write cost).
///   3. 4-channel batch: all 4 PSG channels fire simultaneously.
///
/// Uses gba::benchmark helpers. Run in mgba-headless to see output.

#include <gba/bios>
#include <gba/interrupt>
#include <gba/music>
#include <gba/peripherals>

#include "bench.hpp"

using namespace gba::music;
using namespace gba::music::literals;

// -- Compiled patterns (consteval) --------------------------------------------

// 1. Simple 4-note melody - measures single-channel dispatch.
static constexpr auto simple_music = compile<120_bpm>(note("c4 e4 g4 c5"));

// 2. 4-channel stack - measures full-batch dispatch (all channels active).
static constexpr auto full_music =
    compile<120_bpm>(stack(note("c4 e4 g4 c5").channel(channel::sq1), note("c3 e3 g3 c4").channel(channel::sq2),
                           note("c3 g3 c3 g3").channel(channel::wav, waves::triangle), s("bd sd hh sd")));

// Volatile sinks to prevent DCE
volatile bool sink_bool;
volatile std::uint32_t sink_u32;

namespace {

    // -- Measurement wrappers -----------------------------------------------------

    // Advance player by N frames, skipping past initial events.
    template<const compiled_music& M>
    void advance_to_skip_frame(music_player<M>& player, int frames) {
        for (int i = 0; i < frames; ++i) sink_bool = player();
    }

    // Measure a single player() call.
    template<const compiled_music& M>
    void tick_player(music_player<M>& player) {
        sink_bool = player();
    }

} // namespace

int main() {
    // IRQ + PSG setup (player writes to sound registers)
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;
    gba::reg_soundcnt_x = {.master_enable = true};
    gba::reg_soundcnt_l = {
        .volume_right = 7,
        .volume_left = 7,
        .enable_1_right = true,
        .enable_2_right = true,
        .enable_3_right = true,
        .enable_4_right = true,
        .enable_1_left = true,
        .enable_2_left = true,
        .enable_3_left = true,
        .enable_4_left = true,
    };
    gba::reg_soundcnt_h = {.psg_volume = 2};

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "=== music_player benchmark (cycles) ===");
        bench::log_printf(gba::log::level::info, "");
    });

    // -- Pattern metadata -------------------------------------------------
    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "--- Pattern metadata ---");
        bench::log_printf(gba::log::level::info, "  simple: %d events, %d timepoints", simple_music.event_count,
                          simple_music.timepoint_count);
        bench::log_printf(gba::log::level::info, "  full:   %d events, %d timepoints", full_music.event_count,
                          full_music.timepoint_count);
        bench::log_printf(gba::log::level::info, "");
    });

    // -- Skip-frame cost (no events due) ----------------------------------
    {
        bench::with_logger([] {
            bench::log_printf(gba::log::level::info, "--- Skip-frame (no events due) ---");
            bench::log_printf(gba::log::level::info, "  %-24s  cycles", "Case");
        });

        // Simple: advance past the first batch, then measure a skip frame
        {
            music_player<simple_music> player{};
            advance_to_skip_frame(player, 2); // past initial events
            auto cycles = bench::measure_avg(64, tick_player<simple_music>, player);
            bench::with_logger(
                [&] { bench::log_printf(gba::log::level::info, "  %-24s  %u", "simple (1ch)", cycles); });
        }

        // Full: advance past the first batch, then measure a skip frame
        {
            music_player<full_music> player{};
            advance_to_skip_frame(player, 2);
            auto cycles = bench::measure_avg(64, tick_player<full_music>, player);
            bench::with_logger([&] { bench::log_printf(gba::log::level::info, "  %-24s  %u", "full (4ch)", cycles); });
        }
    }

    // -- Event-firing cost (first frame with events) ----------------------
    {
        bench::with_logger([] {
            bench::log_printf(gba::log::level::info, "");
            bench::log_printf(gba::log::level::info, "--- First-frame dispatch (events fire) ---");
            bench::log_printf(gba::log::level::info, "  %-24s  cycles", "Case");
        });

        // Simple: measure frame 0 (instrument_change + first note_on)
        {
            music_player<simple_music> player{};
            auto cycles = bench::measure(tick_player<simple_music>, player);
            bench::with_logger(
                [&] { bench::log_printf(gba::log::level::info, "  %-24s  %u", "simple (1ch)", cycles); });
        }

        // Full: measure frame 0 (4x instrument_change + 4x note_on)
        {
            music_player<full_music> player{};
            auto cycles = bench::measure(tick_player<full_music>, player);
            bench::with_logger([&] { bench::log_printf(gba::log::level::info, "  %-24s  %u", "full (4ch)", cycles); });
        }
    }

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
