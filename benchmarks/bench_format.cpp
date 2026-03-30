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

    // Volatile values to prevent constant folding
    volatile int val_hp = 42;
    volatile int val_max = 100;
    volatile int val_gold = 9999;
    volatile int val_addr = 0xFF80;

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
