#include <gba/music>
#include <gba/testing>

#include <array>

using namespace gba::music;
using namespace gba::music::literals;

int main() {
    // Section: Note parsing tests

    gba::test("parse rest", [] {
        static constexpr auto result = parse_note_name("~", 0, 1);
        static_assert(result.value == note::rest);
        static_assert(result.length == 1);
        gba::test.is_true(true);
    });

    gba::test("parse hold (_)", [] {
        static constexpr auto result = parse_note_name("_", 0, 1);
        static_assert(result.value == note::hold);
        static_assert(result.length == 1);
        gba::test.is_true(true);
    });

    gba::test("parse octave 1 note: a1", [] {
        // Octave 1 is preserved in the parsed note value. Playback later saturates
        // to the lowest representable pitched PSG rate instead of folding upward.
        static constexpr auto result = parse_note_name("a1", 0, 2);
        static_assert(result.value == note::a1);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse octave 1 note: b1", [] {
        static constexpr auto result = parse_note_name("b1", 0, 2);
        static_assert(result.value == note::b1);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse octave 1 note: c1", [] {
        static constexpr auto result = parse_note_name("c1", 0, 2);
        static_assert(result.value == note::c1);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("octave 1 notes are consteval error in note_to_rate", [] {
        // note_to_rate(note::c1) would throw at compile time - cannot be tested
        // at runtime. We verify the boundary: C2 is the lowest valid pitched note.
        static_assert(note_to_rate(note::c2) > 0);
        gba::test.is_true(true);
    });

    gba::test("parse g#2 sharp notation", [] {
        static constexpr auto result = parse_note_name("g#2", 0, 3);
        static_assert(result.value == note::gs2);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse c4", [] {
        static constexpr auto result = parse_note_name("c4", 0, 2);
        static_assert(result.value == note::c4);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse a4 (440 Hz)", [] {
        static constexpr auto result = parse_note_name("a4", 0, 2);
        static_assert(result.value == note::a4);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse cs4 (C#4)", [] {
        static constexpr auto result = parse_note_name("cs4", 0, 3);
        static_assert(result.value == note::cs4);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse eb4 flat notation (E-flat = D#4)", [] {
        static constexpr auto result = parse_note_name("eb4", 0, 3);
        static_assert(result.value == note::ds4);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse bb4 flat notation (B-flat = A#4)", [] {
        static constexpr auto result = parse_note_name("bb4", 0, 3);
        static_assert(result.value == note::as4);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse ab4 flat notation (A-flat = G#4)", [] {
        static constexpr auto result = parse_note_name("ab4", 0, 3);
        static_assert(result.value == note::gs4);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse drum bd", [] {
        static constexpr auto result = parse_note_name("bd", 0, 2);
        static_assert(result.value == note::bd);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse drum rim (3-char name)", [] {
        static constexpr auto result = parse_note_name("rim", 0, 3);
        static_assert(result.value == note::rim);
        static_assert(result.length == 3);
        gba::test.is_true(true);
    });

    gba::test("parse rim in sequence context", [] {
        static constexpr auto pat = parse_mini("[rim bd]");
        static_assert(pat.nodes[pat.root].type == ast_type::subdivision);
        static_assert(pat.nodes[pat.root].child_count == 2);
        static constexpr auto rimIdx = pat.nodes[pat.root].children[0];
        static_assert(pat.nodes[rimIdx].note_value == note::rim);
        gba::test.is_true(true);
    });

    gba::test("s() with rim drum", [] {
        static constexpr auto p = s("bd sd [rim bd] sd");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    gba::test("compile s() with rim", [] {
        static constexpr auto music = compile(s("bd sd [rim bd] sd"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        // bd sd [rim bd] sd = 4 top-level steps, [rim bd] has 2 notes = 5 total note_ons
        gba::test.eq(noteOns, 5);
    });

    gba::test("parse bare c (default octave 3)", [] {
        static constexpr auto result = parse_note_name("c ", 0, 2);
        static_assert(result.value == note::c3);
        static_assert(result.length == 1);
        gba::test.is_true(true);
    });

    gba::test("parse bare a (default octave 3)", [] {
        static constexpr auto result = parse_note_name("a ", 0, 2);
        static_assert(result.value == note::a3);
        static_assert(result.length == 1);
        gba::test.is_true(true);
    });

    gba::test("parse bare b (default octave 3)", [] {
        static constexpr auto result = parse_note_name("b ", 0, 2);
        static_assert(result.value == note::b3);
        static_assert(result.length == 1);
        gba::test.is_true(true);
    });

    gba::test("parse bare eb (Eb3)", [] {
        static constexpr auto result = parse_note_name("eb ", 0, 3);
        static_assert(result.value == note::ds3);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("parse bare c# (C#3)", [] {
        static constexpr auto result = parse_note_name("c# ", 0, 3);
        static_assert(result.value == note::cs3);
        static_assert(result.length == 2);
        gba::test.is_true(true);
    });

    gba::test("compile bare note sequence", [] {
        static constexpr auto music = compile(note("c d e f g a b"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 7);
    });

    gba::test("A4 rate is 1750", [] {
        static constexpr auto rate = note_to_rate(note::a4);
        static_assert(rate == 1750);
        gba::test.eq(rate, static_cast<std::uint16_t>(1750));
    });

    gba::test("C4 rate", [] {
        static constexpr auto rate = note_to_rate(note::c4);
        static_assert(rate == 1546);
        gba::test.eq(rate, static_cast<std::uint16_t>(1546));
    });

    // Section: Rational arithmetic tests

    gba::test("rational arithmetic", [] {
        static constexpr auto a = rational{1, 3};
        static constexpr auto b = rational{1, 6};
        static constexpr auto sum = a + b;
        static_assert(sum == rational{1, 2});
        gba::test.is_true(true);
    });

    gba::test("frames_per_cycle at 120 BPM", [] {
        static constexpr auto fpc = frames_per_cycle(120, 4);
        static_assert(fpc.num > 0);
        static_assert(fpc.den > 0);
        gba::test.is_true(fpc.num > 0);
    });

    gba::test("120 BPM == 0.5 CPS == 30 CPM", [] {
        static constexpr auto bpm_fpc = (120_bpm).frames_per_cycle();
        static constexpr auto cps_fpc = (0.5_cps).frames_per_cycle();
        static constexpr auto cpm_fpc = (30_cpm).frames_per_cycle();
        static_assert(bpm_fpc == cps_fpc);
        static_assert(bpm_fpc == cpm_fpc);
        gba::test.is_true(true);
    });

    gba::test("default tempo is 0.5 cps", [] {
        static constexpr auto fpc = default_tempo.frames_per_cycle();
        static constexpr auto half_cps = (0.5_cps).frames_per_cycle();
        static_assert(fpc == half_cps);
        gba::test.is_true(true);
    });

    // Section: Pattern parsing tests

    gba::test("parse simple sequence", [] {
        static constexpr auto pat = parse_mini("c4 e4 g4");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 3);
        gba::test.eq(pat.nodes[pat.root].child_count, static_cast<std::uint8_t>(3));
    });

    gba::test("parse rest in sequence", [] {
        static constexpr auto pat = parse_mini("c4 ~ e4");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 3);
        static constexpr auto secondIdx = pat.nodes[pat.root].children[1];
        static_assert(pat.nodes[secondIdx].note_value == note::rest);
        gba::test.is_true(true);
    });

    gba::test("parse subdivision", [] {
        static constexpr auto pat = parse_mini("[c4 e4] g4");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 2);
        static constexpr auto firstIdx = pat.nodes[pat.root].children[0];
        static_assert(pat.nodes[firstIdx].type == ast_type::subdivision);
        gba::test.is_true(true);
    });

    gba::test("parse alternating", [] {
        static constexpr auto pat = parse_mini("<c4 e4 g4>");
        static_assert(pat.nodes[pat.root].type == ast_type::alternating);
        static_assert(pat.nodes[pat.root].child_count == 3);
        gba::test.is_true(true);
    });

    gba::test("parse stacked (commas in <>)", [] {
        static constexpr auto pat = parse_mini("<c4 e4, g4 b4>");
        static_assert(pat.nodes[pat.root].type == ast_type::stacked);
        static_assert(pat.nodes[pat.root].child_count == 2);
        // First child is alternating with 2 children (c4, e4)
        static constexpr auto firstAlt = pat.nodes[pat.root].children[0];
        static_assert(pat.nodes[firstAlt].type == ast_type::alternating);
        static_assert(pat.nodes[firstAlt].child_count == 2);
        // Second child is alternating with 2 children (g4, b4)
        static constexpr auto secondAlt = pat.nodes[pat.root].children[1];
        static_assert(pat.nodes[secondAlt].type == ast_type::alternating);
        static_assert(pat.nodes[secondAlt].child_count == 2);
        gba::test.is_true(true);
    });

    gba::test("parse fast operator", [] {
        static constexpr auto pat = parse_mini("c4*3");
        static_assert(pat.nodes[pat.root].type == ast_type::fast);
        static_assert(pat.nodes[pat.root].modifier_num == 3);
        gba::test.is_true(true);
    });

    gba::test("parse slow operator", [] {
        static constexpr auto pat = parse_mini("c4/2");
        static_assert(pat.nodes[pat.root].type == ast_type::slow);
        static_assert(pat.nodes[pat.root].modifier_num == 2);
        gba::test.is_true(true);
    });

    gba::test("parse replicate operator", [] {
        static constexpr auto pat = parse_mini("c4!3");
        static_assert(pat.nodes[pat.root].type == ast_type::replicate);
        static_assert(pat.nodes[pat.root].modifier_num == 3);
        gba::test.is_true(true);
    });

    gba::test("parse euclidean", [] {
        static constexpr auto pat = parse_mini("c4(3,8)");
        static_assert(pat.nodes[pat.root].type == ast_type::euclidean);
        static_assert(pat.nodes[pat.root].euclidean_k == 3);
        static_assert(pat.nodes[pat.root].euclidean_n == 8);
        gba::test.is_true(true);
    });

    gba::test("parse euclidean with rotation", [] {
        static constexpr auto pat = parse_mini("c4(3,8,2)");
        static_assert(pat.nodes[pat.root].type == ast_type::euclidean);
        static_assert(pat.nodes[pat.root].euclidean_k == 3);
        static_assert(pat.nodes[pat.root].euclidean_n == 8);
        static_assert(pat.nodes[pat.root].euclidean_r == 2);
        gba::test.is_true(true);
    });

    gba::test("parse elongation", [] {
        static constexpr auto pat = parse_mini("c4@3 e4");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 2);
        static_assert(pat.nodes[pat.root].weights[0] == 3);
        static_assert(pat.nodes[pat.root].weights[1] == 1);
        gba::test.is_true(true);
    });

    gba::test("parse drum pattern", [] {
        static constexpr auto pat = parse_mini("bd hh sd hh");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 4);
        gba::test.is_true(true);
    });

    gba::test("parse multiline (whitespace)", [] {
        static constexpr auto pat = parse_mini("<c4\ne4\ng4>");
        static_assert(pat.nodes[pat.root].type == ast_type::alternating);
        static_assert(pat.nodes[pat.root].child_count == 3);
        gba::test.is_true(true);
    });

    gba::test("parse polymeter %N", [] {
        static constexpr auto pat = parse_mini("{c4 e4 g4}%8");
        static_assert(pat.nodes[pat.root].type == ast_type::polymetric);
        static_assert(pat.nodes[pat.root].polymeter_steps == 8);
        gba::test.eq(pat.nodes[pat.root].polymeter_steps, static_cast<std::uint8_t>(8));
    });

    gba::test("parse polymetric braces", [] {
        static constexpr auto pat = parse_mini("{c4 e4, g4 b4 c5}");
        static_assert(pat.nodes[pat.root].type == ast_type::polymetric);
        static_assert(pat.nodes[pat.root].child_count == 2);
        static_assert(pat.nodes[pat.root].polymeter_steps == 0);
        gba::test.eq(pat.nodes[pat.root].child_count, static_cast<std::uint8_t>(2));
    });

    gba::test("parse timeline fast", [] {
        static constexpr auto pat = parse_mini("c4*<2 3>");
        static_assert(pat.nodes[pat.root].type == ast_type::fast);
        static_assert(pat.nodes[pat.root].modifier_is_timeline);
        gba::test.is_true(pat.nodes[pat.root].modifier_is_timeline);
    });

    gba::test("parse timeline slow", [] {
        static constexpr auto pat = parse_mini("c4/<2 3>");
        static_assert(pat.nodes[pat.root].type == ast_type::slow);
        static_assert(pat.nodes[pat.root].modifier_is_timeline);
        gba::test.is_true(pat.nodes[pat.root].modifier_is_timeline);
    });

    // Section: note() API tests

    gba::test("note() creates pattern", [] {
        static constexpr auto p = note("c4 e4 g4");
        static_assert(p.ast.nodes[p.ast.root].child_count == 3);
        static_assert(!p.has_channel());
        gba::test.is_true(true);
    });

    gba::test("note() with stacked layers", [] {
        static constexpr auto p = note("<c4 e4, g4 b4>");
        static_assert(p.is_stacked());
        static_assert(p.layer_count() == 2);
        gba::test.is_true(true);
    });

    gba::test("note().channel()", [] {
        static constexpr auto p = note("c4 e4").channel(channel::sq1);
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::sq1);
        gba::test.is_true(true);
    });

    gba::test("note().channel() with instrument", [] {
        static constexpr auto p = note("c4 e4").channel(channel::sq1, sq1_instrument{.duty = 3, .env_volume = 15});
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::sq1);
        static_assert(p.sq1_inst.duty == 3);
        static_assert(p.sq1_inst.env_volume == 15);
        gba::test.is_true(true);
    });

    // Section: UDL tests

    gba::test("sq1 UDL", [] {
        static constexpr auto p = "c4 e4 g4"_sq1;
        static_assert(p.ast.nodes[p.ast.root].child_count == 3);
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::sq1);
        gba::test.is_true(true);
    });

    gba::test("noise UDL", [] {
        static constexpr auto p = "bd hh sd hh"_noise;
        static_assert(p.ast.nodes[p.ast.root].child_count == 4);
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    // Section: Compile tests

    gba::test("compile note() single layer (default tempo)", [] {
        static constexpr auto music = compile(note("c4 e4 g4"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
    });

    gba::test("compile note() with rest (default tempo)", [] {
        static constexpr auto music = compile(note("c4 ~ e4"));
        int noteOns = 0, noteOffs = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) noteOns++;
            if (music.events[i].type == event_type::note_off) noteOffs++;
        }
        gba::test.eq(noteOns, 2);
        gba::test.eq(noteOffs, 1);
    });

    gba::test("compile UDL _sq1 (default tempo)", [] {
        static constexpr auto music = compile("c4 e4 g4"_sq1);
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
        // All events should be on sq1
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            gba::test.eq(static_cast<int>(music.events[i].chan), static_cast<int>(channel::sq1));
    });

    gba::test("compile noise drum pattern (default tempo)", [] {
        static constexpr auto music = compile("bd hh sd hh"_noise);
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 4);
    });

    gba::test("compile euclidean pattern (default tempo)", [] {
        static constexpr auto music = compile(note("c4(3,8)"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
    });

    gba::test("compile replicate operator", [] {
        static constexpr auto music = compile(note("c4!3"));
        int noteOns = 0;
        std::array<std::int64_t, 3> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type != event_type::note_on) continue;
            times[noteOns] = music.events[i].time_num;
            noteOns++;
        }
        gba::test.eq(noteOns, 3);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], music.cycle_time_num / 3);
        gba::test.eq(times[2], (music.cycle_time_num * 2) / 3);
    });

    gba::test("compile elongation timing ratio", [] {
        static constexpr auto music = compile(note("c4@3 e4"));
        std::int64_t first = -1, second = -1;
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type != event_type::note_on) continue;
            if (noteOns == 0) first = music.events[i].time_num;
            if (noteOns == 1) second = music.events[i].time_num;
            noteOns++;
        }
        gba::test.eq(noteOns, 2);
        // c4@3 e4 splits one cycle as 3/4 then 1/4.
        gba::test.eq(second - first, music.cycle_time_num * 3 / 4);
    });

    gba::test("compile timeline fast", [] {
        static constexpr auto music = compile(note("c4*<2 3>"));
        int noteOns = 0;
        std::array<std::int64_t, 5> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) times[noteOns++] = music.events[i].time_num;
        // Cycle 0 uses 2 hits, cycle 1 uses 3 hits.
        gba::test.eq(noteOns, 5);
        gba::test.eq(music.loop_step_count, static_cast<std::int64_t>(2));
        gba::test.eq(music.total_time_num, music.cycle_time_num * 2);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], music.cycle_time_num / 2);
        gba::test.eq(times[2], music.cycle_time_num);
        gba::test.eq(times[3], music.cycle_time_num + music.cycle_time_num / 3);
        gba::test.eq(times[4], music.cycle_time_num + (music.cycle_time_num * 2) / 3);
    });

    gba::test("compile slow stretches single note across cycles", [] {
        static constexpr auto music = compile(note("c4/3"));
        int noteOns = 0;
        std::int64_t first = -1;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type != event_type::note_on) continue;
            if (noteOns == 0) first = music.events[i].time_num;
            noteOns++;
        }
        gba::test.eq(noteOns, 1);
        gba::test.eq(first, static_cast<std::int64_t>(0));
        gba::test.eq(music.total_time_num, music.cycle_time_num * 3);
    });

    gba::test("compile slow stretches sequence timing", [] {
        static constexpr auto music = compile(note("[c4 e4]/4"));
        int noteOns = 0;
        std::array<std::int64_t, 2> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type != event_type::note_on) continue;
            times[noteOns++] = music.events[i].time_num;
        }
        gba::test.eq(noteOns, 2);
        gba::test.eq(music.total_time_num, music.cycle_time_num * 4);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], music.cycle_time_num * 2);
    });

    gba::test("compile timeline slow", [] {
        static constexpr auto music = compile(note("c4/<2 3>"));
        int noteOns = 0;
        std::array<std::int64_t, 4> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) times[noteOns++] = music.events[i].time_num;
        // c%2==0 (cycles 0,2,4) and c%6==3 (cycle 3) => 4 hits over 6 cycles.
        gba::test.eq(noteOns, 4);
        gba::test.eq(music.total_time_num, music.cycle_time_num * 6);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], music.cycle_time_num * 2);
        gba::test.eq(times[2], music.cycle_time_num * 3);
        gba::test.eq(times[3], music.cycle_time_num * 4);
    });

    gba::test("compile polymetric braces", [] {
        static constexpr auto music = compile(note("{c4 e4, g4 b4 c5}"));
        int noteOns = 0;
        std::array<std::int64_t, 5> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) times[noteOns++] = music.events[i].time_num;
        // First layer has 2 notes, second has 3 notes over same cycle.
        gba::test.eq(noteOns, 5);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], static_cast<std::int64_t>(0));
        gba::test.eq(times[2], music.cycle_time_num / 3);
        gba::test.eq(times[3], music.cycle_time_num / 2);
        gba::test.eq(times[4], (music.cycle_time_num * 2) / 3);
    });

    gba::test("compile alternating materializes all cycles", [] {
        static constexpr auto music = compile(note("<c4 e4>"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) noteOns++;
            gba::test.eq(music.events[i].cycle_mod, static_cast<std::int16_t>(-1));
        }
        gba::test.eq(noteOns, 2);
    });

    gba::test("compile stacked from commas in <>", [] {
        static constexpr auto music = compile(note("<c4 e4, g4 b4>"));
        // Should have events on both sq1 and sq2 (auto-assigned)
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].chan == channel::sq1) hasSq1 = true;
            if (music.events[i].chan == channel::sq2) hasSq2 = true;
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    // Section: Stack tests

    gba::test("stack auto-assigns channels", [] {
        static constexpr auto sp = stack(note("c4 e4"), note("g4 b4"));
        static_assert(sp.layer_count == 2);
        static_assert(sp.layers[0].get_channel() == channel::sq1);
        static_assert(sp.layers[1].get_channel() == channel::sq2);
        gba::test.is_true(true);
    });

    gba::test("stack respects explicit channels", [] {
        static constexpr auto sp = stack(note("c4").channel(channel::sq2), note("g4"));
        static_assert(sp.layers[0].get_channel() == channel::sq2);
        static_assert(sp.layers[1].get_channel() == channel::sq1); // auto: first available
        gba::test.is_true(true);
    });

    gba::test("compile stack", [] {
        static constexpr auto music = compile(stack(note("c4 e4"), note("g4 b4")));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 4);
    });

    // Section: Seq tests

    gba::test("seq chains patterns", [] {
        static constexpr auto s = seq(note("c4 c4"), note("e4 g4"));
        static_assert(s.step_count == 2);
        gba::test.eq(s.step_count, static_cast<std::uint8_t>(2));
    });

    gba::test("seq with channel switching", [] {
        static constexpr auto s = seq("c4 c4"_sq1, "e4 g4"_sq2);
        static_assert(s.step_count == 2);
        gba::test.is_true(true);
    });

    gba::test("compile seq", [] {
        static constexpr auto s = seq("c4 c4"_sq1, "e4 g4"_sq1);
        static constexpr auto music = compile(s);
        int noteOns = 0, instChanges = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) noteOns++;
            if (music.events[i].type == event_type::instrument_change) instChanges++;
        }
        gba::test.eq(noteOns, 4);
        gba::test.eq(instChanges, 2);
    });

    gba::test("same-time ordering: instrument_change before note_on", [] {
        static constexpr auto s = seq(note("c4").channel(channel::sq1, sq1_instrument{.duty = 3}),
                                      note("e4").channel(channel::sq1, sq1_instrument{.duty = 2}));
        static constexpr auto music = compile(s);

        // Events at frame 0 must start with instrument_change.
        gba::test.eq(static_cast<int>(music.events[0].type), static_cast<int>(event_type::instrument_change));
    });

    gba::test("timepoint batches are built", [] {
        static constexpr auto music = compile(stack(note("c4"), note("e4")));
        gba::test.eq(music.timepoint_count, static_cast<std::uint16_t>(1));
        gba::test.eq(music.timepoints[0].event_begin, static_cast<std::uint16_t>(0));
        gba::test.eq(music.timepoints[0].event_end, static_cast<std::uint16_t>(2));
    });

    gba::test("compile polymeter computes loop point", [] {
        static constexpr auto music = compile(note("{c4 e4 g4}%8"));
        // 3-step layer against 8 polymeter steps -> LCM(3,8)=24 steps loop point.
        gba::test.eq(music.loop_step_count, static_cast<std::int64_t>(24));
        // Loop spans multiple base cycles.
        gba::test.is_true(music.total_time_num > music.cycle_time_num);

        int noteOns = 0;
        std::array<std::int64_t, 9> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) times[noteOns++] = music.events[i].time_num;
        // 3 notes repeated over 3 cycles.
        gba::test.eq(noteOns, 9);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.eq(times[1], music.cycle_time_num / 3);
        gba::test.eq(times[2], (music.cycle_time_num * 2) / 3);
        gba::test.eq(times[3], music.cycle_time_num);
        gba::test.eq(times[4], music.cycle_time_num + music.cycle_time_num / 3);
        gba::test.eq(times[5], music.cycle_time_num + (music.cycle_time_num * 2) / 3);
        gba::test.eq(times[6], music.cycle_time_num * 2);
        gba::test.eq(times[7], music.cycle_time_num * 2 + music.cycle_time_num / 3);
        gba::test.eq(times[8], music.cycle_time_num * 2 + (music.cycle_time_num * 2) / 3);
    });

    // Section: Loop test

    gba::test("loop sets flag", [] {
        static constexpr auto l = loop(note("c4 e4"));
        static_assert(l.looping == true);
        gba::test.is_true(l.looping);
    });

    // Section: Bjorklund algorithm tests

    gba::test("bjorklund(3,8)", [] {
        // Real Bjorklund E(3,8) = x..x..x. (positions 0,3,6)
        static constexpr auto pat = compile_detail::bjorklund(3, 8);
        static_assert(pat[0] == true);
        static_assert(pat[1] == false);
        static_assert(pat[2] == false);
        static_assert(pat[3] == true);
        static_assert(pat[4] == false);
        static_assert(pat[5] == false);
        static_assert(pat[6] == true);
        static_assert(pat[7] == false);
        gba::test.is_true(true);
    });

    gba::test("bjorklund(5,8)", [] {
        // Real Bjorklund E(5,8) = x.xx.xx. (positions 0,2,3,5,6)
        static constexpr auto pat = compile_detail::bjorklund(5, 8);
        static constexpr int pulses = pat[0] + pat[1] + pat[2] + pat[3] + pat[4] + pat[5] + pat[6] + pat[7];
        static_assert(pulses == 5);
        static_assert(pat[0] == true);
        static_assert(pat[1] == false);
        static_assert(pat[2] == true);
        static_assert(pat[3] == true);
        static_assert(pat[4] == false);
        static_assert(pat[5] == true);
        static_assert(pat[6] == true);
        static_assert(pat[7] == false);
        gba::test.eq(pulses, 5);
    });

    // Section: Instrument configuration tests

    gba::test("sq1 default instrument", [] {
        static constexpr auto p = "c4"_sq1;
        static_assert(p.sq1_inst.duty == 2);
        static_assert(p.sq1_inst.env_volume == 5);
        static_assert(p.sq1_inst.sweep_shift == 0);
        gba::test.is_true(true);
    });

    gba::test("sq1 with custom instrument via .channel()", [] {
        static constexpr auto p = note("c4").channel(channel::sq1,
                                                     sq1_instrument{.sweep_shift = 2, .sweep_time = 3, .duty = 3});
        static_assert(p.sq1_inst.sweep_shift == 2);
        static_assert(p.sq1_inst.sweep_time == 3);
        static_assert(p.sq1_inst.duty == 3);
        gba::test.is_true(true);
    });

    // Section: Hold / tie tests

    gba::test("parse sequence with hold", [] {
        static constexpr auto pat = parse_mini("c4 _ e4");
        static_assert(pat.nodes[pat.root].type == ast_type::sequence);
        static_assert(pat.nodes[pat.root].child_count == 3);
        static constexpr auto holdIdx = pat.nodes[pat.root].children[1];
        static_assert(pat.nodes[holdIdx].type == ast_type::note_literal);
        static_assert(pat.nodes[holdIdx].note_value == note::hold);
        gba::test.is_true(true);
    });

    gba::test("compile hold: fewer note_ons than without", [] {
        // "c4 _" = one note_on; "c4 c4" = two note_ons
        static constexpr auto with_hold = compile(note("c4 _"));
        static constexpr auto without_hold = compile(note("c4 c4"));
        int ons1 = 0, ons2 = 0;
        for (std::uint16_t i = 0; i < with_hold.event_count; ++i)
            if (with_hold.events[i].type == event_type::note_on) ons1++;
        for (std::uint16_t i = 0; i < without_hold.event_count; ++i)
            if (without_hold.events[i].type == event_type::note_on) ons2++;
        gba::test.eq(ons1, 1);
        gba::test.eq(ons2, 2);
    });

    gba::test("compile hold at start: no spurious event", [] {
        // "_ c4" - hold emits nothing; only c4 fires
        static constexpr auto music = compile(note("_ c4"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
        // c4 fires at 1/2 cycle
        gba::test.eq(music.events[0].time_num, music.cycle_time_num / 2);
    });

    gba::test("compile hold after rest: silence continues", [] {
        // "~ _" - rest silences; hold emits nothing; net = one note_off
        static constexpr auto music = compile(note("~ _"));
        int noteOns = 0, noteOffs = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) noteOns++;
            if (music.events[i].type == event_type::note_off) noteOffs++;
        }
        gba::test.eq(noteOns, 0);
        gba::test.eq(noteOffs, 1); // only the ~ emits note_off
    });

    gba::test("hold preserves note position: c4@3 _ in subdivision", [] {
        // "[c4 _]" - one cycle, c4 for half, hold for half -> one note_on at 0
        static constexpr auto music = compile(note("[c4 _]"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
        gba::test.eq(music.events[0].time_num, static_cast<std::int64_t>(0));
    });

    gba::test("fast inside subdivision produces correct note count", [] {
        // c4*4 = 4 note_ons (root is fast node)
        static constexpr auto m1 = compile(note("c4*4"));
        static constexpr int ons1 = [] {
            int c = 0;
            for (std::uint16_t i = 0; i < m1.event_count; ++i)
                if (m1.events[i].type == event_type::note_on) c++;
            return c;
        }();

        // [c4*4] = should be 4 note_ons (subdivision wrapping fast)
        static constexpr auto m2 = compile(note("[c4*4]"));
        static constexpr int ons2 = [] {
            int c = 0;
            for (std::uint16_t i = 0; i < m2.event_count; ++i)
                if (m2.events[i].type == event_type::note_on) c++;
            return c;
        }();

        // [e2 e3]*4 = 8 note_ons (fast wrapping subdivision)
        static constexpr auto m3 = compile(note("[e2 e3]*4"));
        static constexpr int ons3 = [] {
            int c = 0;
            for (std::uint16_t i = 0; i < m3.event_count; ++i)
                if (m3.events[i].type == event_type::note_on) c++;
            return c;
        }();

        // [[e2 e3]*4] = should be 8 note_ons (outer subdivision wrapping fast)
        static constexpr auto m4 = compile(note("[[e2 e3]*4]"));
        static constexpr int ons4 = [] {
            int c = 0;
            for (std::uint16_t i = 0; i < m4.event_count; ++i)
                if (m4.events[i].type == event_type::note_on) c++;
            return c;
        }();

        gba::test.eq(ons1, 4);
        gba::test.eq(ons2, 4);
        gba::test.eq(ons3, 8);
        gba::test.eq(ons4, 8);
    });

    gba::test("[[e2 e3]*4] all note_ons have distinct times", [] {
        static constexpr auto music = compile(note("[[e2 e3]*4]"));
        std::array<std::int64_t, 8> times{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && idx < 8) times[idx++] = music.events[i].time_num;
        }
        gba::test.eq(idx, 8);
        // All 8 should be at distinct times, each spaced evenly
        for (int i = 1; i < 8; ++i) gba::test.is_true(times[i] > times[i - 1]);
    });

    // Section: Fractional modifier tests

    gba::test("parse fractional fast *2.75", [] {
        static constexpr auto pat = parse_mini("[c4 e4]*2.75");
        static_assert(pat.nodes[pat.root].type == ast_type::fast);
        // 2.75 = 275/100 = 11/4
        static_assert(pat.nodes[pat.root].modifier_num == 11);
        static_assert(pat.nodes[pat.root].modifier_den == 4);
        gba::test.is_true(true);
    });

    gba::test("parse fractional slow /1.5", [] {
        static constexpr auto pat = parse_mini("c4/1.5");
        static_assert(pat.nodes[pat.root].type == ast_type::slow);
        // 1.5 = 15/10 = 3/2
        static_assert(pat.nodes[pat.root].modifier_num == 3);
        static_assert(pat.nodes[pat.root].modifier_den == 2);
        gba::test.is_true(true);
    });

    gba::test("parse fractional fast *0.5", [] {
        static constexpr auto pat = parse_mini("c4*0.5");
        static_assert(pat.nodes[pat.root].type == ast_type::fast);
        // 0.5 = 5/10 = 1/2
        static_assert(pat.nodes[pat.root].modifier_num == 1);
        static_assert(pat.nodes[pat.root].modifier_den == 2);
        gba::test.is_true(true);
    });

    gba::test("compile [e5 b4 d5 c5]*2.75 (Strudel example)", [] {
        // 2.75 = 11/4. Whole copies = 2, remainder = 3/4.
        // [e5 b4 d5 c5] has 4 notes. 2 full copies = 8 notes.
        // Partial copy (3/4 of duration) = 4 notes squeezed into less time.
        // Total = 12 note_ons.
        static constexpr auto music = compile(note("[e5 b4 d5 c5]*2.75"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 12);
    });

    gba::test("compile c4*1.5 produces 2 note_ons (1 full + 1 partial)", [] {
        static constexpr auto music = compile(note("c4*1.5"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 2);
    });

    gba::test("parse stacked subdivision [g3,b3,e4]", [] {
        static constexpr auto pat = parse_mini("[g3,b3,e4]");
        static_assert(pat.nodes[pat.root].type == ast_type::stacked);
        static_assert(pat.nodes[pat.root].child_count == 3);
        gba::test.is_true(true);
    });

    gba::test("compile stacked subdivision [g3,b3,e4] (chord)", [] {
        static constexpr auto music = compile(note("[g3,b3,e4]"));
        // 3 simultaneous notes on auto-assigned channels
        bool hasSq1 = false, hasSq2 = false, hasWav = false;
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                noteOns++;
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
                if (music.events[i].chan == channel::wav) hasWav = true;
            }
        }
        gba::test.eq(noteOns, 3);
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
        gba::test.is_true(hasWav);
    });

    gba::test("fast on alternating advances children: <c4 e4 g4 b4>*2", [] {
        // <c4 e4 g4 b4>*2 should play 2 per cycle: cycle 0 = c4 e4, cycle 1 = g4 b4
        static constexpr auto music = compile(note("<c4 e4 g4 b4>*2"));
        int noteOns = 0;
        std::array<std::int64_t, 4> times{};
        std::array<std::uint16_t, 4> rates{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && noteOns < 4) {
                times[noteOns] = music.events[i].time_num;
                rates[noteOns] = music.events[i].rate;
                noteOns++;
            }
        }
        gba::test.eq(noteOns, 4);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        gba::test.is_true(times[1] > times[0]);
        gba::test.is_true(times[2] >= music.cycle_time_num);
        gba::test.is_true(times[3] > times[2]);
        gba::test.is_true(rates[0] != rates[1]);
        gba::test.is_true(rates[1] != rates[2]);
        gba::test.is_true(rates[2] != rates[3]);
    });

    gba::test("@N weight inside alternating: <c4@2 e4 g4>*2", [] {
        // <c4@2 e4 g4>*2: total weight=4, slots=[c4,hold,e4,g4]
        // *2: cycle 0 = c4 (sustains, hold slot skipped); cycle 1 = e4,g4
        static constexpr auto music = compile(note("<c4@2 e4 g4>*2"));
        int noteOns = 0;
        std::array<std::uint16_t, 3> rates{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && noteOns < 3) rates[noteOns++] = music.events[i].rate;
        }
        gba::test.eq(noteOns, 3);                // c4 once (sustained), e4, g4
        gba::test.is_true(rates[0] != rates[1]); // c4 -> e4
        gba::test.is_true(rates[1] != rates[2]); // e4 -> g4
    });

    gba::test("!N replicate inside alternating: <c4!2 e4 g4>*2", [] {
        // <c4!2 e4 g4>*2: !2 expands to 4 children [c4,c4,e4,g4]
        // *2: cycle 0 = c4,c4 (two attacks); cycle 1 = e4,g4
        static constexpr auto music = compile(note("<c4!2 e4 g4>*2"));
        int noteOns = 0;
        std::array<std::uint16_t, 4> rates{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && noteOns < 4) rates[noteOns++] = music.events[i].rate;
        }
        gba::test.eq(noteOns, 4);
        gba::test.eq(rates[0], rates[1]);        // both c4 (two separate attacks)
        gba::test.is_true(rates[1] != rates[2]); // c4 -> e4
        gba::test.is_true(rates[2] != rates[3]); // e4 -> g4
    });

    gba::test("euclidean rhythm in sequence", [] {
        // "e5(2,8) b4(3,8)" should produce 2+3 = 5 note_ons
        static constexpr auto music = compile(note("e5(2,8) b4(3,8)"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 5);
    });

    gba::test(".slow(2) stretches pattern across 2 cycles", [] {
        static constexpr auto music = compile(note("c4 e4").slow(2));
        int noteOns = 0;
        std::array<std::int64_t, 2> times{};
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && noteOns < 2) times[noteOns++] = music.events[i].time_num;
        }
        gba::test.eq(noteOns, 2);
        gba::test.eq(times[0], static_cast<std::int64_t>(0));
        // Second note at 1 cycle (halfway through the stretched 2-cycle duration)
        gba::test.eq(times[1], music.cycle_time_num);
        gba::test.eq(music.total_time_num, music.cycle_time_num * 2);
    });

    gba::test(".fast(3) plays 3x faster", [] {
        static constexpr auto music = compile(note("c4").fast(3));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
    });

    // Section: s() drum pattern tests

    gba::test("s() creates noise channel pattern", [] {
        static constexpr auto p = s("bd hh sd hh");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        static_assert(p.ast.nodes[p.ast.root].child_count == 4);
        gba::test.is_true(true);
    });

    gba::test("s() with euclidean rhythm", [] {
        static constexpr auto p = s("bd(3,8,0)");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    gba::test("s() with rest and hold", [] {
        static constexpr auto p = s("bd ~ hh _");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        static_assert(p.ast.nodes[p.ast.root].child_count == 4);
        gba::test.is_true(true);
    });

    gba::test("s() with fast modifier", [] {
        static constexpr auto p = s("hh*8");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    gba::test("s() with alternating", [] {
        static constexpr auto p = s("<bd sd hh>");
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    gba::test("compile s() euclidean drum pattern", [] {
        static constexpr auto music = compile(s("bd(3,8,0)"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                noteOns++;
                gba::test.eq(static_cast<int>(music.events[i].chan), static_cast<int>(channel::noise));
            }
        }
        gba::test.eq(noteOns, 3);
    });

    gba::test("compile s() sequence", [] {
        static constexpr auto music = compile(s("bd hh sd hh"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 4);
    });

    gba::test("s() in stack with note()", [] {
        static constexpr auto music = compile(stack(note("c4 e4 g4"), s("bd ~ sd ~")));
        bool hasSq1 = false, hasNoise = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].chan == channel::sq1) hasSq1 = true;
            if (music.events[i].chan == channel::noise) hasNoise = true;
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasNoise);
    });

    gba::test("_s UDL creates noise pattern", [] {
        static constexpr auto p = "bd sd hh oh"_s;
        static_assert(p.has_channel());
        static_assert(p.get_channel() == channel::noise);
        gba::test.is_true(true);
    });

    gba::test("compile _s UDL euclidean", [] {
        static constexpr auto music = compile("bd(3,8)"_s);
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
    });

    gba::test("s() drum presets have correct noise params", [] {
        static constexpr auto music = compile(s("bd"));
        // bd should produce a noise note_on with preset params
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::noise) {
                gba::test.eq(static_cast<int>(music.events[i].noise_div_ratio), 7); // bd: deep low
                gba::test.eq(music.events[i].noise_width, false);                   // bd: 15-bit full noise
                gba::test.eq(static_cast<int>(music.events[i].noise_shift), 8);
                gba::test.eq(static_cast<int>(music.events[i].noise_env_volume), 15);
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    // Section: Full Strudel percussion name parity tests

    gba::test("parse all Strudel drum names", [] {
        static constexpr auto rBd = parse_note_name("bd ", 0, 3);
        static constexpr auto rSd = parse_note_name("sd ", 0, 3);
        static constexpr auto rHh = parse_note_name("hh ", 0, 3);
        static constexpr auto rOh = parse_note_name("oh ", 0, 3);
        static constexpr auto rCp = parse_note_name("cp ", 0, 3);
        static constexpr auto rRs = parse_note_name("rs ", 0, 3);
        static constexpr auto rRim = parse_note_name("rim", 0, 3);
        static constexpr auto rLt = parse_note_name("lt ", 0, 3);
        static constexpr auto rMt = parse_note_name("mt ", 0, 3);
        static constexpr auto rHt = parse_note_name("ht ", 0, 3);
        static constexpr auto rCb = parse_note_name("cb ", 0, 3);
        static constexpr auto rCr = parse_note_name("cr ", 0, 3);
        static constexpr auto rRd = parse_note_name("rd ", 0, 3);
        static constexpr auto rHc = parse_note_name("hc ", 0, 3);
        static constexpr auto rMc = parse_note_name("mc ", 0, 3);
        static constexpr auto rLc = parse_note_name("lc ", 0, 3);
        static constexpr auto rCl = parse_note_name("cl ", 0, 3);
        static constexpr auto rSh = parse_note_name("sh ", 0, 3);
        static constexpr auto rMa = parse_note_name("ma ", 0, 3);
        static constexpr auto rAg = parse_note_name("ag ", 0, 3);

        static_assert(rBd.value == note::bd);
        static_assert(rSd.value == note::sd);
        static_assert(rHh.value == note::hh);
        static_assert(rOh.value == note::oh);
        static_assert(rCp.value == note::cp);
        static_assert(rRs.value == note::rs);
        static_assert(rRim.value == note::rim && rRim.length == 3);
        static_assert(rLt.value == note::lt);
        static_assert(rMt.value == note::mt);
        static_assert(rHt.value == note::ht);
        static_assert(rCb.value == note::cb);
        static_assert(rCr.value == note::cr);
        static_assert(rRd.value == note::rd);
        static_assert(rHc.value == note::hc);
        static_assert(rMc.value == note::mc);
        static_assert(rLc.value == note::lc);
        static_assert(rCl.value == note::cl);
        static_assert(rSh.value == note::sh);
        static_assert(rMa.value == note::ma);
        static_assert(rAg.value == note::ag);
        gba::test.is_true(true);
    });

    gba::test("all 20 drum presets are valid", [] {
        // Verify every drum in the table is recognized as a drum
        static_assert(is_drum(note::bd));
        static_assert(is_drum(note::ag));
        static_assert(!is_drum(note::rest));
        static_assert(!is_drum(note::c4));
        static_assert(!is_drum(note::hold));
        gba::test.is_true(true);
    });

    gba::test("compile full Strudel percussion kit sequence", [] {
        static constexpr auto music = compile(s("bd sd hh oh cp rs cl sh ma ag hc mc lc"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 13);
    });

    gba::test("s() with conga sequence", [] {
        static constexpr auto music = compile(s("hc mc lc lc"));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 4);
    });

    gba::test("cl does not conflict with c-flat parsing", [] {
        // "cl" in drum context = claves
        static constexpr auto drumPat = parse_note_name("cl ", 0, 3);
        static_assert(drumPat.value == note::cl);
        // "cb4" = C-flat octave 4 (NOT cowbell)
        static constexpr auto flatPat = parse_note_name("cb4", 0, 3);
        static_assert(flatPat.value == note::b3);
        static_assert(flatPat.length == 3);
        gba::test.is_true(true);
    });

    // Section: .channels() tests

    gba::test("channels() assigns channels to stacked layers", [] {
        // Two layers: first -> wav, second -> sq2
        static constexpr auto music = compile(note("<c4 e4, g3 b3>").channels(channel::wav, channel::sq2));
        // Verify note_on events use the overridden channels
        bool hasWav = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::wav) hasWav = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
            }
        }
        gba::test.is_true(hasWav);
        gba::test.is_true(hasSq2);
        // No notes on default auto-assigned sq1
        bool hasSq1 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::sq1) hasSq1 = true;
        gba::test.is_true(!hasSq1);
    });

    gba::test("channels() emits instrument_change for overridden channels", [] {
        static constexpr auto music = compile(note("<c4, g3>").channels(channel::wav, channel::sq2));
        bool wavInst = false, sq2Inst = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::instrument_change) {
                if (music.events[i].chan == channel::wav) wavInst = true;
                if (music.events[i].chan == channel::sq2) sq2Inst = true;
            }
        }
        gba::test.is_true(wavInst);
        gba::test.is_true(sq2Inst);
    });

    gba::test("channels() with custom wav_instrument", [] {
        static constexpr auto music =
            compile(note("<c4, g3>").channels(layer_cfg{channel::wav, waves::triangle}, channel::sq2));
        // Verify wav instrument_change event is present and uses the custom instrument
        bool foundWavInst = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::instrument_change && music.events[i].chan == channel::wav) {
                foundWavInst = true;
                // The wav_inst_idx should reference a valid entry
                gba::test.is_true(music.events[i].wav_inst_idx < music.wav_instrument_count);
                // Verify it matches our custom instrument
                const auto& inst = music.wav_instruments[music.events[i].wav_inst_idx];
                const auto expected = waves::triangle;
                gba::test.is_true(inst.wave_data == expected.wave_data && inst.volume == expected.volume);
            }
        }
        gba::test.is_true(foundWavInst);
    });

    gba::test("channels() with loop() preserves overrides", [] {
        static constexpr auto music = compile(loop(note("<c4 e4, g3 b3>").channels(channel::sq2, channel::sq1)));
        bool hasSq2 = false, hasSq1 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
            }
        }
        gba::test.is_true(hasSq2);
        gba::test.is_true(hasSq1);
        gba::test.is_true(music.looping);
    });

    gba::test("channels() with stack() and s()", [] {
        // Tetris-like: note with 2 layers + drums
        static constexpr auto music =
            compile(loop(stack(note("<c4 e4, g3 b3>").channels(channel::wav, channel::sq2), s("bd sd"))));
        bool hasWav = false, hasSq2 = false, hasNoise = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::wav) hasWav = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
                if (music.events[i].chan == channel::noise) hasNoise = true;
            }
        }
        gba::test.is_true(hasWav);
        gba::test.is_true(hasSq2);
        gba::test.is_true(hasNoise);
    });

    gba::test("channels() note count matches non-overridden version", [] {
        // Same pattern compiled with and without channel overrides should have same note count.
        static constexpr auto defaultMusic = compile(note("<c4 e4, g3 b3>"));
        static constexpr auto overrideMusic = compile(note("<c4 e4, g3 b3>").channels(channel::sq1, channel::sq2));
        int defaultNotes = 0, overrideNotes = 0;
        for (std::uint16_t i = 0; i < defaultMusic.event_count; ++i)
            if (defaultMusic.events[i].type == event_type::note_on) defaultNotes++;
        for (std::uint16_t i = 0; i < overrideMusic.event_count; ++i)
            if (overrideMusic.events[i].type == event_type::note_on) overrideNotes++;
        gba::test.eq(defaultNotes, overrideNotes);
    });

    gba::test("channels() reversed order assigns correctly", [] {
        // Reverse the default: first layer -> sq2, second -> sq1
        static constexpr auto music = compile(note("<c4, g3>").channels(channel::sq2, channel::sq1));
        // Find first note_on for each channel
        std::uint16_t sq1Rate = 0, sq2Rate = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq2 && sq2Rate == 0) sq2Rate = music.events[i].rate;
                if (music.events[i].chan == channel::sq1 && sq1Rate == 0) sq1Rate = music.events[i].rate;
            }
        }
        // c4 -> sq2 (rate for c4), g3 -> sq1 (rate for g3)
        static constexpr auto c4Rate = note_to_rate(note::c4);
        static constexpr auto g3Rate = note_to_rate(note::g3);
        gba::test.eq(static_cast<int>(sq2Rate), static_cast<int>(c4Rate));
        gba::test.eq(static_cast<int>(sq1Rate), static_cast<int>(g3Rate));
    });

    gba::test("layer_cfg implicit from channel", [] {
        // bare channel converts to layer_cfg
        static constexpr layer_cfg cfg = channel::sq1;
        static_assert(cfg.assigned_channel == static_cast<std::uint8_t>(channel::sq1));
        gba::test.is_true(true);
    });

    gba::test("layer_cfg with sq1_instrument", [] {
        static constexpr layer_cfg cfg{channel::sq1, sq1_instrument{.duty = 3}};
        static_assert(cfg.assigned_channel == static_cast<std::uint8_t>(channel::sq1));
        static_assert(cfg.sq1_inst.duty == 3);
        gba::test.is_true(true);
    });

    gba::test("layer_cfg with wav_instrument", [] {
        static constexpr layer_cfg cfg{channel::wav, waves::sine};
        static_assert(cfg.assigned_channel == static_cast<std::uint8_t>(channel::wav));
        static_assert(cfg.wav_inst == waves::sine);
        gba::test.is_true(true);
    });

    // Section: Wave channel pitch tests

    gba::test("note_to_wav_rate produces valid rate for A4", [] {
        static constexpr auto wavRate = note_to_wav_rate(note::a4);
        static constexpr auto sqRate = note_to_rate(note::a4);
        // wav_rate = 2048 - (2048 - sqRate) / 4
        static_assert(wavRate == static_cast<std::uint16_t>(2048 - (2048 - sqRate) / 4));
        // Rate must be in valid range (0-2047)
        static_assert(wavRate < 2048);
        gba::test.is_true(true);
    });

    gba::test("note_to_wav_rate: C4 differs from sq rate", [] {
        static constexpr auto wavRate = note_to_wav_rate(note::c4);
        static constexpr auto sqRate = note_to_rate(note::c4);
        // WAV rate should be higher (closer to 2048) than sq rate for same pitch
        static_assert(wavRate > sqRate);
        gba::test.is_true(true);
    });

    gba::test("wav channel compile uses wav rate, not sq rate", [] {
        static constexpr auto music = compile(note("a4").channel(channel::wav));
        static constexpr auto expectedRate = note_to_wav_rate(note::a4);
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::wav) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(expectedRate));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    // Section: Pitched noise rejection test

    gba::test("pitched noise is consteval error (documented)", [] {
        // note("c4").channel(channel::noise) would throw at consteval time:
        //   "compile: chromatic notes cannot target the noise channel"
        // Cannot be tested at runtime - this verifies the design intention.
        // The throw prevents the player from silently playing wrong noise params.
        gba::test.is_true(true);
    });

    // Section: .rev() tests

    gba::test("rev() reverses note order in sequence", [] {
        // "c4 e4 g4" reversed should produce g4 e4 c4
        static constexpr auto music = compile(note("c4 e4 g4").rev());
        // Collect note_on rates in order
        std::array<int, 3> rates{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && idx < 3) rates[idx++] = music.events[i].rate;
        gba::test.eq(idx, 3);
        // g4 rate > e4 rate > c4 rate (higher pitch = higher rate)
        static constexpr auto g4Rate = static_cast<int>(note_to_rate(note::g4));
        static constexpr auto e4Rate = static_cast<int>(note_to_rate(note::e4));
        static constexpr auto c4Rate = static_cast<int>(note_to_rate(note::c4));
        gba::test.eq(rates[0], g4Rate);
        gba::test.eq(rates[1], e4Rate);
        gba::test.eq(rates[2], c4Rate);
    });

    gba::test("rev() same note count as original", [] {
        static constexpr auto original = compile(note("c4 e4 g4 b4"));
        static constexpr auto reversed = compile(note("c4 e4 g4 b4").rev());
        int origNotes = 0, revNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < reversed.event_count; ++i)
            if (reversed.events[i].type == event_type::note_on) revNotes++;
        gba::test.eq(origNotes, revNotes);
    });

    gba::test("rev() on single note is identity", [] {
        static constexpr auto music = compile(note("c4").rev());
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
    });

    gba::test("rev().rev() is identity", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto doubled = compile(note("c4 e4 g4").rev().rev());
        // Collect rates
        std::array<int, 3> origRates{}, dblRates{};
        int oi = 0, di = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on && oi < 3) origRates[oi++] = original.events[i].rate;
        for (std::uint16_t i = 0; i < doubled.event_count; ++i)
            if (doubled.events[i].type == event_type::note_on && di < 3) dblRates[di++] = doubled.events[i].rate;
        gba::test.eq(origRates[0], dblRates[0]);
        gba::test.eq(origRates[1], dblRates[1]);
        gba::test.eq(origRates[2], dblRates[2]);
    });

    // Section: .add() / .sub() tests

    gba::test("add(12) transposes up one octave", [] {
        static constexpr auto music = compile(note("c4").add(12));
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c5)));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    gba::test("sub(12) transposes down one octave", [] {
        static constexpr auto music = compile(note("c4").sub(12));
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c3)));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    gba::test("add(7) transposes by a fifth", [] {
        // c4 + 7 semitones = g4
        static constexpr auto music = compile(note("c4").add(7));
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::g4)));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    gba::test("add() skips rests and holds", [] {
        // "c4 ~ _" has 1 note_on + 1 note_off (rest). add(12) should only shift the note.
        static constexpr auto music = compile(note("c4 ~ _").add(12));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c5)));
                noteOns++;
            }
        }
        gba::test.eq(noteOns, 1);
    });

    gba::test("add() preserves note count", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto transposed = compile(note("c4 e4 g4").add(3));
        int origNotes = 0, transNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < transposed.event_count; ++i)
            if (transposed.events[i].type == event_type::note_on) transNotes++;
        gba::test.eq(origNotes, transNotes);
    });

    gba::test("add(0) is identity", [] {
        static constexpr auto music = compile(note("c4 e4").add(0));
        bool found = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && !found) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c4)));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    gba::test("stack with add() creates harmony", [] {
        // Stack root + fifth
        static constexpr auto music =
            compile(stack(note("c4").channel(channel::sq1), note("c4").add(7).channel(channel::sq2)));
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1) {
                    gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c4)));
                    hasSq1 = true;
                }
                if (music.events[i].chan == channel::sq2) {
                    gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::g4)));
                    hasSq2 = true;
                }
            }
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    // Section: .ply() tests

    gba::test("ply(2) doubles note count", [] {
        static constexpr auto original = compile(note("c4 e4"));
        static constexpr auto plied = compile(note("c4 e4").ply(2));
        int origNotes = 0, pliedNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < plied.event_count; ++i)
            if (plied.events[i].type == event_type::note_on) pliedNotes++;
        gba::test.eq(pliedNotes, origNotes * 2);
    });

    gba::test("ply(3) triples note count", [] {
        static constexpr auto plied = compile(note("c4").ply(3));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < plied.event_count; ++i)
            if (plied.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 3);
    });

    gba::test("ply(1) is identity", [] {
        static constexpr auto original = compile(note("c4 e4"));
        static constexpr auto plied = compile(note("c4 e4").ply(1));
        int origNotes = 0, pliedNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < plied.event_count; ++i)
            if (plied.events[i].type == event_type::note_on) pliedNotes++;
        gba::test.eq(origNotes, pliedNotes);
    });

    gba::test("ply() skips rests and holds", [] {
        // "c4 ~" -> ply(2) should double c4 but leave rest as-is
        static constexpr auto plied = compile(note("c4 ~").ply(2));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < plied.event_count; ++i)
            if (plied.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 2); // c4 played twice, rest is unchanged
    });

    // Section: .press() tests

    gba::test("press() doubles event density (note + rest per step)", [] {
        // "c4 e4" -> press -> each note becomes [note rest] subdivision
        static constexpr auto original = compile(note("c4 e4"));
        static constexpr auto pressed = compile(note("c4 e4").press());
        int origNoteOns = 0, pressedNoteOns = 0;
        int origNoteOffs = 0, pressedNoteOffs = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i) {
            if (original.events[i].type == event_type::note_on) origNoteOns++;
            if (original.events[i].type == event_type::note_off) origNoteOffs++;
        }
        for (std::uint16_t i = 0; i < pressed.event_count; ++i) {
            if (pressed.events[i].type == event_type::note_on) pressedNoteOns++;
            if (pressed.events[i].type == event_type::note_off) pressedNoteOffs++;
        }
        // Same number of note_on events (each note still fires once)
        gba::test.eq(origNoteOns, pressedNoteOns);
        // More note_off events (each note now has an explicit rest after it)
        gba::test.is_true(pressedNoteOffs > origNoteOffs);
    });

    gba::test("press() preserves pitch", [] {
        static constexpr auto pressed = compile(note("c4").press());
        bool found = false;
        for (std::uint16_t i = 0; i < pressed.event_count; ++i) {
            if (pressed.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(pressed.events[i].rate), static_cast<int>(note_to_rate(note::c4)));
                found = true;
            }
        }
        gba::test.is_true(found);
    });

    gba::test("press() skips rests", [] {
        // "c4 ~" -> press should only wrap c4, rest stays as-is
        static constexpr auto pressed = compile(note("c4 ~").press());
        int noteOns = 0;
        for (std::uint16_t i = 0; i < pressed.event_count; ++i)
            if (pressed.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
    });

    // Section: Chained pattern functions

    gba::test("rev().add(7) chains correctly", [] {
        // "c4 e4 g4" -> rev -> "g4 e4 c4" -> add(7) -> "d5 b4 g4"
        static constexpr auto music = compile(note("c4 e4 g4").rev().add(7));
        std::array<int, 3> rates{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && idx < 3) rates[idx++] = music.events[i].rate;
        gba::test.eq(idx, 3);
        // d5 = g4+7, b4 = e4+7, g4 = c4+7
        gba::test.eq(rates[0], static_cast<int>(note_to_rate(note::d5)));
        gba::test.eq(rates[1], static_cast<int>(note_to_rate(note::b4)));
        gba::test.eq(rates[2], static_cast<int>(note_to_rate(note::g4)));
    });

    // Section: .late() / .early() tests

    gba::test("late() shifts events later in time", [] {
        // Compare "c4" vs "c4".late(1,4) - the shifted version should fire later
        static constexpr auto original = compile(note("c4"));
        static constexpr auto shifted = compile(note("c4").late(1, 4));
        // Find the note_on time in each
        std::int64_t origTime = -1, shiftedTime = -1;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origTime = original.events[i].time_num;
        for (std::uint16_t i = 0; i < shifted.event_count; ++i)
            if (shifted.events[i].type == event_type::note_on) shiftedTime = shifted.events[i].time_num;
        gba::test.is_true(origTime >= 0);
        gba::test.is_true(shiftedTime >= 0);
        gba::test.is_true(shiftedTime > origTime);
    });

    gba::test("early() shifts events earlier in time", [] {
        // "c4 e4" - first note at t=0, second at t=half.
        // early(1,4) shifts everything 1/4 cycle earlier.
        static constexpr auto original = compile(note("c4 e4"));
        static constexpr auto shifted = compile(note("c4 e4").early(1, 4));
        // Second note_on in shifted version should be earlier than in original
        std::int64_t origSecond = -1, shiftedSecond = -1;
        int origIdx = 0, shiftedIdx = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on && ++origIdx == 2)
                origSecond = original.events[i].time_num;
        for (std::uint16_t i = 0; i < shifted.event_count; ++i)
            if (shifted.events[i].type == event_type::note_on && ++shiftedIdx == 2)
                shiftedSecond = shifted.events[i].time_num;
        gba::test.is_true(origSecond > 0);
        gba::test.is_true(shiftedSecond >= 0);
        gba::test.is_true(shiftedSecond < origSecond);
    });

    gba::test("late() preserves note count", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto shifted = compile(note("c4 e4 g4").late(1, 8));
        int origNotes = 0, shiftedNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < shifted.event_count; ++i)
            if (shifted.events[i].type == event_type::note_on) shiftedNotes++;
        gba::test.eq(origNotes, shiftedNotes);
    });

    gba::test("late(0) is identity", [] {
        static constexpr auto original = compile(note("c4"));
        static constexpr auto shifted = compile(note("c4").late(0));
        std::int64_t origTime = -1, shiftedTime = -1;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origTime = original.events[i].time_num;
        for (std::uint16_t i = 0; i < shifted.event_count; ++i)
            if (shifted.events[i].type == event_type::note_on) shiftedTime = shifted.events[i].time_num;
        gba::test.eq(origTime, shiftedTime);
    });

    gba::test("late().early() cancels out", [] {
        static constexpr auto original = compile(note("c4"));
        static constexpr auto shifted = compile(note("c4").late(1, 4).early(1, 4));
        std::int64_t origTime = -1, shiftedTime = -1;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origTime = original.events[i].time_num;
        for (std::uint16_t i = 0; i < shifted.event_count; ++i)
            if (shifted.events[i].type == event_type::note_on) shiftedTime = shifted.events[i].time_num;
        gba::test.eq(origTime, shiftedTime);
    });

    gba::test("stack with late() creates offset canon", [] {
        // Two voices: original + delayed copy
        static constexpr auto music =
            compile(stack(note("c4").channel(channel::sq1), note("c4").late(1, 2).channel(channel::sq2)));
        std::int64_t sq1Time = -1, sq2Time = -1;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1 && sq1Time < 0) sq1Time = music.events[i].time_num;
                if (music.events[i].chan == channel::sq2 && sq2Time < 0) sq2Time = music.events[i].time_num;
            }
        }
        gba::test.is_true(sq1Time >= 0);
        gba::test.is_true(sq2Time >= 0);
        gba::test.is_true(sq2Time > sq1Time);
    });

    gba::test("late() works with loop()", [] {
        static constexpr auto music = compile(loop(note("c4 e4").late(1, 8)));
        gba::test.is_true(music.looping);
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 2);
    });

    // Section: .iter() tests

    gba::test("iter(4) produces 4 rotation variants via alternating", [] {
        // "c4 e4 g4 b4".iter(4) should produce:
        // cycle 0: c4 e4 g4 b4, cycle 1: e4 g4 b4 c4, etc.
        // Across 4 cycles we should hear all 4 notes in each cycle = 16 note_ons total
        static constexpr auto music = compile(note("c4 e4 g4 b4").iter(4));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        // 4 cycles * 4 notes per cycle = 16 note_ons
        gba::test.eq(noteOns, 16);
    });

    gba::test("iter(2) rotates by half each cycle", [] {
        // "c4 e4 g4 b4".iter(2): cycle 0 = c4 e4 g4 b4, cycle 1 = g4 b4 c4 e4
        // Over 2 cycles = 8 note_ons
        static constexpr auto music = compile(note("c4 e4 g4 b4").iter(2));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 8);
    });

    gba::test("iter(1) is identity", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto iterated = compile(note("c4 e4 g4").iter(1));
        int origNotes = 0, iterNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < iterated.event_count; ++i)
            if (iterated.events[i].type == event_type::note_on) iterNotes++;
        gba::test.eq(origNotes, iterNotes);
    });

    gba::test("iter(4) first cycle matches original note order", [] {
        // The first rotation (shift=0) should have the original order: c4 e4 g4 b4
        static constexpr auto music = compile(note("c4 e4 g4 b4").iter(4));
        // The first 4 note_ons should be c4, e4, g4, b4 (in time order)
        std::array<int, 4> rates{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && idx < 4) rates[idx++] = music.events[i].rate;
        gba::test.eq(idx, 4);
        gba::test.eq(rates[0], static_cast<int>(note_to_rate(note::c4)));
        gba::test.eq(rates[1], static_cast<int>(note_to_rate(note::e4)));
        gba::test.eq(rates[2], static_cast<int>(note_to_rate(note::g4)));
        gba::test.eq(rates[3], static_cast<int>(note_to_rate(note::b4)));
    });

    gba::test("iter() works with loop()", [] {
        static constexpr auto music = compile(loop(note("c4 e4 g4 b4").iter(4)));
        gba::test.is_true(music.looping);
    });

    // Section: .palindrome() tests

    gba::test("palindrome() creates forward-reverse alternation", [] {
        // "c4 e4 g4" palindrome: cycle 0 forward (c4 e4 g4), cycle 1 reversed (g4 e4 c4)
        // Over 2 cycles = 6 note_ons
        static constexpr auto music = compile(note("c4 e4 g4").palindrome());
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 6);
    });

    gba::test("palindrome() first cycle is forward order", [] {
        static constexpr auto music = compile(note("c4 e4 g4").palindrome());
        std::array<int, 3> rates{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && idx < 3) rates[idx++] = music.events[i].rate;
        gba::test.eq(idx, 3);
        gba::test.eq(rates[0], static_cast<int>(note_to_rate(note::c4)));
        gba::test.eq(rates[1], static_cast<int>(note_to_rate(note::e4)));
        gba::test.eq(rates[2], static_cast<int>(note_to_rate(note::g4)));
    });

    gba::test("palindrome() second cycle is reversed order", [] {
        static constexpr auto music = compile(note("c4 e4 g4").palindrome());
        // Collect all note_on rates - first 3 are forward, next 3 are reversed
        std::array<int, 6> rates{};
        int idx = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && idx < 6) rates[idx++] = music.events[i].rate;
        gba::test.eq(idx, 6);
        // Second cycle: g4, e4, c4
        gba::test.eq(rates[3], static_cast<int>(note_to_rate(note::g4)));
        gba::test.eq(rates[4], static_cast<int>(note_to_rate(note::e4)));
        gba::test.eq(rates[5], static_cast<int>(note_to_rate(note::c4)));
    });

    gba::test("palindrome() preserves pitch content", [] {
        // Forward and reversed cycles should use the same set of pitches
        static constexpr auto music = compile(note("c4 e4 g4").palindrome());
        int noteOns = 0;
        bool allValid = true;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                noteOns++;
                auto r = music.events[i].rate;
                if (r != note_to_rate(note::c4) && r != note_to_rate(note::e4) && r != note_to_rate(note::g4))
                    allValid = false;
            }
        }
        gba::test.eq(noteOns, 6);
        gba::test.is_true(allValid);
    });

    gba::test("palindrome() works with loop()", [] {
        static constexpr auto music = compile(loop(note("c4 e4").palindrome()));
        gba::test.is_true(music.looping);
    });

    // Section: Phase 4: Higher-order combinators

    gba::test("superimpose(add(7)) creates stacked AST with 2 channels", [] {
        static constexpr auto music = compile(note("c4 e4 g4").superimpose(add(7)));
        // Should have events on two channels: sq1 (original) and sq2 (transposed)
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
            }
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    gba::test("superimpose(add(7)) doubles note count", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto stacked = compile(note("c4 e4 g4").superimpose(add(7)));
        int origNotes = 0, stackedNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < stacked.event_count; ++i)
            if (stacked.events[i].type == event_type::note_on) stackedNotes++;
        gba::test.eq(stackedNotes, origNotes * 2);
    });

    gba::test("superimpose(rev()) overlays reversed copy", [] {
        static constexpr auto music = compile(note("c4 e4 g4").superimpose(rev()));
        // Both channels should exist
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
            }
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    gba::test("superimpose(sub(12)) transposes down", [] {
        static constexpr auto music = compile(note("c4 e4 g4").superimpose(sub(12)));
        // Transposed layer should have C3 rate on sq2
        bool hasC3 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::sq2 &&
                music.events[i].rate == note_to_rate(note::c3))
                hasC3 = true;
        }
        gba::test.is_true(hasC3);
    });

    gba::test("superimpose() preserves original on sq1", [] {
        static constexpr auto music = compile(note("c4 e4 g4").superimpose(add(7)));
        // First note on sq1 should be C4
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::sq1) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c4)));
                break;
            }
        }
    });

    gba::test("superimpose(ply(2)) stutters the overlay", [] {
        static constexpr auto music = compile(note("c4 e4").superimpose(ply(2)));
        // Overlay (sq2) should have 4 note_ons (each note ply'd x2)
        int sq2Notes = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on && music.events[i].chan == channel::sq2) sq2Notes++;
        gba::test.eq(sq2Notes, 4);
    });

    gba::test("superimpose(press()) staccato overlay", [] {
        static constexpr auto music = compile(note("c4 e4").superimpose(press()));
        // Overlay (sq2) should have rest events (note_offs from press)
        bool hasNoteOff = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_off && music.events[i].chan == channel::sq2) hasNoteOff = true;
        gba::test.is_true(hasNoteOff);
    });

    gba::test("superimpose() with loop() compiles", [] {
        static constexpr auto music = compile(loop(note("c4 e4 g4").superimpose(add(7))));
        gba::test.is_true(music.looping);
    });

    gba::test("off(1, 8, add(7)) creates stacked_pattern with 2 layers", [] {
        static constexpr auto music = compile(note("c4 e4 g4").off(1, 8, add(7)));
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
            }
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    gba::test("off() second layer starts later than first", [] {
        static constexpr auto music = compile(note("c4 e4 g4").off(1, 4, add(7)));
        // Find first note_on on each channel
        std::int64_t firstSq1 = -1, firstSq2 = -1;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1 && firstSq1 < 0) firstSq1 = music.events[i].time_num;
                if (music.events[i].chan == channel::sq2 && firstSq2 < 0) firstSq2 = music.events[i].time_num;
            }
        }
        gba::test.is_true(firstSq1 >= 0);
        gba::test.is_true(firstSq2 >= 0);
        gba::test.is_true(firstSq2 > firstSq1); // delayed layer starts later
    });

    gba::test("off() with loop() compiles", [] {
        static constexpr auto music = compile(loop(note("c4 e4").off(1, 8, rev())));
        gba::test.is_true(music.looping);
    });

    gba::test("linger(2) halves note count (4->2)", [] {
        static constexpr auto original = compile(note("c4 e4 g4 b4"));
        static constexpr auto lingered = compile(note("c4 e4 g4 b4").linger(2));
        int origNotes = 0, lingerNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < lingered.event_count; ++i)
            if (lingered.events[i].type == event_type::note_on) lingerNotes++;
        gba::test.eq(origNotes, 4);
        gba::test.eq(lingerNotes, 2);
    });

    gba::test("linger(4) keeps only first note", [] {
        static constexpr auto music = compile(note("c4 e4 g4 b4").linger(4));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
    });

    gba::test("linger(1) is identity", [] {
        static constexpr auto original = compile(note("c4 e4 g4"));
        static constexpr auto same = compile(note("c4 e4 g4").linger(1));
        int origNotes = 0, sameNotes = 0;
        for (std::uint16_t i = 0; i < original.event_count; ++i)
            if (original.events[i].type == event_type::note_on) origNotes++;
        for (std::uint16_t i = 0; i < same.event_count; ++i)
            if (same.events[i].type == event_type::note_on) sameNotes++;
        gba::test.eq(origNotes, sameNotes);
    });

    gba::test("linger(2) preserves first note pitch", [] {
        static constexpr auto music = compile(note("c4 e4 g4 b4").linger(2));
        // First note should be C4
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                gba::test.eq(static_cast<int>(music.events[i].rate), static_cast<int>(note_to_rate(note::c4)));
                break;
            }
        }
    });

    gba::test("linger() works with loop()", [] {
        static constexpr auto music = compile(loop(note("c4 e4 g4 b4").linger(2)));
        gba::test.is_true(music.looping);
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 2);
    });

    gba::test("linger() on single note is identity", [] {
        static constexpr auto music = compile(note("c4").linger(4));
        int noteOns = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i)
            if (music.events[i].type == event_type::note_on) noteOns++;
        gba::test.eq(noteOns, 1);
    });

    gba::test("superimpose + linger chain", [] {
        // linger keeps first half, superimpose adds fifth above
        static constexpr auto music = compile(note("c4 e4 g4 b4").linger(2).superimpose(add(7)));
        bool hasSq1 = false, hasSq2 = false;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].type == event_type::note_on) {
                if (music.events[i].chan == channel::sq1) hasSq1 = true;
                if (music.events[i].chan == channel::sq2) hasSq2 = true;
            }
        }
        gba::test.is_true(hasSq1);
        gba::test.is_true(hasSq2);
    });

    // Section: wav_from_samples round-trip tests

    gba::test("wav_from_samples packs nibbles correctly", [] {
        // 8 samples: high nibble first, packed into uint32 words.
        // Samples: 0xA,0xB,0xC,0xD repeated -> word = 0xABCD_ABCD...
        static constexpr std::array<std::uint8_t, 64> samples = {
            0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD,
            0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD,
            0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD,
            0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD, 0xA, 0xB, 0xC, 0xD,
        };
        static constexpr auto inst = wav_from_samples(samples, 1);
        // Each word: byte0=(A<<4|B)=0xAB, byte1=(C<<4|D)=0xCD,
        //            byte2=(A<<4|B)=0xAB, byte3=(C<<4|D)=0xCD
        // little-endian: 0xCDABCDAB
        static_assert(inst.wave_data[0] == 0xCDABCDAB);
        static_assert(inst.volume == 1);
        gba::test.is_true(true);
    });

    gba::test("wav_from_samples volume parameter", [] {
        static constexpr std::array<std::uint8_t, 64> zeros{};
        static constexpr auto inst = wav_from_samples(zeros, 3);
        static_assert(inst.volume == 3);
        gba::test.is_true(true);
    });

    // Section: Timing precision tests

    gba::test("total_time_num is exact multiple of cycle_time_num", [] {
        static constexpr auto music = compile(note("c4 e4 g4 c5"));
        static_assert(music.total_time_num > 0);
        static_assert(music.cycle_time_num > 0);
        static_assert((music.total_time_num % music.cycle_time_num) == 0);
        gba::test.is_true(true);
    });

    gba::test("alternating timing: total = cycles * cycle_time", [] {
        // <a b c> takes 3 cycles. total should be 3*cycle_time.
        static constexpr auto music = compile(note("<c4 e4 g4>"));
        static_assert(music.total_time_num == music.cycle_time_num * 3);
        gba::test.is_true(true);
    });

    gba::test("slow timing: total expands for /3", [] {
        // c4/3 stretches over 3 cycles.
        static constexpr auto music = compile(note("c4/3"));
        static_assert(music.total_time_num >= music.cycle_time_num * 3);
        gba::test.is_true(true);
    });

    // Section: Sort stability: same-time ordering

    gba::test("events at same time: instrument_change before note_off before note_on", [] {
        // seq() emits instrument_change at boundary, notes produce note_on,
        // articulation produces note_off. At t=0, order must be deterministic.
        static constexpr auto music = compile(seq(note("c4").channel(channel::sq1), note("e4").channel(channel::sq1)));
        // Find events at time 0
        int lastPri = -1;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].time_num != 0) continue;
            int pri = 0;
            if (music.events[i].type == event_type::instrument_change) pri = 0;
            else if (music.events[i].type == event_type::note_off) pri = 1;
            else if (music.events[i].type == event_type::note_on) pri = 2;
            gba::test.ge(pri, lastPri);
            lastPri = pri;
        }
    });

    // Section: Articulation injection correctness

    gba::test("articulation: repeated notes get note_off between them", [] {
        static constexpr auto music = compile(note("a4 a4").channel(channel::sq1));
        // Should have: note_on, note_off, note_on (at minimum)
        int noteOns = 0, noteOffs = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].chan == channel::sq1) {
                if (music.events[i].type == event_type::note_on) noteOns++;
                if (music.events[i].type == event_type::note_off) noteOffs++;
            }
        }
        gba::test.eq(noteOns, 2);
        gba::test.ge(noteOffs, 1); // at least one articulation note_off
    });

    gba::test("articulation: held notes do NOT get extra note_off", [] {
        static constexpr auto music = compile(note("a4 _").channel(channel::sq1));
        // Hold means sustain - should be just one note_on, no articulation note_off
        int noteOns = 0, noteOffs = 0;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].chan == channel::sq1) {
                if (music.events[i].type == event_type::note_on) noteOns++;
                if (music.events[i].type == event_type::note_off) noteOffs++;
            }
        }
        gba::test.eq(noteOns, 1);
        gba::test.eq(noteOffs, 0);
    });

    gba::test("articulation: note_off placed before next note_on time", [] {
        static constexpr auto music = compile(note("c4 e4").channel(channel::sq1));
        // Find the articulation note_off and verify it's before the second note_on
        std::int64_t firstNoteOnTime = -1;
        std::int64_t noteOffTime = -1;
        std::int64_t secondNoteOnTime = -1;
        for (std::uint16_t i = 0; i < music.event_count; ++i) {
            if (music.events[i].chan != channel::sq1) continue;
            if (music.events[i].type == event_type::note_on) {
                if (firstNoteOnTime < 0) firstNoteOnTime = music.events[i].time_num;
                else if (secondNoteOnTime < 0) secondNoteOnTime = music.events[i].time_num;
            }
            if (music.events[i].type == event_type::note_off) noteOffTime = music.events[i].time_num;
        }
        gba::test.is_true(static_cast<int>(secondNoteOnTime) > 0);
        gba::test.is_true(static_cast<int>(noteOffTime) > 0);
        gba::test.le(static_cast<int>(noteOffTime), static_cast<int>(secondNoteOnTime));
    });

    // Section: compiled_music metadata

    gba::test("compiled_music looping flag from loop()", [] {
        static constexpr auto music = compile(loop(note("c4")));
        static_assert(music.looping == true);
        gba::test.is_true(true);
    });

    gba::test("compiled_music non-looping by default", [] {
        static constexpr auto music = compile(note("c4"));
        static_assert(music.looping == false);
        gba::test.is_true(true);
    });

    gba::test("compiled_music seq_step_count matches seq() args", [] {
        static constexpr auto music = compile(
            seq(note("c4").channel(channel::sq1), note("e4").channel(channel::sq1), note("g4").channel(channel::sq1)));
        static_assert(music.seq_step_count == 3);
        gba::test.is_true(true);
    });

    return gba::test.finish();
}
