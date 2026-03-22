/// @file bits/music/pattern.hpp
/// @brief Channel-agnostic pattern type, note() API, and combinators.
///
/// A `pattern` carries a parsed mini-notation AST, an optional channel
/// assignment, and instrument configurations. Patterns are combined via
/// `stack()` (parallel, auto-channel assignment), `seq()` (sequential,
/// channel switching at boundaries), and `loop()` (infinite repeat).
///
/// Primary API: `note("c4 e4 g4")` - returns a channel-agnostic pattern.
/// Multi-voice: `note("<melody, bass>")` - commas in <> create parallel layers.
#pragma once

#include <gba/bits/music/parse.hpp>

#include <cstddef>
#include <cstdint>

namespace gba::music {

    // -- Pattern ---------------------------------------------------------

    /// @brief Sentinel value: no channel assigned.
    inline constexpr std::uint8_t no_channel = 0xFF;

    // -- Transform descriptors (Phase 4) ---------------------------------
    //
    // Lightweight tag types encoding a single pattern transform. Used as
    // arguments to `.superimpose()` and `.off()` for Strudel-style
    // higher-order combinators.

    /// @brief Transpose up by N semitones.
    struct add_transform {
        int semitones;
    };

    /// @brief Transpose down by N semitones.
    struct sub_transform {
        int semitones;
    };

    /// @brief Reverse child order in sequence/subdivision nodes.
    struct rev_transform {};

    /// @brief Repeat each note N times in-place (stutter/roll).
    struct ply_transform {
        int n;
    };

    /// @brief Staccato: each note plays for half its duration.
    struct press_transform {};

    // Factory functions for concise Strudel-like syntax:
    //   note("c4 e4").superimpose(add(7))
    //   note("c4 e4").off(1, 8, add(7))
    consteval add_transform add(int n) {
        return {n};
    }
    consteval sub_transform sub(int n) {
        return {n};
    }
    consteval rev_transform rev() {
        return {};
    }
    consteval ply_transform ply(int n) {
        return {n};
    }
    consteval press_transform press() {
        return {};
    }

    // Forward declaration needed for pattern::off() return type.
    struct stacked_pattern;

    // -- AST validation + rewrite helpers ---------------------------------

    namespace pattern_detail {

        /// @brief Recursively validate that an AST contains only drum names, rests, and holds.
        ///
        /// Used by `s()` to enforce that no chromatic notes appear in a drum pattern.
        /// Throws at consteval time with a clear diagnostic if a pitched note is found.
        consteval void validate_drum_only(const parsed_pattern& ast, std::uint16_t nodeIdx) {
            const auto& node = ast.nodes[nodeIdx];
            if (node.type == ast_type::note_literal) {
                if (node.note_value != note::rest && node.note_value != note::hold && !is_drum(node.note_value))
                    throw "s(): only drum names (bd, sd, hh, oh, cp, rs, rim, lt, mt, ht, cb, cr, rd, hc, mc, lc, cl, "
                          "sh, ma, ag), rests (~), and holds (_) are allowed - use note() for pitched notes";
            }
            for (std::uint8_t i = 0; i < node.child_count; ++i) validate_drum_only(ast, node.children[i]);
        }

        // -- AST rewrite helpers (Phase 1 pattern functions) --------------

        /// @brief Deep-copy an AST subtree, returning the new root index.
        ///
        /// Creates new node slots for the entire subtree rooted at nodeIdx.
        /// Used by `.palindrome()` to create a reversed copy without
        /// mutating the original.
        consteval std::uint16_t copy_subtree(parsed_pattern& ast, std::uint16_t nodeIdx) {
            auto orig = ast.nodes[nodeIdx]; // value copy
            auto newIdx = ast.add_node(orig);
            auto& newNode = ast.nodes[newIdx];
            // Recursively deep-copy all children
            for (std::uint8_t i = 0; i < orig.child_count; ++i) {
                newNode.children[i] = copy_subtree(ast, orig.children[i]);
            }
            return newIdx;
        }

        /// @brief Reverse child order in sequence/subdivision nodes recursively.
        consteval void reverse_node(parsed_pattern& ast, std::uint16_t nodeIdx) {
            auto& node = ast.nodes[nodeIdx];

            // Reverse children of sequence and subdivision nodes
            if (node.type == ast_type::sequence || node.type == ast_type::subdivision) {
                // Reverse both children[] and weights[] arrays in-place
                for (std::uint8_t i = 0; i < node.child_count / 2; ++i) {
                    auto j = static_cast<std::uint8_t>(node.child_count - 1 - i);
                    // Swap children
                    auto tmpChild = node.children[i];
                    node.children[i] = node.children[j];
                    node.children[j] = tmpChild;
                    // Swap weights
                    auto tmpWeight = node.weights[i];
                    node.weights[i] = node.weights[j];
                    node.weights[j] = tmpWeight;
                }
            }

            // Recurse into all children
            for (std::uint8_t i = 0; i < node.child_count; ++i) reverse_node(ast, node.children[i]);
        }

