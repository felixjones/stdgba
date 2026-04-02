/// @file benchmarks/bench_format.cpp
/// @brief Benchmark measuring format string rendering cost.
///
/// Measures several scenarios:
///   1. Literal-only format (no placeholders).
///   2. Single integer placeholder (decimal).
///   3. Single string placeholder.
///   4. Multiple named placeholders (mixed int + string).
///   5. Hex formatting.
///   6. Typewriter generator per-character cost.
///   7. to_array convenience wrapper.
///
/// Uses gba::benchmark helpers. Run in mgba-headless to see output.

#include <gba/angle>
#include <gba/fixed_point>
#include <gba/format>

#include "bench.hpp"

using namespace gba::literals;

namespace {

    constexpr int iters = 256;

    volatile int sink_len = 0;
    volatile char sink_char = 0;

    // -- Format strings (consteval compiled) --------------------------------------

    constexpr auto fmt_literal = "Hello, World!"_fmt;
    constexpr auto fmt_int = "HP: {hp}"_fmt;
    constexpr auto fmt_string = "Name: {name}"_fmt;
    constexpr auto fmt_multi = "HP: {hp}/{max} ({name})"_fmt;
    constexpr auto fmt_hex = "Addr: {a:X}"_fmt;
    constexpr auto fmt_long = "Player {name} has {hp}/{max} HP and {gold} gold"_fmt;
    constexpr auto fmt_grouped = "Gold: {gold:_d}"_fmt;
    constexpr auto fmt_fixed = "X: {x}"_fmt;
    constexpr auto fmt_fixed_prec = "X: {x:.2f}"_fmt;
    constexpr auto fmt_fixed_grouped = "X: {x:,.2f}"_fmt;
    constexpr auto fmt_angle_deg = "Angle: {a}"_fmt;
    constexpr auto fmt_angle_rad = "Angle: {a:.4r}"_fmt;
    constexpr auto fmt_angle_hex = "Angle: {a:#.4X}"_fmt;
    constexpr auto fmt_fixed_pct = "X: {x:%}"_fmt;
    constexpr auto fmt_fixed_sci = "X: {x:.2e}"_fmt;
    constexpr auto fmt_fixed_general = "X: {x:.4g}"_fmt;

    // Volatile values to prevent constant folding
    volatile int val_hp = 42;
    volatile int val_max = 100;
    volatile int val_gold = 9999;
    volatile int val_addr = 0xFF80;
    volatile int val_fixed_bits = 1234 << 8 | 128;
    volatile unsigned int val_angle_bits = 0x40000000u;
    volatile unsigned short val_packed_angle16_bits = 0x4000u;

    using fix8 = gba::fixed<int, 8>;

    [[gnu::always_inline]]
    inline fix8 load_fixed(volatile int& raw_bits) {
        const auto raw = raw_bits;
        return __builtin_bit_cast(fix8, raw);
    }

    [[gnu::always_inline]]
    inline gba::angle load_angle(volatile unsigned int& raw_bits) {
        const auto raw = raw_bits;
        return gba::angle{raw};
    }

    [[gnu::always_inline]]
    inline gba::packed_angle16 load_packed_angle16(volatile unsigned short& raw_bits) {
        const auto raw = raw_bits;
        return gba::packed_angle16{raw};
    }

    // -- Benchmark functions ------------------------------------------------------

    // 1. Literal-only: "Hello, World!" (no placeholders)
    [[gnu::noinline]]
    void format_literal_to_buf() {
        char buf[64];
        auto len = fmt_literal.to(buf);
        sink_len = static_cast<int>(len);
    }

    // 2. Single integer: "HP: {hp}"
    [[gnu::noinline]]
    void format_int_to_buf() {
        char buf[64];
        auto len = fmt_int.to(buf, "hp"_arg = val_hp);
        sink_len = static_cast<int>(len);
    }

