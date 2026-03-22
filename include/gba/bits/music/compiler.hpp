/// @file bits/music/compiler.hpp
/// @brief Consteval pattern compiler - flattens AST into frame-indexed events.
///
/// `compile<BPM>(pattern)` walks the AST and produces a sorted event table
/// using rational frame arithmetic for zero-drift timing. The output is a
/// `compiled_music` struct containing a flat event array and metadata.
#pragma once

#include <gba/bits/music/pattern.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace gba::music {

    // -- Event types -----------------------------------------------------

    /// @brief What kind of event to emit.
    enum class event_type : std::uint8_t {
        note_on,          ///< Trigger a note (write frequency + trigger).
        note_off,         ///< Silence the channel.
        instrument_change ///< Write new instrument registers (seq boundary).
    };

    /// @brief A single compiled music event.
    ///
    /// Events are the fundamental output of the consteval compiler. Each event
    /// represents a single PSG register write at a specific frame. Events are
    /// sorted by `time_num` and grouped into `timepoint` batches for dispatch.
    ///
    /// The `time_num` field uses rational numerator form: an event fires when
    /// `frame_counter * time_den >= time_num`.
    struct event {
        /// @brief Trigger time (rational numerator). Compare with
        /// `frame_counter * time_den` to decide when to fire.
        std::int64_t time_num{};

        /// @brief What action to perform (note_on, note_off, instrument_change).
        event_type type{};
        /// @brief Which PSG channel this event targets.
        channel chan{};

        // For note_on: PSG rate (channels 1-3) or drum preset index (noise)
        std::uint16_t rate{};

        // For noise note_on: full frequency params
        std::uint8_t noise_div_ratio{};
        bool noise_width{};
        std::uint8_t noise_shift{};
        // Noise per-drum envelope overrides
        std::uint8_t noise_env_volume{};
        std::uint8_t noise_env_step{};
        std::uint8_t noise_env_direction{};

        // For instrument_change: per-channel instrument (small ones inline)
        sq1_instrument sq1_inst{};
        sq2_instrument sq2_inst{};
        noise_instrument noise_inst{};
        // wav_instrument is stored in compiled_music::wav_instruments[] to keep
        // events compact (wav_instrument carries 32 bytes of wave RAM data).
        std::uint8_t wav_inst_idx{};

        // For alternating: which cycle this event belongs to (-1 = all cycles)
        std::int16_t cycle_mod{-1}; ///< -1 = always fire. >=0 = fire when cycle%N == this.
        std::int16_t cycle_n{1};    ///< Modulus for cycle selection.
    };

    /// @brief Group of events that share the same trigger time.
    struct timepoint {
        std::int64_t time_num{};
        std::uint16_t event_begin{};
        std::uint16_t event_end{};
    };

    /// @brief Maximum events in compiled music. Generous - the lambda-chain
    /// player template folds event data into code at compile time.
    inline constexpr std::size_t max_events = 1024;

    /// @brief Maximum distinct wav_instrument values in compiled music.
    inline constexpr std::size_t max_wav_instruments = 32;

    /// @brief Compiled music data - the output of `compile<BPM>()`.
    struct compiled_music {
        std::array<event, max_events> events{};
        std::uint16_t event_count{};
        std::array<timepoint, max_events> timepoints{};
        std::uint16_t timepoint_count{};

        /// @brief Wav instrument table - wav_instrument is stored here instead
        /// of inline in events to keep the event struct compact. Events
        /// reference entries via `event::wav_inst_idx`.
        std::array<wav_instrument, max_wav_instruments> wav_instruments{};
        std::uint8_t wav_instrument_count{};

        /// @brief Common denominator for all event times.
        std::int64_t time_den{1};

        /// @brief Total duration in frames (rational numerator / time_den).
        std::int64_t total_time_num{};

        /// @brief Duration of one base cycle in frames (rational numerator / time_den).
        std::int64_t cycle_time_num{};

        /// @brief Compile-time step count until full polymeter realignment.
        std::int64_t loop_step_count{1};

        bool looping{};

        /// @brief Number of steps in seq() (for instrument change tracking).
        std::uint8_t seq_step_count{};

        consteval void add_event(event e) {
            if (event_count >= max_events) throw "compiled_music: too many events";
            events[event_count++] = e;
        }

        /// @brief Add or deduplicate a wav_instrument, returning its index.
        consteval std::uint8_t intern_wav_instrument(const wav_instrument& inst) {
            // Deduplicate: return existing index if same instrument already stored
            for (std::uint8_t i = 0; i < wav_instrument_count; ++i)
                if (wav_instruments[i] == inst) return i;
            if (wav_instrument_count >= max_wav_instruments) throw "compiled_music: too many distinct wav instruments";
            wav_instruments[wav_instrument_count] = inst;
            return wav_instrument_count++;
        }

        /// @brief Compare two events for sorting.
        ///
        /// Deterministic same-tick ordering:
        ///   1. `time_num` ascending
        ///   2. `instrument_change` -> `note_off` -> `note_on`
        ///   3. Channel index ascending (sq1 < sq2 < wav < noise)
        static consteval bool event_less(const event& a, const event& b) {
            if (a.time_num != b.time_num) return a.time_num < b.time_num;
            constexpr auto pri = [](event_type t) consteval {
                switch (t) {
                    case event_type::instrument_change: return 0;
                    case event_type::note_off: return 1;
                    case event_type::note_on: return 2;
                }
                return 3;
            };
            auto ap = pri(a.type), bp = pri(b.type);
            if (ap != bp) return ap < bp;
            return static_cast<std::uint8_t>(a.chan) < static_cast<std::uint8_t>(b.chan);
        }

        /// @brief Consteval merge sort - O(n log n) for fast compile times.
        ///
        /// Replaces the previous O(n^2) insertion sort. Uses a scratch buffer
        /// for merging; the temporary allocation is consteval-only and has zero
        /// runtime cost.
        consteval void sort_events() {
            if (event_count <= 1) return;
            std::array<event, max_events> tmp{};
            merge_sort(events.data(), tmp.data(), 0, event_count);
        }

    private:
        /// @brief Recursive merge sort implementation.
        static consteval void merge_sort(event* arr, event* tmp, std::uint16_t lo, std::uint16_t hi) {
            if (hi - lo <= 1) return;
            if (hi - lo <= 16) {
                // Small-range insertion sort (avoids recursion overhead)
                for (auto i = lo + 1; i < hi; ++i) {
                    auto key = arr[i];
                    auto j = i;
                    while (j > lo && event_less(key, arr[j - 1])) {
                        arr[j] = arr[j - 1];
                        --j;
                    }
                    arr[j] = key;
                }
                return;
            }
            auto mid = static_cast<std::uint16_t>(lo + (hi - lo) / 2);
            merge_sort(arr, tmp, lo, mid);
            merge_sort(arr, tmp, mid, hi);
            // Merge [lo..mid) and [mid..hi) into tmp, then copy back
            std::uint16_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) tmp[k++] = event_less(arr[j], arr[i]) ? arr[j++] : arr[i++];
            while (i < mid) tmp[k++] = arr[i++];
            while (j < hi) tmp[k++] = arr[j++];
            for (auto x = lo; x < hi; ++x) arr[x] = tmp[x];
        }

    public:
        /// @brief Inject note_off events for articulation between consecutive note_on events.
        ///
        /// For each channel, tracks the last `note_on` index and injects a `note_off`
        /// one frame before the next `note_on` on the same channel (unless a `note_off`
        /// already exists between them). This creates audible separation for repeated
        /// notes (e.g. `a4 a4`) while leaving holds (`a4 _`) untouched.
        ///
        /// The note_off is placed one frame early (`time_num - time_den`) so the PSG
        /// trigger at `env_volume=0` fires in a separate VBlank from the re-attack.
        /// Falls back to same-frame placement when notes are exactly 1 frame apart.
        ///
        /// Complexity: O(n) - single pass with 4-entry per-channel state.
        consteval void inject_articulation_note_offs() {
            if (event_count == 0) return;

            // Per-channel: index of the last note_on that needs a trailing note_off.
            // -1 means no pending note_on on that channel.
            std::array<std::int32_t, 4> lastNoteOn = {-1, -1, -1, -1};

            // Collect injected note_offs in a separate buffer, merge after.
            std::array<event, max_events> injected{};
            std::uint16_t injectedCount = 0;

            for (std::uint16_t i = 0; i < event_count; ++i) {
                const auto& ev = events[i];
                auto ch = static_cast<std::uint8_t>(ev.chan);

                if (ev.type == event_type::note_on) {
                    // If there's a pending note_on on this channel, inject note_off
                    if (lastNoteOn[ch] >= 0) {
                        auto noteOffTime = ev.time_num - time_den;
                        if (noteOffTime <= events[lastNoteOn[ch]].time_num)
                            noteOffTime = ev.time_num; // fallback: same-frame

                        if (injectedCount >= max_events)
                            throw "compiled_music: too many events after articulation injection";
                        event noteOff{};
                        noteOff.time_num = noteOffTime;
                        noteOff.type = event_type::note_off;
                        noteOff.chan = ev.chan;
                        injected[injectedCount++] = noteOff;
                    }
                    lastNoteOn[ch] = static_cast<std::int32_t>(i);
                } else if (ev.type == event_type::note_off) {
                    // Explicit note_off clears the pending state
                    lastNoteOn[ch] = -1;
                }
            }

            // Append injected note_offs to the event array
            for (std::uint16_t i = 0; i < injectedCount; ++i) {
                if (event_count >= max_events) throw "compiled_music: too many events after articulation injection";
                events[event_count++] = injected[i];
            }
        }

        /// @brief Build batches of same-time events for per-tick lambda dispatch.
        consteval void build_timepoints() {
            timepoint_count = 0;
            if (event_count == 0) return;

            std::uint16_t begin = 0;
            while (begin < event_count) {
                auto t = events[begin].time_num;
                std::uint16_t end = begin + 1;
                while (end < event_count && events[end].time_num == t) end++;

                if (timepoint_count >= max_events) throw "compiled_music: too many timepoints";
                timepoints[timepoint_count++] = {
                    .time_num = t,
                    .event_begin = begin,
                    .event_end = end,
                };
                begin = end;
            }
        }

        /// @brief Resolve cycle-gated events (`cycle_mod`) into concrete events over the loop span.
        consteval void resolve_cycle_events() {
            if (event_count == 0) return;
            if (cycle_time_num <= 0) throw "compiled_music: cycle_time_num must be > 0";

            std::uint16_t out = 0;
            for (std::uint16_t i = 0; i < event_count; ++i) {
                auto e = events[i];
                if (e.cycle_mod >= 0) {
                    if (e.cycle_n <= 0) throw "compiled_music: cycle_n must be > 0";
                    auto cycleIdx = static_cast<std::int64_t>(e.time_num / cycle_time_num);
                    if ((cycleIdx % e.cycle_n) != e.cycle_mod) continue;
                    e.cycle_mod = -1;
                    e.cycle_n = 1;
                }

                if (out >= max_events) throw "compiled_music: too many resolved events";
                events[out++] = e;
            }
            event_count = out;
        }

        /// @brief Repeat compiled span until all cycle modulators can be resolved in one loop.
        consteval void expand_for_cycle_modulation() {
            if (event_count == 0) return;
            if (cycle_time_num <= 0 || total_time_num <= 0) throw "compiled_music: invalid cycle/total time";
            if ((total_time_num % cycle_time_num) != 0)
                throw "compiled_music: total_time_num must be a multiple of cycle_time_num";

            auto gcd64 = [](std::int64_t a, std::int64_t b) consteval {
                auto aa = a < 0 ? -a : a;
                auto bb = b < 0 ? -b : b;
                while (bb) {
                    auto t = bb;
                    bb = aa % bb;
                    aa = t;
                }
                return aa;
            };
            auto lcm64 = [&](std::int64_t a, std::int64_t b) consteval {
                if (a <= 0 || b <= 0) throw "compiled_music: invalid lcm input";
                return a / gcd64(a, b) * b;
            };

            std::int64_t baseCycles = total_time_num / cycle_time_num;
            std::int64_t cyclePeriod = 1;
            for (std::uint16_t i = 0; i < event_count; ++i)
                if (events[i].cycle_mod >= 0) cyclePeriod = lcm64(cyclePeriod, events[i].cycle_n);

            auto targetCycles = lcm64(baseCycles, cyclePeriod);
            if (targetCycles == baseCycles) return;

            auto repeats = targetCycles / baseCycles;
            auto originalCount = event_count;
            auto originalTotal = total_time_num;

            for (std::int64_t rep = 1; rep < repeats; ++rep) {
                for (std::uint16_t i = 0; i < originalCount; ++i) {
                    auto e = events[i];
                    e.time_num += originalTotal * rep;
                    if (event_count >= max_events) throw "compiled_music: too many expanded events";
                    events[event_count++] = e;
                }
            }

            total_time_num = originalTotal * repeats;
            loop_step_count *= repeats;
        }

        /// @brief Final post-processing: sort, resolve cycles, inject articulation, build timepoints.
        ///
        /// Called at the end of every `compile_with_tempo` overload. Consolidates the
        /// 7-step finalization sequence that was previously duplicated 3 times.
        consteval void finalize() {
            sort_events();
            expand_for_cycle_modulation();
            resolve_cycle_events();
            sort_events();
            inject_articulation_note_offs();
            sort_events();
            build_timepoints();
        }
    };

    // -- Bjorklund / Euclidean -------------------------------------------

    namespace compile_detail {

        consteval std::int64_t to_time_num(rational t, std::int64_t den) {
            return (t.num * den) / t.den;
        }

        consteval std::int64_t lcm(std::int64_t a, std::int64_t b);

        consteval int timeline_value(const parsed_pattern& ast, std::uint16_t valueNodeIdx) {
            auto value = static_cast<int>(ast.nodes[valueNodeIdx].note_value);
            if (value <= 0) throw "timeline: value must be > 0";
            return value;
        }

        // Solve c % a == modA and c % b == modB. Returns {-1,0} if no solution.
        consteval std::pair<int, int> solve_congruence(int a, int modA, int b, int modB) {
            if (a <= 0 || b <= 0) throw "solve_congruence: modulus must be > 0";
            auto n = static_cast<int>(lcm(a, b));
            for (int x = 0; x < n; ++x)
                if ((x % a) == modA && (x % b) == modB) return {x, n};
            return {-1, 0};
        }

        consteval int lcm_int(int a, int b) {
            if (a <= 0 || b <= 0) throw "lcm_int: values must be > 0";
            return static_cast<int>(lcm(a, b));
        }

        consteval int node_step_count(const parsed_pattern& ast, std::uint16_t nodeIdx) {
            const auto& node = ast.nodes[nodeIdx];
            switch (node.type) {
                case ast_type::note_literal: return 1;
                case ast_type::sequence:
                case ast_type::subdivision: {
                    int total = 0;
                    for (std::uint8_t i = 0; i < node.child_count; ++i) total += node.weights[i];
                    return total > 0 ? total : 1;
                }
                case ast_type::alternating: {
                    int childLcm = 1;
                    int totalWeight = 0;
                    for (std::uint8_t i = 0; i < node.child_count; ++i) {
                        childLcm = lcm_int(childLcm, node_step_count(ast, node.children[i]));
                        totalWeight += (node.weights[i] > 0 ? node.weights[i] : 1);
                    }
                    return childLcm * (totalWeight > 0 ? totalWeight : 1);
                }
                case ast_type::stacked:
                case ast_type::polymetric: {
                    if (node.child_count == 0) return 1;
                    int childLcm = 1;
                    for (std::uint8_t i = 0; i < node.child_count; ++i)
                        childLcm = lcm_int(childLcm, node_step_count(ast, node.children[i]));
                    return node.type == ast_type::polymetric && node.polymeter_steps > 0
                               ? static_cast<int>(node.polymeter_steps)
                               : childLcm;
                }
                case ast_type::fast: return node.child_count > 0 ? node_step_count(ast, node.children[0]) : 1;
                case ast_type::slow: {
                    int child = node.child_count > 0 ? node_step_count(ast, node.children[0]) : 1;
                    int n = node.modifier_is_timeline
                                ? 1
                                : (node.modifier_num > 0 ? static_cast<int>(node.modifier_num) : 1);
                    // For fractional slow (e.g. /1.5 = {3,2}), multiply steps by ceiling
                    return child * n;
                }
                case ast_type::replicate: {
                    int child = node.child_count > 0 ? node_step_count(ast, node.children[0]) : 1;
                    int n = node.modifier_num > 0 ? static_cast<int>(node.modifier_num) : 1;
                    return child * n;
                }
                case ast_type::euclidean: return node.euclidean_n > 0 ? static_cast<int>(node.euclidean_n) : 1;
            }
            return 1;
        }

        consteval int polymeter_loop_steps(const parsed_pattern& ast, std::uint16_t nodeIdx) {
            const auto& node = ast.nodes[nodeIdx];
            int result = 1;

            if (node.type == ast_type::polymetric && node.polymeter_steps > 0) {
                result = static_cast<int>(node.polymeter_steps);
                for (std::uint8_t i = 0; i < node.child_count; ++i)
                    result = lcm_int(result, node_step_count(ast, node.children[i]));
            }

            for (std::uint8_t i = 0; i < node.child_count; ++i)
                result = lcm_int(result, polymeter_loop_steps(ast, node.children[i]));

            return result;
        }

        consteval int steps_until_loop(const parsed_pattern& ast) {
            int baseSteps = node_step_count(ast, ast.root);
            int loopSteps = polymeter_loop_steps(ast, ast.root);
            int merged = lcm_int(baseSteps, loopSteps);
            if (merged <= 0) throw "steps_until_loop: invalid loop step count";
            return merged;
        }

        consteval int cycles_until_loop(const parsed_pattern& ast) {
            int baseSteps = node_step_count(ast, ast.root);
            int steps = steps_until_loop(ast);
            if (baseSteps <= 0 || (steps % baseSteps) != 0)
                throw "cycles_until_loop: invalid base/loop step relationship";
            return steps / baseSteps;
        }

        /// @brief Bjorklund algorithm: distribute k pulses over n steps.
        ///
        /// Computes a Euclidean rhythm pattern where `k` pulses are distributed
        /// as evenly as possible over `n` steps, optionally rotated by `rotation`.
        ///
        /// @param k Number of pulses (1 <= k <= n).
        /// @param n Number of steps (1 <= n <= 64).
        /// @param rotation Circular rotation offset (default 0).
        /// @return Boolean array where `true` = pulse, `false` = rest.
        consteval std::array<bool, 64> bjorklund(int k, int n, int rotation = 0) {
            if (n <= 0 || n > 64) throw "bjorklund: n must be 1-64";
            if (k < 0 || k > n) throw "bjorklund: k must be 0-n";

            std::array<bool, 64> result{};
            if (k == 0) return result;
            if (k == n) {
                for (int i = 0; i < n; ++i) result[i] = true;
                return result;
            }

            // Flattened 1D storage: groups[i] starts at offset[i], length groupLen[i].
            // Max total elements = n (each group element is one bool).
            constexpr int maxTotal = 64;
            std::array<bool, maxTotal * 2> flat{}; // generous scratch
            std::array<int, 64> offset{};
            std::array<int, 64> groupLen{};

            // Initial: k groups of [true], (n-k) groups of [false]
            for (int i = 0; i < n; ++i) {
                offset[i] = i;
                groupLen[i] = 1;
                flat[i] = (i < k);
            }

            int front = k;
            int back = n - k;
            int nextFree = n; // next free slot in flat[]

            while (back > 1) {
                int pairs = (front < back) ? front : back;

                for (int i = 0; i < pairs; ++i) {
                    int backIdx = front + i;
                    // Allocate new merged group in flat[]
                    int newOff = nextFree;
                    int newLen = groupLen[i] + groupLen[backIdx];
                    // Copy front group
                    for (int j = 0; j < groupLen[i]; ++j) flat[nextFree++] = flat[offset[i] + j];
                    // Copy back group
                    for (int j = 0; j < groupLen[backIdx]; ++j) flat[nextFree++] = flat[offset[backIdx] + j];
                    offset[i] = newOff;
                    groupLen[i] = newLen;
                }

                // Compact unpaired groups
                int newBack = 0;
                int unpairedFront = front - pairs;
                // front[pairs..front-1] are already at those indices
                newBack += unpairedFront;
                int unpairedBack = back - pairs;
                for (int i = 0; i < unpairedBack; ++i) {
                    int srcIdx = front + pairs + i;
                    int dstIdx = pairs + unpairedFront + i;
                    if (srcIdx != dstIdx) {
                        offset[dstIdx] = offset[srcIdx];
                        groupLen[dstIdx] = groupLen[srcIdx];
                    }
                }
                newBack += unpairedBack;

                front = pairs;
                back = newBack;
            }

            // Flatten groups into result
            int groupCount = front + back;
            std::array<bool, 64> raw{};
            int pos = 0;
            for (int i = 0; i < groupCount && pos < n; ++i)
                for (int j = 0; j < groupLen[i] && pos < n; ++j) raw[pos++] = flat[offset[i] + j];

            // Apply rotation
            if (rotation != 0) {
                int r = ((rotation % n) + n) % n;
                for (int i = 0; i < n; ++i) result[(i + r) % n] = raw[i];
            } else {
                result = raw;
            }

            return result;
        }

        // -- AST walker --------------------------------------------------

        /// @brief Compile-time context threaded through the recursive AST walker.
        ///
        /// Carries the AST, output buffer, current channel, timing info,
        /// and per-layer overrides. Passed by value so each recursive branch
        /// can modify its own copy (e.g. stacked nodes change `chan`).
        struct walk_context {
            const parsed_pattern* ast;            ///< Source AST being walked.
            compiled_music* music;                ///< Output event buffer.
            channel chan;                         ///< Current PSG channel for this branch.
            rational cycle_frames;                ///< Total frames in one cycle.
            const layer_cfg* layer_overrides{};   ///< Per-layer channel overrides (from `.channels()`).
            std::uint8_t layer_override_count{0}; ///< Number of valid entries in `layer_overrides`.
        };

        /// @brief Recursively walk AST node and emit events.
        /// @param start Start time (rational numerator, relative to time_den).
        /// @param duration Duration of this node (rational).
        consteval void walk_node(walk_context ctx, std::uint16_t nodeIdx, rational start, rational duration) {
            const auto& node = ctx.ast->nodes[nodeIdx];

            switch (node.type) {
                case ast_type::note_literal: {
                    if (node.note_value == note::rest) {
                        // Emit note_off at start
                        event e{};
                        e.time_num = to_time_num(start, ctx.music->time_den);
                        e.type = event_type::note_off;
                        e.chan = ctx.chan;
                        ctx.music->add_event(e);
                    } else if (node.note_value == note::hold) {
                        // Hold/tie: emit nothing - PSG channel sustains its current
                        // frequency and envelope without a retrigger pulse.
                        // This preserves smooth legato across step boundaries and
                        // prevents the volume envelope from restarting.
                    } else if (ctx.chan == channel::noise && is_drum(node.note_value)) {
                        // Drum hit
                        const auto& preset = note_to_drum(node.note_value);
                        event e{};
                        e.time_num = to_time_num(start, ctx.music->time_den);
                        e.type = event_type::note_on;
                        e.chan = ctx.chan;
                        e.noise_div_ratio = preset.div_ratio;
                        e.noise_width = preset.width;
                        e.noise_shift = preset.shift;
                        e.noise_env_volume = preset.env_volume;
                        e.noise_env_step = preset.env_step;
                        e.noise_env_direction = preset.env_direction;
                        ctx.music->add_event(e);
                    } else if (is_chromatic(node.note_value)) {
                        // Pitched note
                        if (ctx.chan == channel::noise)
                            throw "compile: chromatic notes cannot target the noise channel - use drum presets (bd, "
                                  "sd, hh, ...) with s(), or use note().channel(channel::sq1/sq2/wav) for pitched "
                                  "content";
                        event e{};
                        e.time_num = to_time_num(start, ctx.music->time_den);
                        e.type = event_type::note_on;
                        e.chan = ctx.chan;
                        e.rate = (ctx.chan == channel::wav) ? note_to_wav_rate(node.note_value)
                                                            : note_to_rate(node.note_value);
                        ctx.music->add_event(e);
                    }
                    break;
                }

                case ast_type::sequence:
                case ast_type::subdivision: {
                    // Divide duration among children proportionally by weight
                    int totalWeight = 0;
                    for (std::uint8_t i = 0; i < node.child_count; ++i) totalWeight += node.weights[i];

                    if (totalWeight == 0) break;

                    rational pos = start;
                    for (std::uint8_t i = 0; i < node.child_count; ++i) {
                        rational childDur = duration * rational{node.weights[i], totalWeight};
                        walk_node(ctx, node.children[i], pos, childDur);
                        pos = pos + childDur;
                    }
                    break;
                }

                case ast_type::alternating: {
                    // Compute total weight for cycle modulus.
                    // @N means the element spans N consecutive cycle slots - it plays
                    // ONCE on the first slot and sustains (no retrigger) on the rest.
                    int totalWeight = 0;
                    for (std::uint8_t i = 0; i < node.child_count; ++i)
                        totalWeight += (node.weights[i] > 0 ? node.weights[i] : 1);

                    int slot = 0;
                    for (std::uint8_t i = 0; i < node.child_count; ++i) {
                        int w = (node.weights[i] > 0 ? node.weights[i] : 1);
                        // Walk child once, tagged with the first slot
                        auto beforeCount = ctx.music->event_count;
                        walk_node(ctx, node.children[i], start, duration);
                        for (auto j = beforeCount; j < ctx.music->event_count; ++j) {
                            ctx.music->events[j].cycle_mod = static_cast<std::int16_t>(slot);
                            ctx.music->events[j].cycle_n = static_cast<std::int16_t>(totalWeight);
                        }
                        slot += w; // skip over sustain slots
                    }
                    break;
                }

                case ast_type::stacked: {
                    // Each child is a layer on a different channel.
                    // Use layer_overrides if provided, otherwise auto-assign: sq1, sq2, wav, noise.
                    constexpr channel order[] = {channel::sq1, channel::sq2, channel::wav, channel::noise};
                    if (node.child_count > 4) throw "stacked: too many parallel layers (max 4 PSG channels)";
                    for (std::uint8_t i = 0; i < node.child_count; ++i) {
                        auto childCtx = ctx;
                        if (ctx.layer_overrides && i < ctx.layer_override_count) {
                            childCtx.chan = static_cast<channel>(ctx.layer_overrides[i].assigned_channel);
                        } else {
                            childCtx.chan = order[i];
                        }
                        // Clear overrides so nested stacked nodes use default auto-assignment.
                        childCtx.layer_overrides = nullptr;
                        childCtx.layer_override_count = 0;
                        walk_node(childCtx, node.children[i], start, duration);
                    }
                    break;
                }

                case ast_type::fast: {
                    if (node.child_count == 0) break;
                    if (node.modifier_is_timeline) {
                        if (node.child_count < 2) throw "fast timeline: missing timeline node";
                        const auto& timelineNode = ctx.ast->nodes[node.children[1]];
                        if (timelineNode.type != ast_type::alternating)
                            throw "fast timeline: expected alternating timeline node";
                        if (timelineNode.child_count == 0) throw "fast timeline: empty timeline";

                        for (std::uint8_t ti = 0; ti < timelineNode.child_count; ++ti) {
                            int n = timeline_value(*ctx.ast, timelineNode.children[ti]);
                            rational stepDur = duration / rational{n};
                            auto beforeCount = ctx.music->event_count;
                            for (int i = 0; i < n; ++i)
                                walk_node(ctx, node.children[0], start + stepDur * rational{i}, stepDur);
                            for (auto j = beforeCount; j < ctx.music->event_count; ++j) {
                                ctx.music->events[j].cycle_mod = static_cast<std::int16_t>(ti);
                                ctx.music->events[j].cycle_n = static_cast<std::int16_t>(timelineNode.child_count);
                            }
                        }
                    } else {
                        // modifier is rational: num/den. E.g. *2.75 = {11, 4}.
                        rational modifier{node.modifier_num, node.modifier_den};
                        if (modifier.num <= 0) modifier = rational{1, 1};

                        const auto& childNode = ctx.ast->nodes[node.children[0]];

                        // Special case: fast on alternating -- advance through children.
                        // <a b c d>*2 = cycle 0: a b, cycle 1: c d (NOT a,a,b,b,c,c,d,d).
                        // Respects @N weights: <a@2 b c>*2 expands to slots [a,hold,b,c],
                        // giving cycle 0: a (sustains); cycle 1: b,c.
                        if (childNode.type == ast_type::alternating && modifier.den == 1 && childNode.child_count > 0) {
                            int n = static_cast<int>(modifier.num);

                            // Expand weighted children into a flat slot array.
                            // isAttack marks the first slot of each element; subsequent
                            // weight slots are holds (no retrigger, note sustains).
                            constexpr int maxSlots = max_children * 4;
                            std::array<std::uint16_t, maxSlots> slots{};
                            std::array<bool, maxSlots> isAttack{};
                            int totalSlots = 0;
                            for (std::uint8_t i = 0; i < childNode.child_count; ++i) {
                                int w = (childNode.weights[i] > 0 ? childNode.weights[i] : 1);
                                for (int wi = 0; wi < w && totalSlots < maxSlots; ++wi) {
                                    slots[totalSlots] = childNode.children[i];
                                    isAttack[totalSlots] = (wi == 0); // only first is an attack
                                    totalSlots++;
                                }
                            }

                            int groups = lcm_int(totalSlots, n) / n;
                            rational stepDur = duration / rational{n};

                            for (int c = 0; c < groups; ++c) {
                                for (int r = 0; r < n; ++r) {
                                    int slotIdx = (c * n + r) % totalSlots;
                                    if (!isAttack[slotIdx]) continue; // hold slot -- sustain, no events
                                    auto beforeCount = ctx.music->event_count;
                                    walk_node(ctx, slots[slotIdx], start + stepDur * rational{r}, stepDur);
                                    for (auto j = beforeCount; j < ctx.music->event_count; ++j) {
                                        ctx.music->events[j].cycle_mod = static_cast<std::int16_t>(c);
                                        ctx.music->events[j].cycle_n = static_cast<std::int16_t>(groups);
                                    }
                                }
                            }
                        } else {
                            // General case: play the child N times within the duration.
                            int wholeCopies = static_cast<int>(modifier.num / modifier.den);
                            int remainder = static_cast<int>(modifier.num % modifier.den);
                            rational stepDur = duration / modifier;
                            for (int i = 0; i < wholeCopies; ++i) {
                                walk_node(ctx, node.children[0], start + stepDur * rational{i}, stepDur);
                            }
                            if (remainder > 0) {
                                rational partialDur = duration - stepDur * rational{wholeCopies};
                                walk_node(ctx, node.children[0], start + stepDur * rational{wholeCopies}, partialDur);
                            }
                        }
                    }
                    break;
                }

                case ast_type::slow: {
                    if (node.child_count == 0) break;
                    if (node.modifier_is_timeline) {
                        if (node.child_count < 2) throw "slow timeline: missing timeline node";
                        const auto& timelineNode = ctx.ast->nodes[node.children[1]];
                        if (timelineNode.type != ast_type::alternating)
                            throw "slow timeline: expected alternating timeline node";
                        if (timelineNode.child_count == 0) throw "slow timeline: empty timeline";

                        for (std::uint8_t ti = 0; ti < timelineNode.child_count; ++ti) {
                            int n = timeline_value(*ctx.ast, timelineNode.children[ti]);
                            auto [mod, modN] = solve_congruence(static_cast<int>(timelineNode.child_count),
                                                                static_cast<int>(ti), n, 0);
                            if (modN == 0) continue;

                            auto beforeCount = ctx.music->event_count;
                            walk_node(ctx, node.children[0], start, duration);
                            for (auto j = beforeCount; j < ctx.music->event_count; ++j) {
                                ctx.music->events[j].cycle_mod = static_cast<std::int16_t>(mod);
                                ctx.music->events[j].cycle_n = static_cast<std::int16_t>(modN);
                            }
                        }
                    } else {
                        rational modifier{node.modifier_num, node.modifier_den};
                        if (modifier.num <= 0) modifier = rational{1, 1};
                        // Real Strudel-style slow: stretch the child across N cycles
                        // instead of merely gating playback to every Nth cycle.
                        auto stretchedDuration = duration * modifier;
                        auto stretchedEnd = to_time_num(start + stretchedDuration, ctx.music->time_den);
                        if (stretchedEnd > ctx.music->total_time_num) ctx.music->total_time_num = stretchedEnd;
                        walk_node(ctx, node.children[0], start, stretchedDuration);
                    }
                    break;
                }

                case ast_type::replicate: {
                    if (node.child_count == 0) break;
                    rational modifier{node.modifier_num, node.modifier_den};
                    if (modifier.num <= 0) modifier = rational{1, 1};
                    // For integer replicate, simple iteration. For fractional, same as fast.
                    int wholeCopies = static_cast<int>(modifier.num / modifier.den);
                    int remainder = static_cast<int>(modifier.num % modifier.den);
                    rational stepDur = duration / modifier;
                    for (int i = 0; i < wholeCopies; ++i) {
                        walk_node(ctx, node.children[0], start + stepDur * rational{i}, stepDur);
                    }
                    if (remainder > 0) {
                        rational partialDur = duration - stepDur * rational{wholeCopies};
                        walk_node(ctx, node.children[0], start + stepDur * rational{wholeCopies}, partialDur);
                    }
                    break;
                }

                case ast_type::euclidean: {
                    if (node.child_count == 0) break;
                    auto pulses = bjorklund(node.euclidean_k, node.euclidean_n, node.euclidean_r);
                    rational stepDur = duration / rational{node.euclidean_n};
                    for (int i = 0; i < node.euclidean_n; ++i) {
                        if (pulses[i]) {
                            walk_node(ctx, node.children[0], start + stepDur * rational{i}, stepDur);
                        } else {
                            // Rest
                            event e{};
                            e.time_num = to_time_num(start + stepDur * rational{i}, ctx.music->time_den);
                            e.type = event_type::note_off;
                            e.chan = ctx.chan;
                            ctx.music->add_event(e);
                        }
                    }
                    break;
                }

                case ast_type::polymetric: {
                    // Play all layers simultaneously over the same duration.
                    for (std::uint8_t i = 0; i < node.child_count; ++i) {
                        walk_node(ctx, node.children[i], start, duration);
                    }
                    break;
                }
            }
        }

        /// @brief Compute the LCM of two int64 values.
        consteval std::int64_t lcm(std::int64_t a, std::int64_t b) {
            if (a == 0 || b == 0) return 0;
            auto aa = a < 0 ? -a : a;
            auto bb = b < 0 ? -b : b;
            auto g = aa;
            auto tmp = bb;
            while (tmp) {
                auto t = tmp;
                tmp = g % tmp;
                g = t;
            }
            return aa / g * bb;
        }

        /// @brief Compile a single channel's pattern into events.
        consteval void compile_channel(compiled_music& music, const parsed_pattern& ast, channel chan,
                                       rational cycleFrames, rational timeOffset, const layer_cfg* overrides = nullptr,
                                       std::uint8_t overrideCount = 0, rational patternShift = rational{}) {
            // Set time_den to the cycle denominator (will be unified later)
            auto den = lcm(music.time_den, cycleFrames.den);
            // Scale existing events to new denominator
            if (den != music.time_den) {
                auto scale = den / music.time_den;
                for (std::uint16_t i = 0; i < music.event_count; ++i) music.events[i].time_num *= scale;
                music.total_time_num *= scale;
                music.cycle_time_num *= scale;
                music.time_den = den;
            }

            walk_context ctx{&ast, &music, chan, cycleFrames, overrides, overrideCount};

            // Normalize start/duration to use music.time_den
            rational start{(timeOffset.num * music.time_den) / timeOffset.den, music.time_den};
            rational dur{(cycleFrames.num * music.time_den) / cycleFrames.den, music.time_den};

            // Apply pattern-level time shift (from .early()/.late())
            if (patternShift.num != 0) {
                auto shiftFrames = patternShift * cycleFrames;
                start = start + rational{(shiftFrames.num * music.time_den) / shiftFrames.den, music.time_den};
            }

            walk_node(ctx, ast.root, start, dur);
        }

    } // namespace compile_detail

    // -- Public compile API ----------------------------------------------

    /// @brief Implementation: Compile a single pattern with explicit tempo.
    template<tempo Tempo>
    consteval compiled_music compile_with_tempo(const pattern& pat) {
        compiled_music music{};
        auto cycleFrames = Tempo.frames_per_cycle();
        music.time_den = cycleFrames.den;
        music.cycle_time_num = cycleFrames.num;

        auto loopCycles = compile_detail::cycles_until_loop(pat.ast);
        music.loop_step_count = compile_detail::steps_until_loop(pat.ast);
        music.total_time_num = cycleFrames.num * loopCycles;

        // Emit instrument_change at t=0 for per-layer overrides.
        if (pat.has_layer_overrides()) {
            for (std::uint8_t i = 0; i < pat.m_layerOverrideCount; ++i) {
                event instEv{};
                instEv.time_num = 0;
                instEv.type = event_type::instrument_change;
                instEv.chan = static_cast<channel>(pat.m_layerOverrides[i].assigned_channel);
                instEv.sq1_inst = pat.m_layerOverrides[i].sq1_inst;
                instEv.sq2_inst = pat.m_layerOverrides[i].sq2_inst;
                instEv.wav_inst_idx = music.intern_wav_instrument(pat.m_layerOverrides[i].wav_inst);
                instEv.noise_inst = pat.m_layerOverrides[i].noise_inst;
                music.add_event(instEv);
            }
        }

        for (int cycle = 0; cycle < loopCycles; ++cycle) {
            auto offset = cycleFrames * rational{cycle};
            if (pat.is_stacked()) {
                // Multi-layer from commas in <>: the walker handles stacked nodes.
                // Pass layer overrides so channels are correctly assigned.
                compile_detail::compile_channel(music, pat.ast, channel::sq1, cycleFrames, offset,
                                                pat.has_layer_overrides() ? pat.m_layerOverrides.data() : nullptr,
                                                pat.m_layerOverrideCount, pat.m_timeShift);
            } else {
                // Single-layer: use assigned channel or default to sq1
                auto chan = pat.has_channel() ? pat.get_channel() : channel::sq1;
                compile_detail::compile_channel(music, pat.ast, chan, cycleFrames, offset, nullptr, 0, pat.m_timeShift);
            }
        }

        music.finalize();
        return music;
    }

    /// @brief Implementation: Compile a stack of simultaneous patterns with explicit tempo.
    template<tempo Tempo>
    consteval compiled_music compile_with_tempo(const stacked_pattern& sp) {
        compiled_music music{};
        auto cycleFrames = Tempo.frames_per_cycle();
        music.time_den = cycleFrames.den;
        music.cycle_time_num = cycleFrames.num;

        // Resolve channels (auto-assign untagged layers)
        auto resolved = sp;
        resolved.resolve_channels();

        int loopCycles = 1;
        int loopSteps = 1;
        for (std::uint8_t i = 0; i < resolved.layer_count; ++i) {
            loopCycles = compile_detail::lcm_int(loopCycles, compile_detail::cycles_until_loop(resolved.layers[i].ast));
            loopSteps = compile_detail::lcm_int(loopSteps, compile_detail::steps_until_loop(resolved.layers[i].ast));
        }
        music.loop_step_count = loopSteps;
        music.total_time_num = cycleFrames.num * loopCycles;

        for (int cycle = 0; cycle < loopCycles; ++cycle) {
            auto offset = cycleFrames * rational{cycle};
            for (std::uint8_t i = 0; i < resolved.layer_count; ++i) {
                compile_detail::compile_channel(
                    music, resolved.layers[i].ast, resolved.layers[i].get_channel(), cycleFrames, offset,
                    resolved.layers[i].has_layer_overrides() ? resolved.layers[i].m_layerOverrides.data() : nullptr,
                    resolved.layers[i].m_layerOverrideCount, resolved.layers[i].m_timeShift);
            }
        }

        music.finalize();
        return music;
    }

    /// @brief Implementation: Compile a sequential arrangement with explicit tempo.
    template<tempo Tempo>
    consteval compiled_music compile_with_tempo(const sequential_pattern& sp) {
        compiled_music music{};
        auto cycleFrames = Tempo.frames_per_cycle();
        music.time_den = cycleFrames.den;
        music.cycle_time_num = cycleFrames.num;
        music.looping = sp.looping;
        music.seq_step_count = sp.step_count;
        int seqLoopSteps = 0;

        rational offset{0};
        for (std::uint8_t s = 0; s < sp.step_count; ++s) {
            auto step = sp.steps[s];
            step.resolve_channels();

            int stepLoopCycles = 1;
            int stepLoopSteps = 1;
            for (std::uint8_t l = 0; l < step.layer_count; ++l) {
                stepLoopCycles = compile_detail::lcm_int(stepLoopCycles,
                                                         compile_detail::cycles_until_loop(step.layers[l].ast));
                stepLoopSteps = compile_detail::lcm_int(stepLoopSteps,
                                                        compile_detail::steps_until_loop(step.layers[l].ast));
            }
            seqLoopSteps += stepLoopSteps;

            // Emit instrument_change events at seq boundaries.
            // For layers whose AST root is a stacked node (inline commas in <>),
            // emit one instrument_change per auto-assigned channel so that ALL
            // PSG voices are properly initialised - not just the outermost one.
            // When layer overrides are present, use override channels/instruments.
            constexpr channel chan_order[] = {channel::sq1, channel::sq2, channel::wav, channel::noise};
            for (std::uint8_t l = 0; l < step.layer_count; ++l) {
                const auto& layer = step.layers[l];
                auto instTimeNum = (offset.num * music.time_den) / offset.den;

                if (layer.has_layer_overrides()) {
                    // Per-layer overrides: emit one instrument_change per override.
                    for (std::uint8_t ci = 0; ci < layer.m_layerOverrideCount; ++ci) {
                        event instEv{};
                        instEv.time_num = instTimeNum;
                        instEv.type = event_type::instrument_change;
                        instEv.chan = static_cast<channel>(layer.m_layerOverrides[ci].assigned_channel);
                        instEv.sq1_inst = layer.m_layerOverrides[ci].sq1_inst;
                        instEv.sq2_inst = layer.m_layerOverrides[ci].sq2_inst;
                        instEv.wav_inst_idx = music.intern_wav_instrument(layer.m_layerOverrides[ci].wav_inst);
                        instEv.noise_inst = layer.m_layerOverrides[ci].noise_inst;
                        music.add_event(instEv);
                    }
                } else if (layer.ast.nodes[layer.ast.root].type == ast_type::stacked) {
                    // Multi-voice inline: emit instrument_change for each child channel.
                    const auto& stackNode = layer.ast.nodes[layer.ast.root];
                    for (std::uint8_t ci = 0; ci < stackNode.child_count && ci < 4; ++ci) {
                        event instEv{};
                        instEv.time_num = instTimeNum;
                        instEv.type = event_type::instrument_change;
                        instEv.chan = chan_order[ci];
                        instEv.sq1_inst = layer.sq1_inst;
                        instEv.sq2_inst = layer.sq2_inst;
                        instEv.wav_inst_idx = music.intern_wav_instrument(layer.wav_inst);
                        instEv.noise_inst = layer.noise_inst;
                        music.add_event(instEv);
                    }
                } else {
                    // Single-channel layer: standard path.
                    event instEv{};
                    instEv.time_num = instTimeNum;
                    instEv.type = event_type::instrument_change;
                    instEv.chan = layer.get_channel();
                    instEv.sq1_inst = layer.sq1_inst;
                    instEv.sq2_inst = layer.sq2_inst;
                    instEv.wav_inst_idx = music.intern_wav_instrument(layer.wav_inst);
                    instEv.noise_inst = layer.noise_inst;
                    music.add_event(instEv);
                }
            }

            // Compile each layer across all cycles needed for polymeter realignment.
            for (int cycle = 0; cycle < stepLoopCycles; ++cycle) {
                auto cycleOffset = offset + cycleFrames * rational{cycle};
                for (std::uint8_t l = 0; l < step.layer_count; ++l) {
                    compile_detail::compile_channel(
                        music, step.layers[l].ast, step.layers[l].get_channel(), cycleFrames, cycleOffset,
                        step.layers[l].has_layer_overrides() ? step.layers[l].m_layerOverrides.data() : nullptr,
                        step.layers[l].m_layerOverrideCount, step.layers[l].m_timeShift);
                }
            }

            offset = offset + cycleFrames * rational{stepLoopCycles};
        }

        music.loop_step_count = seqLoopSteps > 0 ? seqLoopSteps : 1;
        auto seqNominalEnd = (offset.num * music.time_den) / offset.den;
        if (seqNominalEnd > music.total_time_num) music.total_time_num = seqNominalEnd;
        music.finalize();
        return music;
    }

    // -- Public compile API ----------------------------------------------

    /// @brief Compile a single pattern using default tempo (0.5 cps = 120 BPM).
    ///
    /// @param pat A `pattern` created via `note()`, `s()`, or UDL suffixes.
    /// @return A `compiled_music` struct with sorted events and timepoint batches.
    consteval compiled_music compile(const pattern& pat) {
        return compile_with_tempo<default_tempo>(pat);
    }

    /// @brief Compile a single pattern with explicit tempo.
    ///
    /// @tparam Tempo Compile-time tempo value (`120_bpm`, `0.5_cps`, `30_cpm`).
    /// @param pat A `pattern` created via `note()`, `s()`, or UDL suffixes.
    /// @return A `compiled_music` struct with sorted events and timepoint batches.
    template<tempo Tempo>
    consteval compiled_music compile(const pattern& pat) {
        return compile_with_tempo<Tempo>(pat);
    }

    /// @brief Compile a stack of simultaneous patterns using default tempo.
    ///
    /// @param sp A `stacked_pattern` from `stack(p1, p2, ...)`.
    /// @return A `compiled_music` with events from all layers interleaved.
    consteval compiled_music compile(const stacked_pattern& sp) {
        return compile_with_tempo<default_tempo>(sp);
    }

    /// @brief Compile a stack with explicit tempo.
    ///
    /// @tparam Tempo Compile-time tempo value.
    /// @param sp A `stacked_pattern` from `stack(p1, p2, ...)`.
    /// @return A `compiled_music` with events from all layers interleaved.
    template<tempo Tempo>
    consteval compiled_music compile(const stacked_pattern& sp) {
        return compile_with_tempo<Tempo>(sp);
    }

    /// @brief Compile a sequential arrangement using default tempo.
    ///
    /// @param sp A `sequential_pattern` from `seq(p1, p2, ...)` or `loop(p)`.
    /// @return A `compiled_music` with sequential segments and instrument changes.
    consteval compiled_music compile(const sequential_pattern& sp) {
        return compile_with_tempo<default_tempo>(sp);
    }

    /// @brief Compile a sequential arrangement with explicit tempo.
    ///
    /// @tparam Tempo Compile-time tempo value.
    /// @param sp A `sequential_pattern` from `seq(p1, p2, ...)` or `loop(p)`.
    /// @return A `compiled_music` with sequential segments and instrument changes.
    template<tempo Tempo>
    consteval compiled_music compile(const sequential_pattern& sp) {
        return compile_with_tempo<Tempo>(sp);
    }

} // namespace gba::music