        /// @brief Transpose chromatic notes by N semitones, recursively.
        ///
        /// Skips rests, holds, and drum notes. Throws if result is below C2
        /// (GBA PSG floor) or above B8.
        consteval void transpose_node(parsed_pattern& ast, std::uint16_t nodeIdx, int semitones) {
            auto& node = ast.nodes[nodeIdx];

            if (node.type == ast_type::note_literal && is_chromatic(node.note_value)) {
                auto idx = static_cast<int>(node.note_value) - static_cast<int>(first_chromatic);
                idx += semitones;
                // Validate range: C2 (index 12) to B8 (index 95)
                if (idx < 12) throw "pattern::add/sub: transposition would go below C2 (GBA PSG floor)";
                if (idx > 95) throw "pattern::add/sub: transposition would go above B8";
                node.note_value = static_cast<note>(static_cast<int>(first_chromatic) + idx);
            }

            for (std::uint8_t i = 0; i < node.child_count; ++i) transpose_node(ast, node.children[i], semitones);
        }

        /// @brief Wrap each note_literal in a fast(N) node for stutter/roll effect.
        consteval void ply_node(parsed_pattern& ast, std::uint16_t nodeIdx, int n) {
            auto& node = ast.nodes[nodeIdx];

            if (node.type == ast_type::note_literal && node.note_value != note::rest && node.note_value != note::hold) {
                // Wrap: create a fast node pointing to this note_literal.
                // We can't move the existing node, so we clone it to a new slot
                // and replace this slot with the fast wrapper.
                auto noteCloneIdx = ast.add_node(node); // copy current note

                // Overwrite this node as a fast wrapper
                node = ast_node{};
                node.type = ast_type::fast;
                node.modifier_num = static_cast<std::uint16_t>(n);
                node.modifier_den = 1;
                node.children[0] = noteCloneIdx;
                node.child_count = 1;
                node.weights[0] = 1;
                return; // don't recurse into the cloned note
            }

            for (std::uint8_t i = 0; i < node.child_count; ++i) ply_node(ast, node.children[i], n);
        }

        /// @brief Wrap each note_literal in a [note rest] subdivision for staccato.
        consteval void press_node(parsed_pattern& ast, std::uint16_t nodeIdx) {
            auto& node = ast.nodes[nodeIdx];

            if (node.type == ast_type::note_literal && node.note_value != note::rest && node.note_value != note::hold) {
                // Clone the note to a new slot
                auto noteCloneIdx = ast.add_node(node);

                // Create a rest node
                ast_node restNode{};
                restNode.type = ast_type::note_literal;
                restNode.note_value = note::rest;
                auto restIdx = ast.add_node(restNode);

                // Overwrite this node as a subdivision [note rest]
                node = ast_node{};
                node.type = ast_type::subdivision;
                node.children[0] = noteCloneIdx;
                node.children[1] = restIdx;
                node.weights[0] = 1;
                node.weights[1] = 1;
                node.child_count = 2;
                return; // don't recurse into children
            }

            for (std::uint8_t i = 0; i < node.child_count; ++i) press_node(ast, node.children[i]);
        }

        // -- AST rewrite helpers (Phase 3 - rotation/palindrome) ----------

        /// @brief Rotate children[] and weights[] of a node left by k positions.
        ///
        /// Only affects sequence and subdivision nodes (which have ordered children).
        /// Used by `.iter()` to create rotated variants of a pattern.
        consteval void rotate_children_left(ast_node& node, int k) {
            if (node.child_count <= 1) return;
            // Normalize k into [0, child_count)
            k = k % static_cast<int>(node.child_count);
            if (k < 0) k += node.child_count;
            if (k == 0) return;

            // Simple consteval rotation via temp arrays
            std::array<std::uint16_t, max_children> tmpChildren{};
            std::array<std::uint8_t, max_children> tmpWeights{};
            for (std::uint8_t i = 0; i < node.child_count; ++i) {
                auto dst = static_cast<std::uint8_t>((i + node.child_count - k) % node.child_count);
                tmpChildren[dst] = node.children[i];
                tmpWeights[dst] = node.weights[i];
            }
            for (std::uint8_t i = 0; i < node.child_count; ++i) {
                node.children[i] = tmpChildren[i];
                node.weights[i] = tmpWeights[i];
            }
        }

        /// @brief Recursively rotate children of the top-level sequence/subdivision left by k.
        ///
        /// Only the root sequence/subdivision is rotated - nested subdivisions within
        /// each step keep their internal order. This matches Strudel's `iter()` semantics
        /// where the rotation operates on the outermost sequence steps.
        consteval void rotate_root_left(parsed_pattern& ast, std::uint16_t nodeIdx, int k) {
            auto& node = ast.nodes[nodeIdx];
            if (node.type == ast_type::sequence || node.type == ast_type::subdivision) {
                rotate_children_left(node, k);
            }
        }

        // -- AST-level transform dispatch (Phase 4) ------------------------

        /// @brief Apply an add_transform to an AST subtree.
        consteval void apply_ast_transform(parsed_pattern& ast, std::uint16_t root, const add_transform& t) {
            transpose_node(ast, root, t.semitones);
        }

