/**
 * @file bits/format/generator.hpp
 * @brief Typewriter generator with MSB-first formatting.
 */
#pragma once
#include <gba/bits/format/parse.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace gba::format {

// User-Provided Workspace

/**
 * @brief External workspace for user-provided memory.
 *
 * Use this when you want to control where formatting scratch space is allocated.
 *
 * @code{.cpp}
 * char my_buffer[64];
 * auto gen = fmt.generator_with(
 *     external_workspace{my_buffer, sizeof(my_buffer)},
 *     "x"_arg = 42
 * );
 * @endcode
 */
struct external_workspace {
    char* buffer;
    std::size_t capacity;
    std::size_t used = 0;

    constexpr external_workspace(char* buf, std::size_t cap)
        : buffer{buf}, capacity{cap} {}

    constexpr char* allocate(std::size_t n) {
        if (used + n > capacity) return nullptr;
        char* ptr = buffer + used;
        used += n;
        return ptr;
    }

    constexpr void reset() { used = 0; }
};

/**
 * @brief Internal workspace with compile-time sized buffer.
 */
template<std::size_t Size>
struct internal_workspace {
    std::array<char, Size> buffer{};
    std::size_t used = 0;

    constexpr char* allocate(std::size_t n) {
        if (used + n > Size) return nullptr;
        char* ptr = buffer.data() + used;
        used += n;
        return ptr;
    }

    constexpr void reset() { used = 0; }
};

// Detail namespace

namespace detail {
// Power-of-10 lookup table for consteval formatting
inline constexpr std::uint32_t pow10[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};
constexpr std::uint8_t count_digits(std::uint32_t value) {
    if (value < 10) return 1;
    if (value < 100) return 2;
    if (value < 1000) return 3;
    if (value < 10000) return 4;
    if (value < 100000) return 5;
    if (value < 1000000) return 6;
    if (value < 10000000) return 7;
    if (value < 100000000) return 8;
    if (value < 1000000000) return 9;
    return 10;
}
struct msb_emitter {
    char digits[10];      // Pre-computed digits (max 10 for 32-bit)
    std::uint8_t count;   // Total digit count
    std::uint8_t pos;     // Current emission position

    constexpr msb_emitter() : digits{}, count{0}, pos{0} {}
    constexpr msb_emitter(std::uint32_t v) : digits{}, count{0}, pos{0} {
        if (v == 0) {
            digits[0] = '0';
            count = 1;
            return;
        }
        // Extract digits LSB-first into temp storage
        std::uint8_t idx = 0;
        while (v > 0) {
            digits[idx++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        count = idx;
        // Reverse in place to get MSB-first order
        for (std::uint8_t i = 0; i < count / 2; ++i) {
            const char tmp = digits[i];
            digits[i] = digits[count - 1 - i];
            digits[count - 1 - i] = tmp;
        }
    }
    [[nodiscard]] constexpr bool has_next() const { return pos < count; }
    constexpr char next() {
        if (pos >= count) return 0;
        return digits[pos++];
    }
};
struct hex_emitter {
    std::uint32_t value;
    std::uint8_t remaining_nibbles;
    bool uppercase;
    constexpr hex_emitter(std::uint32_t v, bool upper)
        : value{v}, remaining_nibbles{0}, uppercase{upper}
    {
        if (v == 0) remaining_nibbles = 1;
        else { auto tmp = v; while (tmp > 0) { ++remaining_nibbles; tmp >>= 4; } }
    }
    [[nodiscard]] constexpr bool has_next() const { return remaining_nibbles > 0; }
    constexpr char next() {
        if (remaining_nibbles == 0) return 0;
        --remaining_nibbles;
        std::uint8_t nibble = (value >> (remaining_nibbles * 4)) & 0xF;
        if (nibble < 10) return static_cast<char>('0' + nibble);
        return static_cast<char>((uppercase ? 'A' : 'a') + nibble - 10);
    }
};
} // namespace detail
constexpr bool is_break_char(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\0';
}

struct arg_stream_state {
    enum class phase : std::uint8_t { not_started, sign, digits, complete };
    enum class kind : std::uint8_t { none, decimal, hex, string };

    phase current_phase = phase::not_started;
    kind value_kind = kind::none;

    union {
        detail::msb_emitter decimal;
        detail::hex_emitter hex;
        struct {
            const char* ptr;
            std::size_t pos;
            std::size_t len;
        } str;
    } emitter{};

