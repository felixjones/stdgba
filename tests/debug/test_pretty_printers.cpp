/// @file tests/debug/test_pretty_printers.cpp
/// @brief Test file for GDB pretty printers.
///
/// This file creates instances of all types with GDB pretty printers.
/// To test: set a breakpoint at the "// BREAKPOINT HERE" comment and
/// inspect variables in the debugger.
///
/// Load pretty printers with:
/// source D:/CLionProjects/stdgba/gdb/stdgba.py

#include <gba/angle>
#include <gba/fixed_point>
#include <gba/format>
#include <gba/keyinput>
#include <gba/logger>
#include <gba/memory>
#include <gba/peripherals>
#include <gba/testing>

using namespace gba::literals;

// Prevent optimizer from removing variables
template<typename T>
void use(T const& val) {
    asm volatile("" : : "m"(val));
}

int main() {
    // === fixed_point.py ===

    // gba::fixed<Rep, FracBits, IntermediateRep>
    gba::fixed<short, 8> fix8_val = 3.5_fx;
    gba::fixed<int, 16> fix16_val = -1.25_fx;
    gba::fixed<int, 8> fix24_8_val = 42.125_fx;

    use(fix8_val);
    use(fix16_val);
    use(fix24_8_val);

    // === angle.py ===

    // gba::angle
    gba::angle angle_90 = 90_deg;
    gba::angle angle_45 = 45_deg;
    gba::angle angle_180 = 180_deg;

    // gba::packed_angle<Bits>
    gba::packed_angle<16> packed16 = 90_deg;
    gba::packed_angle<8> packed8 = 180_deg;
    gba::packed_angle<12> packed12 = 45_deg;

    use(angle_90);
    use(angle_45);
    use(angle_180);
    use(packed16);
    use(packed8);
    use(packed12);

    // === format.py ===

    // gba::format::compiled_format (stateless, info in type)
    constexpr auto fmt_simple = "Hello World"_fmt;
    constexpr auto fmt_args = "HP: {hp}/{max}"_fmt;

    // gba::format::arg_binder (stateless, info in type)
    constexpr auto arg_hp = "hp"_arg;
    constexpr auto arg_max = "max"_arg;

    // gba::format::bound_arg
    auto bound_hp = "hp"_arg = 42;
    auto bound_max = "max"_arg = 100;
    auto bound_str = "name"_arg = "Hero";

    // gba::format::format_generator
    auto gen = fmt_args.generator("hp"_arg = 10, "max"_arg = 50);

    // Advance generator a bit
    gen();
    gen();
    gen();

    use(fmt_simple);
    use(fmt_args);
    use(arg_hp);
    use(arg_max);
    use(bound_hp);
    use(bound_max);
    use(bound_str);
    use(gen);

    // === log.py ===

    // gba::log::level
    gba::log::level level_info = gba::log::level::info;
    gba::log::level level_warn = gba::log::level::warn;
    gba::log::level level_debug = gba::log::level::debug;
    gba::log::level level_error = gba::log::level::error;

    use(level_info);
    use(level_warn);
    use(level_debug);
    use(level_error);

    // === registral.py ===
    // Note: These are compile-time register wrappers, they just hold addresses
    // The pretty printer will show the address and attempt to read memory

    // Example registrals (these are just address wrappers, safe to create)
    // In actual use, these would be the predefined reg_* variables
    constexpr auto test_reg_u16 = gba::registral<unsigned short>{0x04000000};
    constexpr auto test_reg_u32 = gba::registral<unsigned int>{0x04000004};
    constexpr auto test_reg_array = gba::registral<short[256]>{0x05000000};

    use(test_reg_u16);
    use(test_reg_u32);
    use(test_reg_array);

    // === keyinput.py ===

    // gba::key (button mask)
    auto key_single = gba::key_a;
    auto key_combo = gba::key_a | gba::key_b;
    auto key_dpad = gba::key_up | gba::key_right;
    auto key_shoulders = gba::key_l | gba::key_r;

    use(key_single);
    use(key_combo);
    use(key_dpad);
    use(key_shoulders);

    // gba::keypad (state tracker)
    gba::keypad keypad_empty; // No keys pressed (default 0x3ff)

    use(keypad_empty);

    // === memory.py ===

    // gba::plex (tuple-like for registers)
    gba::plex<short, short> plex2{short{100}, short{200}};
    gba::plex<char, char, char, char> plex4{'R', 'G', 'B', 'A'};

    use(plex2);
    use(plex4);

    // gba::unique (smart pointer)
    auto unique_int = gba::make_unique<int>(42);
    gba::unique<int> unique_null{nullptr};

    use(unique_int);
    use(unique_null);

    // gba::bitpool (memory allocator)
    // Create a small test buffer for the pool
    alignas(4) char test_buffer[1024];
    gba::bitpool test_pool{test_buffer, 32}; // 32 chunks of 32 bytes
    void* alloc1 = test_pool.allocate(64);   // Allocate 2 chunks
    void* alloc2 = test_pool.allocate(32);   // Allocate 1 chunk

    use(test_pool);
    use(alloc1);
    use(alloc2);

    // BREAKPOINT HERE - inspect all variables above
    asm volatile("nop");

    return gba::test.finish();
}