        /// @brief Apply a sub_transform to an AST subtree.
        consteval void apply_ast_transform(parsed_pattern& ast, std::uint16_t root, const sub_transform& t) {
            transpose_node(ast, root, -t.semitones);
        }

        /// @brief Apply a rev_transform to an AST subtree.
        consteval void apply_ast_transform(parsed_pattern& ast, std::uint16_t root, const rev_transform&) {
            reverse_node(ast, root);
        }

        /// @brief Apply a ply_transform to an AST subtree.
        consteval void apply_ast_transform(parsed_pattern& ast, std::uint16_t root, const ply_transform& t) {
            ply_node(ast, root, t.n);
        }

        /// @brief Apply a press_transform to an AST subtree.
        consteval void apply_ast_transform(parsed_pattern& ast, std::uint16_t root, const press_transform&) {
            press_node(ast, root);
        }

        /// @brief Truncate the root sequence/subdivision to its first ceil(N/n) children.
        ///
        /// Used by `.linger()`. Only the outermost node is truncated - nested
        /// subdivisions keep their full content. For non-sequence types (single
        /// note, alternating, etc.) this is a no-op.
        consteval void linger_node(parsed_pattern& ast, std::uint16_t nodeIdx, int n) {
            auto& node = ast.nodes[nodeIdx];
            if (node.type == ast_type::sequence || node.type == ast_type::subdivision) {
                int keep = (static_cast<int>(node.child_count) + n - 1) / n;
                if (keep < 1) keep = 1;
                node.child_count = static_cast<std::uint8_t>(keep);
            }
        }

    } // namespace pattern_detail

    /// @brief Channel + instrument configuration for a single layer in `.channels()`.
    ///
    /// Implicitly convertible from a bare `channel` enum for the common case.
    /// For custom instruments, use `layer_cfg{channel::wav, wav_instrument{...}}`.
    struct layer_cfg {
        std::uint8_t assigned_channel{no_channel};
        sq1_instrument sq1_inst{};
        sq2_instrument sq2_inst{};
        wav_instrument wav_inst{};
        noise_instrument noise_inst{};

        consteval layer_cfg() = default;

        /// @brief Implicit conversion from bare channel (uses default instruments).
        consteval layer_cfg(music::channel ch) : assigned_channel{static_cast<std::uint8_t>(ch)} {}

        /// @brief Channel + SQ1 instrument.
        consteval layer_cfg(music::channel ch, sq1_instrument inst)
            : assigned_channel{static_cast<std::uint8_t>(ch)}, sq1_inst{inst} {
            if (ch != music::channel::sq1) throw "layer_cfg: sq1_instrument requires channel::sq1";
        }

        /// @brief Channel + SQ2 instrument.
        consteval layer_cfg(music::channel ch, sq2_instrument inst)
            : assigned_channel{static_cast<std::uint8_t>(ch)}, sq2_inst{inst} {
            if (ch != music::channel::sq2) throw "layer_cfg: sq2_instrument requires channel::sq2";
        }

        /// @brief Channel + WAV instrument.
        consteval layer_cfg(music::channel ch, wav_instrument inst)
            : assigned_channel{static_cast<std::uint8_t>(ch)}, wav_inst{inst} {
            if (ch != music::channel::wav) throw "layer_cfg: wav_instrument requires channel::wav";
        }

        /// @brief Channel + NOISE instrument.
        consteval layer_cfg(music::channel ch, noise_instrument inst)
            : assigned_channel{static_cast<std::uint8_t>(ch)}, noise_inst{inst} {
            if (ch != music::channel::noise) throw "layer_cfg: noise_instrument requires channel::noise";
        }
    };

    /// @brief A channel-agnostic pattern with parsed AST and optional channel/instrument.
    ///
    /// Created via `note("...")` or UDL suffixes (`"..."_sq1`).
    /// The AST may contain a `stacked` root node if commas were used inside `<>`.
    struct pattern {
        parsed_pattern ast{};

        /// @brief Assigned channel (no_channel = unassigned, auto-assigned by stack/compile).
        std::uint8_t assigned_channel{no_channel};

        /// @brief Per-channel instruments (stored regardless of assigned channel).
        sq1_instrument sq1_inst{};
        sq2_instrument sq2_inst{};
        wav_instrument wav_inst{};
        noise_instrument noise_inst{};

        /// @brief Per-layer channel/instrument overrides (from .channels()).
        std::array<layer_cfg, 4> m_layerOverrides{};
        std::uint8_t m_layerOverrideCount{0};

        /// @brief Time shift as fraction of a cycle (from .early()/.late()).
        /// Positive = later, negative = earlier. Applied during compilation.
        rational m_timeShift{};

        /// @brief Check if a channel has been explicitly assigned.
        consteval bool has_channel() const { return assigned_channel != no_channel; }

        /// @brief Get the assigned channel.
        consteval music::channel get_channel() const {
            if (!has_channel()) throw "pattern::get_channel: no channel assigned";
            return static_cast<music::channel>(assigned_channel);
        }