    // 3. Single string: "Name: {name}"
    [[gnu::noinline]]
    void format_string_to_buf() {
        char buf[64];
        auto len = fmt_string.to(buf, "name"_arg = "Hero");
        sink_len = static_cast<int>(len);
    }

    // 4. Multiple placeholders: "HP: {hp}/{max} ({name})"
    [[gnu::noinline]]
    void format_multi_to_buf() {
        char buf[64];
        auto len = fmt_multi.to(buf, "hp"_arg = val_hp, "max"_arg = val_max, "name"_arg = "Hero");
        sink_len = static_cast<int>(len);
    }

    // 5. Hex formatting: "Addr: {a:X}"
    [[gnu::noinline]]
    void format_hex_to_buf() {
        char buf[64];
        auto len = fmt_hex.to(buf, "a"_arg = val_addr);
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_grouped_to_buf() {
        char buf[64];
        auto len = fmt_grouped.to(buf, "gold"_arg = val_gold);
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_to_buf() {
        char buf[64];
        auto len = fmt_fixed.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_prec_to_buf() {
        char buf[64];
        auto len = fmt_fixed_prec.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_grouped_to_buf() {
        char buf[64];
        auto len = fmt_fixed_grouped.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_angle_deg_to_buf() {
        char buf[64];
        auto len = fmt_angle_deg.to(buf, "a"_arg = load_angle(val_angle_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_angle_rad_to_buf() {
        char buf[64];
        auto len = fmt_angle_rad.to(buf, "a"_arg = load_angle(val_angle_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_angle_hex_to_buf() {
        char buf[64];
        auto len = fmt_angle_hex.to(buf, "a"_arg = load_packed_angle16(val_packed_angle16_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_pct_to_buf() {
        char buf[64];
        auto len = fmt_fixed_pct.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_sci_to_buf() {
        char buf[64];
        auto len = fmt_fixed_sci.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    [[gnu::noinline]]
    void format_fixed_general_to_buf() {
        char buf[64];
        auto len = fmt_fixed_general.to(buf, "x"_arg = load_fixed(val_fixed_bits));
        sink_len = static_cast<int>(len);
    }

    // 6. Long multi-arg: "Player {name} has {hp}/{max} HP and {gold} gold"
    [[gnu::noinline]]
    void format_long_to_buf() {
        char buf[96];
        auto len = fmt_long.to(buf, "name"_arg = "Hero", "hp"_arg = val_hp, "max"_arg = val_max, "gold"_arg = val_gold);
        sink_len = static_cast<int>(len);
    }

    // 7. Generator (typewriter) - drain full string character by character
    [[gnu::noinline]]
    void format_generator_drain() {
        auto gen = fmt_multi.generator("hp"_arg = val_hp, "max"_arg = val_max, "name"_arg = "Hero");
        int count = 0;
        while (auto ch = gen()) {
            sink_char = *ch;
            ++count;
        }
        sink_len = count;
    }

    // 8. to_array convenience
    [[gnu::noinline]]
    void format_to_array() {
        auto arr = fmt_multi.to_array("hp"_arg = val_hp, "max"_arg = val_max, "name"_arg = "Hero");
        sink_char = arr[0];
    }

} // namespace

int main() {
    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "=== format benchmark (cycles) ===");
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "  %-36s  %6s  %6s", "Case", "single", "avg");
    });

    // -- Literal only ---------------------------------------------------------
    {
        auto single = bench::measure(format_literal_to_buf);
        auto avg = bench::measure_avg(iters, format_literal_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "literal \"Hello, World!\"", single, avg);
        });
    }

    // -- Single integer -------------------------------------------------------
    {
        auto single = bench::measure(format_int_to_buf);
        auto avg = bench::measure_avg(iters, format_int_to_buf);
        bench::with_logger(
            [&] { bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "int \"HP: {hp}\"", single, avg); });
    }

    // -- Single string --------------------------------------------------------
    {
        auto single = bench::measure(format_string_to_buf);
        auto avg = bench::measure_avg(iters, format_string_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "string \"Name: {name}\"", single, avg);
        });
    }

    // -- Multiple args --------------------------------------------------------
    {
        auto single = bench::measure(format_multi_to_buf);
        auto avg = bench::measure_avg(iters, format_multi_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "multi \"HP: {hp}/{max} ({name})\"", single,
                              avg);
        });
    }

    // -- Hex formatting -------------------------------------------------------
    {
        auto single = bench::measure(format_hex_to_buf);
        auto avg = bench::measure_avg(iters, format_hex_to_buf);
        bench::with_logger(
            [&] { bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "hex \"Addr: {a:X}\"", single, avg); });
    }

    // -- Grouped integer ------------------------------------------------------
    {
        auto single = bench::measure(format_grouped_to_buf);
        auto avg = bench::measure_avg(iters, format_grouped_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "grouped int \"Gold: {gold:_d}\"", single,
                              avg);
        });
    }

    // -- Fixed-point default --------------------------------------------------
    {
        auto single = bench::measure(format_fixed_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_to_buf);
        bench::with_logger(
            [&] { bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed default \"X: {x}\"", single, avg); });
    }

    // -- Fixed-point precision ------------------------------------------------
    {
        auto single = bench::measure(format_fixed_prec_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_prec_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed \"X: {x:.2f}\"", single, avg);
        });
    }

    // -- Fixed-point grouped --------------------------------------------------
    {
        auto single = bench::measure(format_fixed_grouped_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_grouped_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed grouped \"X: {x:,.2f}\"", single,
                              avg);
        });
    }

    // -- Angle degrees --------------------------------------------------------
    {
        auto single = bench::measure(format_angle_deg_to_buf);
        auto avg = bench::measure_avg(iters, format_angle_deg_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "angle deg \"Angle: {a}\"", single, avg);
        });
    }

    // -- Angle radians --------------------------------------------------------
    {
        auto single = bench::measure(format_angle_rad_to_buf);
        auto avg = bench::measure_avg(iters, format_angle_rad_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "angle rad \"Angle: {a:.4r}\"", single,
                              avg);
        });
    }

    // -- Angle raw hex --------------------------------------------------------
    {
        auto single = bench::measure(format_angle_hex_to_buf);
        auto avg = bench::measure_avg(iters, format_angle_hex_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "angle hex \"Angle: {a:#.4X}\"", single,
                              avg);
        });
    }

    // -- Fixed-point percent --------------------------------------------------
    {
        auto single = bench::measure(format_fixed_pct_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_pct_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed pct \"X: {x:%%}\"", single, avg);
        });
    }

    // -- Fixed-point scientific -----------------------------------------------
    {
        auto single = bench::measure(format_fixed_sci_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_sci_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed sci \"X: {x:.2e}\"", single, avg);
        });
    }

    // -- Fixed-point general --------------------------------------------------
    {
        auto single = bench::measure(format_fixed_general_to_buf);
        auto avg = bench::measure_avg(iters, format_fixed_general_to_buf);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "fixed general \"X: {x:.4g}\"", single, avg);
        });
    }

    // -- Long multi-arg -------------------------------------------------------
    {
        auto single = bench::measure(format_long_to_buf);
        auto avg = bench::measure_avg(iters, format_long_to_buf);
        bench::with_logger(
            [&] { bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "long 4-arg mixed", single, avg); });
    }

    // -- Generator drain ------------------------------------------------------
    {
        auto single = bench::measure(format_generator_drain);
        auto avg = bench::measure_avg(iters, format_generator_drain);
        bench::with_logger([&] {
            bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "generator drain (typewriter)", single, avg);
        });
    }

    // -- to_array convenience -------------------------------------------------
    {
        auto single = bench::measure(format_to_array);
        auto avg = bench::measure_avg(iters, format_to_array);
        bench::with_logger(
            [&] { bench::log_printf(gba::log::level::info, "  %-36s  %6u  %6u", "to_array<64> multi", single, avg); });
    }

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
