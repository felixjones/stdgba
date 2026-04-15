/// @file bits/music/parse.hpp
/// @brief Consteval mini-notation lexer and recursive-descent parser.
///
/// Parses Strudel mini-notation at compile time into a constexpr AST.
/// Invalid syntax produces a throw inside consteval -> compiler diagnostic.
///
/// Supported operators (Strudel parity minus randomness):
///   sequence:     "a b c"
///   rest:         "~"
///   subdivision:  "[a b]"
///   alternating:  "<a b c>"
///   stacked:      "<a b, c d>" (parallel layers via commas in <>)
///   polymetric:   "{a b c, d e}"
///   polymeter:    "{a b c}%8"
///   elongation:   "a@3"
///   replication:  "a!3"
///   fast:         "a*3" or "a*<2 3>"
///   slow:         "a/3" or "a/<2 3>"
///   euclidean:    "a(3,8)" or "a(3,8,2)"
///   comma:        "a , b" (parallel within braces or angle brackets)
#pragma once

#include <gba/bits/music/types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba::music {


    /// @brief AST node type tags.
    enum class ast_type : std::uint8_t {
        note_literal, ///< A single note or rest.
        sequence,     ///< Space-separated elements with weights.
        subdivision,  ///< `[a b]` - fits into one parent step.
        alternating,  ///< `<a b c>` - cycle-indexed selection.
        stacked,      ///< `<a b, c d>` - parallel layers (commas in <>).
        polymetric,   ///< `{a b c, d e}` - parallel different-length layers.
        fast,         ///< `a*N` - play N times in one step.
        slow,         ///< `a/N` - stretch over N cycles.
        replicate,    ///< `a!N` - repeat as N steps.
        euclidean,    ///< `a(k,n)` or `a(k,n,r)` - Bjorklund rhythm.
    };

    /// @brief Maximum AST nodes in a parsed pattern.
    inline constexpr std::size_t max_ast_nodes = 128;

    /// @brief Maximum children per AST node.
    inline constexpr std::size_t max_children = 16;

    /// @brief A single AST node in the parsed mini-notation.
    struct ast_node {
        ast_type type{};

        // For note_literal:
        note note_value{};

        // For sequence/subdivision/alternating/stacked/polymetric: child indices
        std::array<std::uint16_t, max_children> children{};
        std::uint8_t child_count{};

        // For sequence: per-child weight (default 1). Used by @N elongation.
        std::array<std::uint8_t, max_children> weights{};

        // For fast/slow: multiplier as rational (num/den).
        // Integer N stores as {N, 1}; decimal 2.75 stores as {275, 100} then reduced.
        std::uint16_t modifier_num{};  ///< Numerator for *N, /N, !N
        std::uint16_t modifier_den{1}; ///< Denominator (1 for integer values)
        // For fast/slow with timeline: `*<2 3>` -> stored as child alternating node
        bool modifier_is_timeline{};

        // For euclidean: k pulses, n steps, r rotation
        std::uint8_t euclidean_k{};
        std::uint8_t euclidean_n{};
        std::uint8_t euclidean_r{};

        // For polymeter: %N step count
        std::uint8_t polymeter_steps{};
    };

    /// @brief Complete parsed AST for a mini-notation string.
    struct parsed_pattern {
        std::array<ast_node, max_ast_nodes> nodes{};
        std::uint16_t node_count{};
        std::uint16_t root{}; ///< Index of the root node.

        consteval std::uint16_t add_node(ast_node node) {
            if (node_count >= max_ast_nodes) throw "parsed_pattern: too many AST nodes";
            auto idx = node_count++;
            nodes[idx] = node;
            return idx;
        }

        consteval void add_child(std::uint16_t parent, std::uint16_t child, std::uint8_t weight = 1) {
            auto& p = nodes[parent];
            if (p.child_count >= max_children) throw "parsed_pattern: too many children per node";
            p.children[p.child_count] = child;
            p.weights[p.child_count] = weight;
            p.child_count++;
        }
    };


    namespace parse_detail {

        consteval bool is_digit(char c) {
            return c >= '0' && c <= '9';
        }

        consteval bool is_note_start(char c) {
            return (c >= 'a' && c <= 'z') || c == '~' || c == '_';
        }

        consteval bool is_whitespace(char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        }

        consteval int parse_int(const char* str, std::size_t& pos, std::size_t end) {
            if (pos >= end || !is_digit(str[pos])) throw "parse_int: expected digit";
            int result = 0;
            while (pos < end && is_digit(str[pos])) {
                result = result * 10 + (str[pos] - '0');
                pos++;
            }
            return result;
        }

        /// @brief Parse integer or decimal number, returning rational num/den.
        /// "3" -> {3,1}, "2.75" -> {275,100} reduced to {11,4}, "0.5" -> {1,2}.
        struct parsed_number {
            std::uint16_t num;
            std::uint16_t den;
        };

        consteval parsed_number parse_number(const char* str, std::size_t& pos, std::size_t end) {
            if (pos >= end || !is_digit(str[pos])) throw "parse_number: expected digit";
            int integer_part = 0;
            while (pos < end && is_digit(str[pos])) {
                integer_part = integer_part * 10 + (str[pos] - '0');
                pos++;
            }
            if (pos < end && str[pos] == '.') {
                pos++; // skip '.'
                if (pos >= end || !is_digit(str[pos])) throw "parse_number: expected digit after decimal point";
                int frac_num = 0;
                int frac_den = 1;
                while (pos < end && is_digit(str[pos])) {
                    frac_num = frac_num * 10 + (str[pos] - '0');
                    frac_den *= 10;
                    pos++;
                }
                // num = integer_part * frac_den + frac_num, den = frac_den
                int num = integer_part * frac_den + frac_num;
                int den = frac_den;
                // GCD reduce
                int a = num, b = den;
                while (b) {
                    int t = b;
                    b = a % b;
                    a = t;
                }
                if (a > 0) {
                    num /= a;
                    den /= a;
                }
                if (num > 65535 || den > 65535) throw "parse_number: value too large for uint16_t rational";
                return {static_cast<std::uint16_t>(num), static_cast<std::uint16_t>(den)};
            }
            if (integer_part > 65535) throw "parse_number: value too large";
            return {static_cast<std::uint16_t>(integer_part), 1};
        }

        consteval void skip_spaces(const char* str, std::size_t& pos, std::size_t end) {
            while (pos < end && is_whitespace(str[pos])) pos++;
        }

        /// @brief Parse a single atom (note, rest, or bracketed group).
        consteval std::uint16_t parse_atom(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat);

        /// @brief Parse an atom with possible postfix operators (*N, /N, !N, @N, (k,n)).
        consteval std::uint16_t parse_postfix(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat);

        /// @brief Parse a sequence of space-separated elements (top-level or within brackets).
        consteval std::uint16_t parse_sequence(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat,
                                               char terminator = '\0');


        consteval std::uint16_t parse_timeline_values(const char* str, std::size_t& pos, std::size_t end,
                                                      parsed_pattern& pat) {
            // Expects: <N M ...> where N, M are integers
            if (pos >= end || str[pos] != '<') throw "parse_timeline_values: expected '<'";
            pos++; // skip '<'

            ast_node alt{};
            alt.type = ast_type::alternating;
            auto altIdx = pat.add_node(alt);

            skip_spaces(str, pos, end);
            while (pos < end && str[pos] != '>') {
                int val = parse_int(str, pos, end);
                if (val <= 0) throw "parse_timeline_values: values must be > 0";
                if (val > 255) throw "parse_timeline_values: value out of range (1-255)";
                // Store each value as a note_literal node (reusing note_value as integer storage)
                ast_node valNode{};
                valNode.type = ast_type::note_literal;
                valNode.note_value = static_cast<note>(val); // Repurposed for integer value
                auto valIdx = pat.add_node(valNode);
                pat.add_child(altIdx, valIdx);
                skip_spaces(str, pos, end);
            }
            if (pos >= end || str[pos] != '>') throw "parse_timeline_values: expected '>'";
            pos++; // skip '>'
            return altIdx;
        }


        consteval std::uint16_t parse_atom(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat) {
            skip_spaces(str, pos, end);
            if (pos >= end) throw "parse_atom: unexpected end of input";

            char c = str[pos];

            // Subdivision: [...] or stacked subdivision: [a, b, c]
            if (c == '[') {
                pos++; // skip '['
                auto seqIdx = parse_sequence(str, pos, end, pat, ']');

                // Check for comma -> stacked layers within subdivision
                if (pos < end && str[pos] == ',') {
                    ast_node stackNode{};
                    stackNode.type = ast_type::stacked;
                    auto stackIdx = pat.add_node(stackNode);

                    // First layer: wrap in subdivision
                    if (pat.nodes[seqIdx].type == ast_type::sequence) {
                        pat.nodes[seqIdx].type = ast_type::subdivision;
                    } else {
                        ast_node sub{};
                        sub.type = ast_type::subdivision;
                        auto subIdx = pat.add_node(sub);
                        pat.add_child(subIdx, seqIdx);
                        seqIdx = subIdx;
                    }
                    pat.add_child(stackIdx, seqIdx);

                    while (pos < end && str[pos] == ',') {
                        pos++; // skip ','
                        auto nextSeq = parse_sequence(str, pos, end, pat, ']');
                        // Wrap each layer as subdivision
                        if (pat.nodes[nextSeq].type == ast_type::sequence) {
                            pat.nodes[nextSeq].type = ast_type::subdivision;
                        } else {
                            ast_node sub{};
                            sub.type = ast_type::subdivision;
                            auto subIdx = pat.add_node(sub);
                            pat.add_child(subIdx, nextSeq);
                            nextSeq = subIdx;
                        }
                        pat.add_child(stackIdx, nextSeq);
                    }

                    if (pos >= end || str[pos] != ']') throw "parse_atom: expected ']'";
                    pos++; // skip ']'
                    return stackIdx;
                }

                if (pos >= end || str[pos] != ']') throw "parse_atom: expected ']'";
                pos++; // skip ']'
                // If parse_sequence returned a sequence node, just retype it as subdivision.
                // Otherwise it was unwrapped (single child optimization), so wrap it.
                if (pat.nodes[seqIdx].type == ast_type::sequence) {
                    pat.nodes[seqIdx].type = ast_type::subdivision;
                    return seqIdx;
                } else {
                    // Wrap the unwrapped child in a new subdivision node
                    ast_node sub{};
                    sub.type = ast_type::subdivision;
                    auto subIdx = pat.add_node(sub);
                    pat.add_child(subIdx, seqIdx);
                    return subIdx;
                }
            }

            // Alternating / Stacked: <...> or <..., ...>
            if (c == '<') {
                pos++; // skip '<'

                // Parse first group of children
                ast_node firstAlt{};
                firstAlt.type = ast_type::alternating;
                auto firstAltIdx = pat.add_node(firstAlt);

                skip_spaces(str, pos, end);
                while (pos < end && str[pos] != '>' && str[pos] != ',') {
                    auto child = parse_postfix(str, pos, end, pat);

                    // Check for @N elongation weight
                    std::uint8_t weight = 1;
                    if (pos < end && str[pos] == '@') {
                        pos++; // skip '@'
                        weight = static_cast<std::uint8_t>(parse_int(str, pos, end));
                    }

                    // If child is a replicate node, expand into N separate children
                    if (pat.nodes[child].type == ast_type::replicate && pat.nodes[child].child_count > 0) {
                        int n = static_cast<int>(pat.nodes[child].modifier_num);
                        if (pat.nodes[child].modifier_den > 1) n = 1; // fractional: treat as 1
                        if (n <= 0) n = 1;
                        auto innerChild = pat.nodes[child].children[0];
                        for (int r = 0; r < n; ++r) pat.add_child(firstAltIdx, innerChild, weight);
                    } else {
                        pat.add_child(firstAltIdx, child, weight);
                    }
                    skip_spaces(str, pos, end);
                }

                // Check for comma -> multi-layer stacked mode
                if (pos < end && str[pos] == ',') {
                    ast_node stackNode{};
                    stackNode.type = ast_type::stacked;
                    auto stackIdx = pat.add_node(stackNode);
                    pat.add_child(stackIdx, firstAltIdx);

                    while (pos < end && str[pos] == ',') {
                        pos++; // skip ','

                        ast_node nextAlt{};
                        nextAlt.type = ast_type::alternating;
                        auto nextAltIdx = pat.add_node(nextAlt);

                        skip_spaces(str, pos, end);
                        while (pos < end && str[pos] != '>' && str[pos] != ',') {
                            auto child = parse_postfix(str, pos, end, pat);

                            std::uint8_t weight = 1;
                            if (pos < end && str[pos] == '@') {
                                pos++;
                                weight = static_cast<std::uint8_t>(parse_int(str, pos, end));
                            }

                            if (pat.nodes[child].type == ast_type::replicate && pat.nodes[child].child_count > 0) {
                                int n = static_cast<int>(pat.nodes[child].modifier_num);
                                if (pat.nodes[child].modifier_den > 1) n = 1;
                                if (n <= 0) n = 1;
                                auto innerChild = pat.nodes[child].children[0];
                                for (int r = 0; r < n; ++r) pat.add_child(nextAltIdx, innerChild, weight);
                            } else {
                                pat.add_child(nextAltIdx, child, weight);
                            }
                            skip_spaces(str, pos, end);
                        }

                        pat.add_child(stackIdx, nextAltIdx);
                    }

                    if (pos >= end || str[pos] != '>') throw "parse_atom: expected '>'";
                    pos++; // skip '>'
                    return stackIdx;
                }

                // No commas - standard alternating
                if (pos >= end || str[pos] != '>') throw "parse_atom: expected '>'";
                pos++; // skip '>'
                return firstAltIdx;
            }

            // Polymetric / braces: {...}
            if (c == '{') {
                pos++; // skip '{'
                ast_node poly{};
                poly.type = ast_type::polymetric;
                auto polyIdx = pat.add_node(poly);

                // Parse comma-separated layers
                auto layer = parse_sequence(str, pos, end, pat, '}');
                pat.add_child(polyIdx, layer);

                while (pos < end && str[pos] == ',') {
                    pos++; // skip ','
                    layer = parse_sequence(str, pos, end, pat, '}');
                    pat.add_child(polyIdx, layer);
                }

                if (pos >= end || str[pos] != '}') throw "parse_atom: expected '}'";
                pos++; // skip '}'

                // Check for polymeter: {a b c}%N
                if (pos < end && str[pos] == '%') {
                    pos++; // skip '%'
                    int steps = parse_int(str, pos, end);
                    if (steps <= 0) throw "parse_atom: polymeter steps must be > 0";
                    if (steps > 255) throw "parse_atom: polymeter steps out of range (1-255)";
                    pat.nodes[polyIdx].polymeter_steps = static_cast<std::uint8_t>(steps);
                }

                return polyIdx;
            }

            // Note or rest or drum
            if (is_note_start(c)) {
                auto [noteVal, consumed] = parse_note_name(str, pos, end);
                pos += consumed;

                ast_node node{};
                node.type = ast_type::note_literal;
                node.note_value = noteVal;
                return pat.add_node(node);
            }

            throw "parse_atom: unexpected character";
        }


        consteval std::uint16_t parse_postfix(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat) {
            auto atom = parse_atom(str, pos, end, pat);

            // Check for postfix operators (can chain)
            while (pos < end) {
                char c = str[pos];

                // Fast: *N, *2.75, or *<N M>
                if (c == '*') {
                    pos++;
                    ast_node node{};
                    node.type = ast_type::fast;
                    auto nodeIdx = pat.add_node(node);
                    pat.add_child(nodeIdx, atom);

                    if (pos < end && str[pos] == '<') {
                        auto timeline = parse_timeline_values(str, pos, end, pat);
                        pat.add_child(nodeIdx, timeline);
                        pat.nodes[nodeIdx].modifier_is_timeline = true;
                    } else {
                        auto value = parse_number(str, pos, end);
                        if (value.num == 0) throw "parse_postfix: fast multiplier must be > 0";
                        pat.nodes[nodeIdx].modifier_num = value.num;
                        pat.nodes[nodeIdx].modifier_den = value.den;
                    }
                    atom = nodeIdx;
                    continue;
                }

                // Slow: /N, /1.5, or /<N M>
                if (c == '/') {
                    pos++;
                    ast_node node{};
                    node.type = ast_type::slow;
                    auto nodeIdx = pat.add_node(node);
                    pat.add_child(nodeIdx, atom);

                    if (pos < end && str[pos] == '<') {
                        auto timeline = parse_timeline_values(str, pos, end, pat);
                        pat.add_child(nodeIdx, timeline);
                        pat.nodes[nodeIdx].modifier_is_timeline = true;
                    } else {
                        auto value = parse_number(str, pos, end);
                        if (value.num == 0) throw "parse_postfix: slow divisor must be > 0";
                        pat.nodes[nodeIdx].modifier_num = value.num;
                        pat.nodes[nodeIdx].modifier_den = value.den;
                    }
                    atom = nodeIdx;
                    continue;
                }

                // Replicate: !N
                if (c == '!') {
                    pos++;
                    ast_node node{};
                    node.type = ast_type::replicate;
                    auto nodeIdx = pat.add_node(node);
                    pat.add_child(nodeIdx, atom);
                    auto value = parse_number(str, pos, end);
                    if (value.num == 0) throw "parse_postfix: replicate count must be > 0";
                    pat.nodes[nodeIdx].modifier_num = value.num;
                    pat.nodes[nodeIdx].modifier_den = value.den;
                    atom = nodeIdx;
                    continue;
                }

                // Elongation: @N
                if (c == '@') {
                    // Don't consume '@' - let the sequence parser handle it
                    break;
                }

                // Euclidean: (k,n) or (k,n,r)
                if (c == '(') {
                    pos++; // skip '('
                    int k = parse_int(str, pos, end);
                    if (pos >= end || str[pos] != ',') throw "parse_postfix: expected ',' in euclidean (k,n)";
                    pos++; // skip ','
                    int n = parse_int(str, pos, end);
                    int r = 0;
                    if (pos < end && str[pos] == ',') {
                        pos++; // skip ','
                        r = parse_int(str, pos, end);
                    }
                    if (pos >= end || str[pos] != ')') throw "parse_postfix: expected ')' in euclidean";
                    pos++; // skip ')'

                    if (k <= 0) throw "parse_postfix: euclidean pulses must be > 0";
                    if (n <= 0) throw "parse_postfix: euclidean steps must be > 0";
                    if (n > 64) throw "parse_postfix: euclidean steps out of range (1-64)";
                    if (k > n) throw "parse_postfix: euclidean pulses must be <= steps";
                    if (r < 0) throw "parse_postfix: euclidean rotation must be >= 0";
                    if (r > 255) throw "parse_postfix: euclidean rotation out of range (0-255)";

                    ast_node node{};
                    node.type = ast_type::euclidean;
                    auto nodeIdx = pat.add_node(node);
                    pat.add_child(nodeIdx, atom);
                    pat.nodes[nodeIdx].euclidean_k = static_cast<std::uint8_t>(k);
                    pat.nodes[nodeIdx].euclidean_n = static_cast<std::uint8_t>(n);
                    pat.nodes[nodeIdx].euclidean_r = static_cast<std::uint8_t>(r);
                    atom = nodeIdx;
                    continue;
                }

                break; // No more postfix operators
            }

            return atom;
        }


        consteval std::uint16_t parse_sequence(const char* str, std::size_t& pos, std::size_t end, parsed_pattern& pat,
                                               char terminator) {
            ast_node seq{};
            seq.type = ast_type::sequence;
            auto seqIdx = pat.add_node(seq);

            skip_spaces(str, pos, end);
            while (pos < end && str[pos] != terminator && str[pos] != ',' && str[pos] != '}') {
                auto child = parse_postfix(str, pos, end, pat);

                // Check for @N elongation weight
                std::uint8_t weight = 1;
                if (pos < end && str[pos] == '@') {
                    pos++; // skip '@'
                    weight = static_cast<std::uint8_t>(parse_int(str, pos, end));
                }

                pat.add_child(seqIdx, child, weight);
                skip_spaces(str, pos, end);
            }

            // If only one child with weight 1, unwrap
            if (pat.nodes[seqIdx].child_count == 1 && pat.nodes[seqIdx].weights[0] == 1) {
                return pat.nodes[seqIdx].children[0];
            }

            return seqIdx;
        }

    } // namespace parse_detail


    /// @brief Parse a mini-notation string at compile time.
    ///
    /// @param str The mini-notation string.
    /// @param len The string length (excluding null terminator).
    /// @return A parsed_pattern containing the AST.
    consteval parsed_pattern parse_mini(const char* str, std::size_t len) {
        parsed_pattern pat{};
        std::size_t pos = 0;
        auto root = parse_detail::parse_sequence(str, pos, len, pat);
        pat.root = root;

        // Verify we consumed everything
        parse_detail::skip_spaces(str, pos, len);
        if (pos != len) throw "parse_mini: unexpected trailing characters";

        return pat;
    }

    /// @brief Parse a mini-notation string literal at compile time.
    template<std::size_t N>
    consteval parsed_pattern parse_mini(const char (&str)[N]) {
        return parse_mini(str, N - 1); // N includes null terminator
    }

} // namespace gba::music