        /// @brief Check if the AST root is a stacked node (multi-layer from commas in <>).
        consteval bool is_stacked() const { return ast.nodes[ast.root].type == ast_type::stacked; }

        /// @brief Get the number of layers (1 for single, N for stacked AST).
        consteval std::uint8_t layer_count() const {
            if (is_stacked()) return ast.nodes[ast.root].child_count;
            return 1;
        }

        /// @brief Check if per-layer overrides are set.
        consteval bool has_layer_overrides() const { return m_layerOverrideCount > 0; }

        /// @brief Assign a channel (default instrument).
        consteval pattern channel(music::channel ch) const {
            if (is_stacked()) throw "pattern::channel: cannot assign channel to multi-layer pattern";
            auto copy = *this;
            copy.assigned_channel = static_cast<std::uint8_t>(ch);
            return copy;
        }

        /// @brief Assign SQ1 channel with custom instrument.
        consteval pattern channel(music::channel ch, sq1_instrument inst) const {
            if (ch != music::channel::sq1) throw "pattern::channel: sq1_instrument requires channel::sq1";
            auto copy = channel(ch);
            copy.sq1_inst = inst;
            return copy;
        }

        /// @brief Assign SQ2 channel with custom instrument.
        consteval pattern channel(music::channel ch, sq2_instrument inst) const {
            if (ch != music::channel::sq2) throw "pattern::channel: sq2_instrument requires channel::sq2";
            auto copy = channel(ch);
            copy.sq2_inst = inst;
            return copy;
        }

        /// @brief Assign WAV channel with custom instrument.
        consteval pattern channel(music::channel ch, wav_instrument inst) const {
            if (ch != music::channel::wav) throw "pattern::channel: wav_instrument requires channel::wav";
            auto copy = channel(ch);
            copy.wav_inst = inst;
            return copy;
        }

        /// @brief Assign NOISE channel with custom instrument.
        consteval pattern channel(music::channel ch, noise_instrument inst) const {
            if (ch != music::channel::noise) throw "pattern::channel: noise_instrument requires channel::noise";
            auto copy = channel(ch);
            copy.noise_inst = inst;
            return copy;
        }

        /// @brief Assign channels (and optional instruments) to comma-separated layers.
        ///
        /// Only valid on multi-layer patterns (where the AST root is a stacked
        /// node from commas inside `<>`). Call before `.slow()` or `.fast()`.
        ///
        /// Each argument is either a bare `channel` enum (for default instruments)
        /// or a `layer_cfg{channel, instrument}` for custom instruments.
        ///
        /// @code{.cpp}
        /// // Channel-only:
        /// note("<melody, bass>").channels(channel::wav, channel::sq2)
        ///
        /// // With custom wav instrument (e.g. Piano.wav):
        /// note("<melody, bass>").channels(layer_cfg{channel::wav, piano}, channel::sq2)
        /// @endcode
        template<typename... Cfgs>
        consteval pattern channels(Cfgs... cfgs) const {
            if (!is_stacked()) throw "pattern::channels: only valid on multi-layer pattern (commas in <>)";
            if (sizeof...(Cfgs) != layer_count()) throw "pattern::channels: argument count must match layer count";
            if (sizeof...(Cfgs) > 4) throw "pattern::channels: too many layers (max 4)";

            auto copy = *this;
            copy.m_layerOverrideCount = static_cast<std::uint8_t>(sizeof...(Cfgs));
            std::uint8_t idx = 0;
            ((copy.m_layerOverrides[idx++] = layer_cfg{cfgs}), ...);

            // Validate no duplicate channels
            for (std::uint8_t i = 0; i < copy.m_layerOverrideCount; ++i)
                for (std::uint8_t j = i + 1; j < copy.m_layerOverrideCount; ++j)
                    if (copy.m_layerOverrides[i].assigned_channel == copy.m_layerOverrides[j].assigned_channel)
                        throw "pattern::channels: duplicate channel - each PSG channel can only appear once";

            return copy;
        }

        /// @brief Slow the pattern by N (stretch across N cycles).
        ///
        /// Equivalent to `/N` in mini-notation: `note("c4 e4").slow(2)` = `note("c4 e4/2")`.
        /// Accepts integer values.
        consteval pattern slow(int n) const {
            if (n <= 0) throw "pattern::slow: value must be > 0";
            auto copy = *this;
            // Wrap existing root in a slow node
            ast_node node{};
            node.type = ast_type::slow;
            node.modifier_num = static_cast<std::uint16_t>(n);
            node.modifier_den = 1;
            auto nodeIdx = copy.ast.add_node(node);
            copy.ast.add_child(nodeIdx, copy.ast.root);
            copy.ast.root = nodeIdx;
            return copy;
        }