    char sign_char = '\0';
    bool sign_emitted = false;

    constexpr arg_stream_state() : emitter{} {}
};
template<typename... BoundArgs>
struct arg_pack {
    std::tuple<BoundArgs...> args;
    constexpr arg_pack(BoundArgs... a) : args{std::move(a)...} {}
};
template<> struct arg_pack<> { std::tuple<> args; constexpr arg_pack() = default; };
template<unsigned int Hash, typename T>
struct bound_arg {
    static constexpr unsigned int hash = Hash;
    static constexpr bool is_supplier = std::invocable<T>;
    T stored;
    constexpr decltype(auto) get() const {
        if constexpr (is_supplier) return stored();
        else return stored;
    }
};
template<fixed_string Name>
struct arg_binder {
    static constexpr auto hash = fnv1a_hash<Name>();
    template<typename T> constexpr auto operator()(T&& v) const { return bound_arg<hash, std::decay_t<T>>{std::forward<T>(v)}; }
    template<typename T> constexpr auto operator=(T&& v) const { return bound_arg<hash, std::decay_t<T>>{std::forward<T>(v)}; }
};

template<fixed_string Fmt, typename ArgPack>
struct format_generator {
    static constexpr auto ast = parse_format<Fmt>();
    static constexpr std::size_t segment_count = ast.segment_count;

    ArgPack args;
    std::size_t segment_idx = 0;
    std::size_t char_idx = 0;
    arg_stream_state arg_state{};
    bool arg_initialized = false;

    constexpr format_generator(ArgPack a) : args{std::move(a)} {}

    std::optional<char> operator()() { return next(); }
    std::optional<char> next() {
        while (segment_idx < segment_count) {
            const auto& seg = ast.segments[segment_idx];
            if (seg.type == segment_type::literal) {
                if (char_idx < seg.lit_length) {
                    return ast.format_str.data[seg.lit_start + char_idx++];
                }
                advance_segment();
            } else {
                auto ch = next_arg_char(seg);
                if (ch) return ch;
                advance_segment();
            }
        }
        return std::nullopt;
    }
    [[nodiscard]] bool done() const { return segment_idx >= segment_count; }
    [[nodiscard]] std::size_t until_break() const {
        if (done()) return 0;
        std::size_t count = 0;
        std::size_t seg = segment_idx;
        std::size_t idx = char_idx;
        while (seg < segment_count) {
            const auto& s = ast.segments[seg];
            if (s.type == segment_type::literal) {
                while (idx < s.lit_length) {
                    char c = ast.format_str.data[s.lit_start + idx];
                    if (is_break_char(c)) return count;
                    ++count; ++idx;
                }
            } else {
                count += 10;
            }
            ++seg; idx = 0;
        }
        return count;
    }
    void reset() { segment_idx = 0; char_idx = 0; arg_state = {}; arg_initialized = false; }
private:
    void advance_segment() { ++segment_idx; char_idx = 0; arg_state = {}; arg_initialized = false; }
    template<typename T>
    void init_arg_state(const T& value, const format_spec& spec) {
        arg_state = {};
        arg_state.current_phase = arg_stream_state::phase::digits;
        if constexpr (std::is_integral_v<T>) {
            using UT = std::make_unsigned_t<T>;
            UT abs_val;
            if constexpr (std::is_signed_v<T>) {
                if (value < 0) { arg_state.sign_char = '-'; abs_val = static_cast<UT>(-value); }
                else abs_val = static_cast<UT>(value);
            } else abs_val = value;
            if (spec.fmt_type == format_spec::type_t::hex_lower || spec.fmt_type == format_spec::type_t::hex_upper) {
                arg_state.value_kind = arg_stream_state::kind::hex;
                arg_state.emitter.hex = detail::hex_emitter{static_cast<std::uint32_t>(abs_val), spec.fmt_type == format_spec::type_t::hex_upper};
            } else {
                arg_state.value_kind = arg_stream_state::kind::decimal;
                arg_state.emitter.decimal = detail::msb_emitter{static_cast<std::uint32_t>(abs_val)};
            }
            if (arg_state.sign_char != '\0') arg_state.current_phase = arg_stream_state::phase::sign;
        } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
            arg_state.value_kind = arg_stream_state::kind::string;
            arg_state.emitter.str.ptr = value;
            arg_state.emitter.str.pos = 0;
            const char* p = value; while (*p) ++p;
            arg_state.emitter.str.len = p - value;
        }
        arg_initialized = true;
    }
    std::optional<char> next_arg_char(const format_segment& seg) {
        if (!arg_initialized) {
            if (seg.type == segment_type::named_placeholder) init_named_arg(seg.name_hash, seg.spec);
            else init_positional_arg(seg.pos_index, seg.spec);
        }
        return emit_next();
    }
    std::optional<char> emit_next() {
        switch (arg_state.current_phase) {
            case arg_stream_state::phase::sign:
                if (!arg_state.sign_emitted && arg_state.sign_char != '\0') {
                    arg_state.sign_emitted = true;
                    arg_state.current_phase = arg_stream_state::phase::digits;
                    return arg_state.sign_char;
                }
                arg_state.current_phase = arg_stream_state::phase::digits;
                [[fallthrough]];
            case arg_stream_state::phase::digits:
                switch (arg_state.value_kind) {
                    case arg_stream_state::kind::string:
                        if (arg_state.emitter.str.pos < arg_state.emitter.str.len)
                            return arg_state.emitter.str.ptr[arg_state.emitter.str.pos++];
                        break;
                    case arg_stream_state::kind::hex:
                        if (arg_state.emitter.hex.has_next()) return arg_state.emitter.hex.next();
                        break;
                    case arg_stream_state::kind::decimal:
                        if (arg_state.emitter.decimal.has_next()) return arg_state.emitter.decimal.next();
                        break;
                    default: break;
                }
                arg_state.current_phase = arg_stream_state::phase::complete;
                [[fallthrough]];
            default: return std::nullopt;
        }
    }
    template<std::size_t... Is>
    void init_named_impl(unsigned int hash, const format_spec& spec, std::index_sequence<Is...>) {
        (void)((std::tuple_element_t<Is, decltype(args.args)>::hash == hash
            ? (init_arg_state(std::get<Is>(args.args).get(), spec), true) : false) || ...);
    }
    void init_named_arg(unsigned int hash, const format_spec& spec) {
        init_named_impl(hash, spec, std::make_index_sequence<std::tuple_size_v<decltype(args.args)>>{});
    }
    template<std::size_t... Is>
    void init_positional_impl(std::uint8_t index, const format_spec& spec, std::index_sequence<Is...>) {
        std::size_t pos = 0;
        (void)((pos++ == index ? (init_arg_state(std::get<Is>(args.args).get(), spec), true) : false) || ...);
    }
    void init_positional_arg(std::uint8_t index, const format_spec& spec) {
        init_positional_impl(index, spec, std::make_index_sequence<std::tuple_size_v<decltype(args.args)>>{});
    }
};
template<fixed_string Fmt>
struct compiled_format {
    static constexpr auto fmt = Fmt;
    static constexpr auto ast = parse_format<Fmt>();
    static_assert(ast.valid, "Invalid format string");