        /// @brief Speed up the pattern by N (play N times faster).
        ///
        /// Equivalent to `*N` in mini-notation: `note("c4 e4").fast(2)` = `note("c4 e4*2")`.
        /// Accepts integer values.
        consteval pattern fast(int n) const {
            if (n <= 0) throw "pattern::fast: value must be > 0";
            auto copy = *this;
            ast_node node{};
            node.type = ast_type::fast;
            node.modifier_num = static_cast<std::uint16_t>(n);
            node.modifier_den = 1;
            auto nodeIdx = copy.ast.add_node(node);
            copy.ast.add_child(nodeIdx, copy.ast.root);
            copy.ast.root = nodeIdx;
            return copy;
        }

        // -- Strudel pattern functions (Phase 1) -------------------------

        /// @brief Reverse the pattern - children of sequence/subdivision nodes play backwards.
        ///
        /// Mirrors Strudel's `rev()`. Recursively reverses child order in
        /// sequence and subdivision AST nodes. Does not affect alternating,
        /// stacked, or single-note nodes.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").rev()  // -> g4 e4 c4
        /// @endcode
        consteval pattern rev() const {
            auto copy = *this;
            pattern_detail::reverse_node(copy.ast, copy.ast.root);
            return copy;
        }

        /// @brief Transpose all chromatic notes up by N semitones.
        ///
        /// Mirrors Strudel's `add()`. Skips rests, holds, and drum notes.
        /// Consteval-throws if any note would go below C2 or above B8.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").add(7)  // -> g4 b4 d5
        /// @endcode
        consteval pattern add(int semitones) const {
            auto copy = *this;
            pattern_detail::transpose_node(copy.ast, copy.ast.root, semitones);
            return copy;
        }

        /// @brief Transpose all chromatic notes down by N semitones.
        ///
        /// Mirrors Strudel's `sub()`. Sugar for `.add(-n)`.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").sub(12)  // -> c3 e3 g3
        /// @endcode
        consteval pattern sub(int semitones) const { return add(-semitones); }

        /// @brief Repeat each note N times in-place (stutter/roll effect).
        ///
        /// Mirrors Strudel's `ply()`. Each note_literal is wrapped in a `fast`
        /// node with multiplier N, producing N repetitions within the same
        /// time slot.
        ///
        /// @code{.cpp}
        /// note("c4 e4").ply(2)  // -> [c4 c4] [e4 e4] (each note plays twice)
        /// @endcode
        consteval pattern ply(int n) const {
            if (n <= 0) throw "pattern::ply: value must be > 0";
            if (n == 1) return *this;
            auto copy = *this;
            pattern_detail::ply_node(copy.ast, copy.ast.root, n);
            return copy;
        }

        /// @brief Staccato articulation - each note plays for half its duration.
        ///
        /// Mirrors Strudel's `press`. Each note_literal is wrapped in a
        /// 2-element sequence `[note rest]`, giving a short-long (staccato) feel.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").press()  // each note sounds for half, silent for half
        /// @endcode
        consteval pattern press() const {
            auto copy = *this;
            pattern_detail::press_node(copy.ast, copy.ast.root);
            return copy;
        }

        // -- Time shift (Phase 2) ----------------------------------------

        /// @brief Delay the pattern by `num/den` cycles.
        ///
        /// Mirrors Strudel's `late()`. Events fire later by the given fraction
        /// of a cycle. Useful for swing/groove and offset canons.
        ///
        /// @code{.cpp}
        /// note("c4 e4").late(1, 8)  // delay by 1/8 of a cycle
        /// note("c4 e4").late(1)     // delay by 1 full cycle
        /// @endcode
        consteval pattern late(int num, int den = 1) const {
            if (den <= 0) throw "pattern::late: denominator must be > 0";
            auto copy = *this;
            copy.m_timeShift = copy.m_timeShift + rational{num, den};
            return copy;
        }

        /// @brief Advance the pattern by `num/den` cycles.
        ///
        /// Mirrors Strudel's `early()`. Events fire earlier by the given fraction
        /// of a cycle. Events shifted before time 0 fire at the start.
        ///
        /// @code{.cpp}
        /// note("c4 e4").early(1, 8)  // advance by 1/8 of a cycle
        /// @endcode
        consteval pattern early(int num, int den = 1) const {
            if (den <= 0) throw "pattern::early: denominator must be > 0";
            auto copy = *this;
            copy.m_timeShift = copy.m_timeShift - rational{num, den};
            return copy;
        }

        // -- Rotation / palindrome (Phase 3) ------------------------------

        /// @brief Rotate the sequence left by 1 step each cycle, producing N variants.
        ///
        /// Mirrors Strudel's `iter(n)`. Creates an `alternating` node with `n`
        /// children, where child `i` is a deep copy of the root with its
        /// top-level sequence/subdivision rotated left by `i` steps.
        ///
        /// For a 4-note sequence `"c4 e4 g4 b4".iter(4)`:
        /// - cycle 0: c4 e4 g4 b4 (shift 0)
        /// - cycle 1: e4 g4 b4 c4 (shift 1)
        /// - cycle 2: g4 b4 c4 e4 (shift 2)
        /// - cycle 3: b4 c4 e4 g4 (shift 3)
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4 b4").iter(4)  // 4 rotations, one per cycle
        /// note("c4 e4 g4 b4").iter(2)  // 2 rotations: shift 0 and shift 2
        /// @endcode
        consteval pattern iter(int n) const {
            if (n <= 0) throw "pattern::iter: value must be > 0";
            if (n == 1) return *this;

            auto copy = *this;

            // Determine the step count of the root node for rotation stride
            const auto& rootNode = copy.ast.nodes[copy.ast.root];
            int stepCount = rootNode.child_count;
            if (stepCount == 0) stepCount = 1; // single note_literal

            // Create alternating node with n children (one per rotation)
            ast_node altNode{};
            altNode.type = ast_type::alternating;

            if (static_cast<int>(n) > static_cast<int>(max_children)) throw "pattern::iter: n exceeds max_children";

            for (int i = 0; i < n; ++i) {
                // Deep-copy the entire subtree
                auto copyIdx = pattern_detail::copy_subtree(copy.ast, copy.ast.root);
                // Rotate the root of this copy left by i steps
                int shift = (i * stepCount) / n;
                pattern_detail::rotate_root_left(copy.ast, copyIdx, shift);
                altNode.children[i] = copyIdx;
                altNode.weights[i] = 1;
            }
            altNode.child_count = static_cast<std::uint8_t>(n);

            auto altIdx = copy.ast.add_node(altNode);
            copy.ast.root = altIdx;
            return copy;
        }

        /// @brief Forward on even cycles, reversed on odd cycles.
        ///
        /// Mirrors Strudel's `palindrome()`. Creates an `alternating` node with
        /// 2 children: child 0 = original (forward), child 1 = deep-copied and
        /// reversed (backward).
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").palindrome()
        /// // cycle 0: c4 e4 g4 (forward)
        /// // cycle 1: g4 e4 c4 (reversed)
        /// // cycle 2: c4 e4 g4 (forward) - and so on
        /// @endcode
        consteval pattern palindrome() const {
            auto copy = *this;

            // Deep-copy the subtree for the reversed variant
            auto reversedIdx = pattern_detail::copy_subtree(copy.ast, copy.ast.root);
            pattern_detail::reverse_node(copy.ast, reversedIdx);

            // Create alternating with 2 children: forward, reversed
            ast_node altNode{};
            altNode.type = ast_type::alternating;
            altNode.children[0] = copy.ast.root;
            altNode.children[1] = reversedIdx;
            altNode.weights[0] = 1;
            altNode.weights[1] = 1;
            altNode.child_count = 2;

            auto altIdx = copy.ast.add_node(altNode);
            copy.ast.root = altIdx;
            return copy;
        }

        // -- Higher-order combinators (Phase 4) ---------------------------

        /// @brief Stack the original pattern with a transformed copy.
        ///
        /// Mirrors Strudel's `superimpose()`. Creates a stacked AST root with
        /// two children: the original pattern and a copy with the transform
        /// applied. Auto-assigns sq1 + sq2 (or next available channels).
        ///
        /// Uses transform descriptor types: `add(7)`, `sub(12)`, `rev()`,
        /// `ply(2)`, `press()`.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").superimpose(add(7))   // original + fifth above
        /// note("c4 e4 g4").superimpose(rev())     // original + reversed
        /// @endcode
        template<typename Transform>
        consteval pattern superimpose(Transform t) const {
            auto result = *this;
            auto origRoot = result.ast.root;

            // Deep-copy the subtree for the transformed layer
            auto copyRoot = pattern_detail::copy_subtree(result.ast, origRoot);

            // Apply the transform to the copy (dispatched by overloading)
            pattern_detail::apply_ast_transform(result.ast, copyRoot, t);

            // Create stacked root: [original, transformed_copy]
            ast_node stackedNode{};
            stackedNode.type = ast_type::stacked;
            stackedNode.children[0] = origRoot;
            stackedNode.children[1] = copyRoot;
            stackedNode.weights[0] = 1;
            stackedNode.weights[1] = 1;
            stackedNode.child_count = 2;

            auto stackedIdx = result.ast.add_node(stackedNode);
            result.ast.root = stackedIdx;
            return result;
        }

        /// @brief Delayed overlay: stack original + transformed copy shifted in time.
        ///
        /// Mirrors Strudel's `off()`. Equivalent to
        /// `stack(original, original.transform(t).late(num, den))`.
        ///
        /// Returns `stacked_pattern` because per-layer time shifts require
        /// separate pattern objects. Works with `compile()`, `loop()`, `seq()`.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4").off(1, 8, add(7))   // fifth above, delayed 1/8 cycle
        /// compile(loop(note("c4 e4").off(1, 4, rev())))
        /// @endcode
        template<typename Transform>
        consteval stacked_pattern off(int num, int den, Transform t) const;