    /**
     * @brief Create a typewriter generator.
     */
    template<typename... Args>
    auto generator(Args... args) const {
        using pack_t = arg_pack<Args...>;
        return format_generator<Fmt, pack_t>{pack_t{std::move(args)...}};
    }

    /**
     * @brief Create a generator with user-provided workspace.
     *
     * @code{.cpp}
     * char workspace[64];
     * auto gen = fmt.generator_with(
     *     external_workspace{workspace, sizeof(workspace)},
     *     "x"_arg = 42
     * );
     * @endcode
     */
    template<typename... Args>
    auto generator_with(external_workspace ws, Args... args) const {
        // For now, workspace is reserved for future use (placeholder caching)
        // The generator still works, workspace will be used for optimization
        (void)ws;
        return generator(std::move(args)...);
    }

    /**
     * @brief Format immediately to buffer.
     */
    template<typename... Args>
    std::size_t to(char* buf, Args... args) const {
        auto gen = generator(std::move(args)...);
        char* start = buf;
        while (auto ch = gen()) *buf++ = *ch;
        *buf = '\0';
        return buf - start;
    }

    /**
     * @brief Format to std::array (runtime).
     */
    template<std::size_t N = 64, typename... Args>
    constexpr auto to_array(Args... args) const {
        std::array<char, N> result{};
        to(result.data(), std::move(args)...);
        return result;
    }