        /// @brief Keep only the first 1/n of the pattern.
        ///
        /// Mirrors Strudel's `linger()`. Truncates the root sequence to its
        /// first `ceil(child_count / n)` children. The remaining children fill
        /// the full cycle, so `"c4 e4 g4 b4".linger(2)` becomes `"c4 e4"`
        /// (2 notes filling the whole cycle).
        ///
        /// For non-sequence types (single note, alternating), this is identity.
        ///
        /// @code{.cpp}
        /// note("c4 e4 g4 b4").linger(2)  // -> c4 e4
        /// note("c4 e4 g4 b4").linger(4)  // -> c4
        /// @endcode
        consteval pattern linger(int n) const {
            if (n <= 0) throw "pattern::linger: value must be > 0";
            if (n == 1) return *this;
            auto copy = *this;
            pattern_detail::linger_node(copy.ast, copy.ast.root, n);
            return copy;
        }
    };

    // -- note() ----------------------------------------------------------

    /// @brief Create a pattern from a mini-notation string.
    ///
    /// This is the primary API. Parses the string at compile time into an AST.
    /// If the notation contains commas inside `<>`, the resulting pattern has
    /// a stacked root (multi-layer, auto-assigned to PSG channels by compile()).
    ///
    /// @code{.cpp}
    /// constexpr auto p = note("c4 e4 g4");           // single layer
    /// constexpr auto p = note("<c4 e4, g4 b4>");     // two parallel layers
    /// @endcode
    template<std::size_t N>
    consteval pattern note(const char (&str)[N]) {
        pattern p{};
        p.ast = parse_mini(str, N - 1);
        return p;
    }

    // -- s() -------------------------------------------------------------

    /// @brief Create a drum pattern from a mini-notation string.
    ///
    /// Mirrors Strudel's `s()` function for sample/drum selection. On GBA,
    /// drums map to the noise channel with presets from `drum_preset_table`.
    ///
    /// Only drum names (`bd`, `sd`, `hh`, `oh`, `cp`, `rs`, `lt`, `mt`,
    /// `ht`, `cb`, `cr`, `rd`), rests (`~`), and holds (`_`) are allowed.
    /// Chromatic notes cause a consteval compile error - use `note()` for pitched content.
    ///
    /// Supports all mini-notation operators: euclidean rhythms, fast/slow,
    /// alternating, subdivision, etc.
    ///
    /// @code{.cpp}
    /// constexpr auto drums = s("bd(3,8,0)");         // euclidean kick
    /// constexpr auto hats  = s("hh*8");              // fast hi-hats
    /// constexpr auto beat  = s("bd sd hh oh");       // four-on-the-floor
    /// @endcode
    template<std::size_t N>
    consteval pattern s(const char (&str)[N]) {
        pattern p{};
        p.ast = parse_mini(str, N - 1);
        pattern_detail::validate_drum_only(p.ast, p.ast.root);
        p.assigned_channel = static_cast<std::uint8_t>(channel::noise);
        return p;
    }

    // -- UDL operators ---------------------------------------------------

    namespace literals {

        namespace detail {
            template<std::size_t N>
            struct mini_string {
                char data[N]{};
                static constexpr std::size_t size = N - 1;

                consteval mini_string(const char (&str)[N]) {
                    for (std::size_t i = 0; i < N; ++i) data[i] = str[i];
                }
            };
        } // namespace detail

        /// @brief `"c4 e4 g4"_sq1` - Square wave channel 1 (with sweep).
        template<detail::mini_string S>
        consteval auto operator""_sq1() {
            return note(S.data).channel(channel::sq1);
        }

        /// @brief `"c4 e4 g4"_sq2` - Square wave channel 2.
        template<detail::mini_string S>
        consteval auto operator""_sq2() {
            return note(S.data).channel(channel::sq2);
        }

        /// @brief `"c4 e4 g4"_wav` - Wave channel.
        template<detail::mini_string S>
        consteval auto operator""_wav() {
            return note(S.data).channel(channel::wav);
        }

        /// @brief `"bd hh sd hh"_noise` - Noise channel.
        template<detail::mini_string S>
        consteval auto operator""_noise() {
            return note(S.data).channel(channel::noise);
        }

        /// @brief `"bd(3,8,0)"_s` - Drum pattern (validates drum-only, auto noise channel).
        ///
        /// Sugar for `s("bd(3,8,0)")`. Mirrors Strudel's `s()` function.
        /// Rejects chromatic notes at compile time.
        template<detail::mini_string S>
        consteval auto operator""_s() {
            return s(S.data);
        }

    } // namespace literals

    // -- Combinators -----------------------------------------------------

    /// @brief Maximum segments in a seq(). Generous - only consteval intermediate.
    inline constexpr std::size_t max_seq_segments = 32;

    /// @brief Maximum channels in a stack().
    inline constexpr std::size_t max_stack_layers = 4;

    /// @brief A stack of simultaneous patterns on distinct channels.
    struct stacked_pattern {
        std::array<pattern, max_stack_layers> layers{};
        std::uint8_t layer_count{};

        consteval bool has_channel(music::channel ch) const {
            for (std::uint8_t i = 0; i < layer_count; ++i)
                if (layers[i].has_channel() && layers[i].get_channel() == ch) return true;
            return false;
        }

        /// @brief Resolve auto-channel assignment: sq1->sq2->wav->noise in order.
        consteval void resolve_channels() {
            constexpr music::channel order[] = {music::channel::sq1, music::channel::sq2, music::channel::wav,
                                                music::channel::noise};
            std::uint8_t next = 0;

            for (std::uint8_t i = 0; i < layer_count; ++i) {
                if (!layers[i].has_channel()) {
                    // Find next available channel
                    while (next < 4 && has_channel(order[next])) next++;
                    if (next >= 4) throw "stacked_pattern: too many layers for 4 PSG channels";
                    layers[i].assigned_channel = static_cast<std::uint8_t>(order[next]);
                    next++;
                }
            }

            // Validate uniqueness
            for (std::uint8_t i = 0; i < layer_count; ++i)
                for (std::uint8_t j = i + 1; j < layer_count; ++j)
                    if (layers[i].get_channel() == layers[j].get_channel())
                        throw "stacked_pattern: duplicate channel - each PSG channel can only appear once";
        }
    };

    /// @brief A sequential arrangement of pattern segments or stacks.
    struct sequential_pattern {
        // Each "step" is a stacked_pattern (which may have 1 layer for single-channel)
        std::array<stacked_pattern, max_seq_segments> steps{};
        std::uint8_t step_count{};
        bool looping{};
    };

    // -- stack() ---------------------------------------------------------

    /// @brief Combine patterns to play simultaneously on distinct channels.
    ///
    /// Untagged layers get auto-assigned sq1->sq2->wav->noise in order.
    /// Compile-time error if two patterns use the same channel or >4 layers.
    template<typename... Ps>
    consteval stacked_pattern stack(const Ps&... patterns) {
        static_assert(sizeof...(Ps) <= max_stack_layers, "stack(): too many layers (max 4 PSG channels)");

        stacked_pattern result{};
        ((result.layers[result.layer_count++] = patterns), ...);
        result.resolve_channels();
        return result;
    }

    // -- seq() -----------------------------------------------------------

    namespace seq_detail {

        consteval stacked_pattern to_stack(const pattern& p) {
            stacked_pattern sp{};
            sp.layers[sp.layer_count++] = p;
            return sp;
        }

        consteval stacked_pattern to_stack(const stacked_pattern& sp) {
            return sp;
        }

        consteval sequential_pattern to_sequential(const pattern& p) {
            sequential_pattern result{};
            result.steps[result.step_count++] = to_stack(p);
            return result;
        }

        consteval sequential_pattern to_sequential(const stacked_pattern& sp) {
            sequential_pattern result{};
            result.steps[result.step_count++] = sp;
            return result;
        }

        consteval sequential_pattern to_sequential(const sequential_pattern& sp) {
            return sp;
        }

        template<typename T>
        consteval void append(sequential_pattern& dest, const T& src) {
            auto seq = to_sequential(src);
            for (std::uint8_t i = 0; i < seq.step_count; ++i) {
                if (dest.step_count >= max_seq_segments) throw "seq(): too many segments";
                dest.steps[dest.step_count++] = seq.steps[i];
            }
        }

    } // namespace seq_detail

    /// @brief Chain patterns/stacks sequentially. Instrument configs change at boundaries.
    template<typename... Ts>
    consteval sequential_pattern seq(const Ts&... args) {
        sequential_pattern result{};
        (seq_detail::append(result, args), ...);
        return result;
    }

    // -- loop() ----------------------------------------------------------

    /// @brief Wrap a pattern/stack/seq for infinite looping.
    template<typename T>
    consteval sequential_pattern loop(const T& arg) {
        auto result = seq_detail::to_sequential(arg);
        result.looping = true;
        return result;
    }

    // -- Phase 4: pattern-level transform application + off() ----------

    namespace pattern_detail {

        /// @brief Apply a transform to a pattern, returning a new pattern.
        ///
        /// These overloads call the corresponding pattern methods. Defined
        /// after the pattern struct because they need the full method set.
        consteval pattern apply_transform(const pattern& p, const add_transform& t) {
            return p.add(t.semitones);
        }
        consteval pattern apply_transform(const pattern& p, const sub_transform& t) {
            return p.sub(t.semitones);
        }
        consteval pattern apply_transform(const pattern& p, const rev_transform&) {
            return p.rev();
        }
        consteval pattern apply_transform(const pattern& p, const ply_transform& t) {
            return p.ply(t.n);
        }
        consteval pattern apply_transform(const pattern& p, const press_transform&) {
            return p.press();
        }

    } // namespace pattern_detail

    // -- Out-of-line: pattern::off() --------------------------------------

    /// @brief Delayed overlay implementation (defined after stack() is visible).
    template<typename Transform>
    consteval stacked_pattern pattern::off(int num, int den, Transform t) const {
        if (den <= 0) throw "pattern::off: denominator must be > 0";
        stacked_pattern result{};
        result.layers[0] = *this;
        result.layers[1] = pattern_detail::apply_transform(*this, t).late(num, den);
        result.layer_count = 2;
        result.resolve_channels();
        return result;
    }

} // namespace gba::music