    /**
     * @brief Compile-time format with constant arguments.
     *
     * When all arguments are compile-time constants, the entire formatted
     * string is computed at compile time.
     *
     * @code{.cpp}
     * constexpr auto msg = "Answer: {x}"_fmt.to_static<64>("x"_arg = 42);
     * // msg contains "Answer: 42" computed at compile time
     * @endcode
     */
    template<std::size_t N = 64, typename... Args>
    static consteval auto to_static(Args... args) {
        std::array<char, N> result{};
        std::size_t pos = 0;

        // Process each segment at compile time
        for (std::size_t i = 0; i < ast.segment_count && pos < N - 1; ++i) {
            const auto& seg = ast.segments[i];

            if (seg.type == segment_type::literal) {
                // Copy literal
                for (std::size_t j = 0; j < seg.lit_length && pos < N - 1; ++j) {
                    result[pos++] = ast.format_str.data[seg.lit_start + j];
                }
            } else if (seg.type == segment_type::named_placeholder) {
                // Format the argument
                pos = format_arg_static<N>(result, pos, seg.name_hash, seg.spec, args...);
            }
        }
        result[pos] = '\0';
        return result;
    }

    /**
     * @brief Shorthand for to_array.
     */
    template<typename... Args>
    constexpr auto operator()(Args... args) const { return to_array(std::move(args)...); }

private:
    template<std::size_t N, typename... Args>
    static consteval std::size_t format_arg_static(std::array<char, N>& out, std::size_t pos,
                                                    unsigned int hash, const format_spec& spec,
                                                    Args... args) {
        return format_arg_static_impl<N>(out, pos, hash, spec, args...);
    }

    template<std::size_t N, typename First, typename... Rest>
    static consteval std::size_t format_arg_static_impl(std::array<char, N>& out, std::size_t pos,
                                                         unsigned int hash, const format_spec& spec,
                                                         First first, Rest... rest) {
        if (First::hash == hash) {
            return format_value_static<N>(out, pos, first.stored, spec);
        }
        if constexpr (sizeof...(Rest) > 0) {
            return format_arg_static_impl<N>(out, pos, hash, spec, rest...);
        }
        return pos;
    }

    template<std::size_t N>
    static consteval std::size_t format_arg_static_impl(std::array<char, N>& out, std::size_t pos,
                                                         unsigned int, const format_spec&) {
        return pos;  // No args - shouldn't happen for valid format
    }

    template<std::size_t N, typename T>
    static consteval std::size_t format_value_static(std::array<char, N>& out, std::size_t pos,
                                                      const T& value, const format_spec& spec) {
        if constexpr (std::is_integral_v<T>) {
            // Format integer
            using UT = std::make_unsigned_t<T>;
            UT abs_val;
            bool negative = false;

            if constexpr (std::is_signed_v<T>) {
                if (value < 0) {
                    negative = true;
                    abs_val = static_cast<UT>(-value);
                } else {
                    abs_val = static_cast<UT>(value);
                }
            } else {
                abs_val = value;
            }

            if (negative && pos < N - 1) {
                out[pos++] = '-';
            }

            // Count digits
            std::uint8_t num_digits = detail::count_digits(static_cast<std::uint32_t>(abs_val));

            // Write digits MSB first
            for (std::uint8_t d = num_digits; d > 0 && pos < N - 1; --d) {
                std::uint32_t divisor = detail::pow10[d - 1];
                out[pos++] = static_cast<char>('0' + (abs_val / divisor));
                abs_val %= divisor;
            }
        } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                            std::is_same_v<std::decay_t<T>, char*>) {
            // Format string
            const char* p = value;
            while (*p && pos < N - 1) {
                out[pos++] = *p++;
            }
        }
        return pos;
    }
};

/**
 * @brief Create a compiled format from a format string.
 *
 * Non-literal alternative to `"..."_fmt`.
 *
 * @code{.cpp}
 * constexpr auto fmt = gba::format::make_format<"HP: {hp}/{max}">();
 * @endcode
 */
template<fixed_string Fmt>
consteval auto make_format() {
    return compiled_format<Fmt>{};
}

/**
 * @brief Create an argument binder for a named argument.
 *
 * Non-literal alternative to `"..."_arg`.
 *
 * @code{.cpp}
 * constexpr auto hp_arg = gba::format::make_arg<"hp">();
 * fmt.to(buf, hp_arg = 42);
 * @endcode
 */
template<fixed_string Name>
consteval auto make_arg() {
    return arg_binder<Name>{};
}

namespace literals {
    template<fixed_string Fmt> consteval auto operator""_fmt() { return compiled_format<Fmt>{}; }
    template<fixed_string Name> consteval auto operator""_arg() { return arg_binder<Name>{}; }
}
} // namespace gba::format
