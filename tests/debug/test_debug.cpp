/// @file tests/debug/test_debug.cpp
/// @brief Tests for gba::log module.
///
/// Run in mGBA with debug console to see output.

#include <gba/format>
#include <gba/logger>

#include <mgba_test.hpp>

using namespace gba::format::literals;

int main() {
    // Auto-detect and initialize
    bool available = gba::log::init();

    if (available) {
        gba::log::info("=== Logger Module Test ===");

        // C string overloads
        gba::log::info("Simple C string message");
        gba::log::warn("This is a warning");
        gba::log::error("This is an error");
        gba::log::debug("This is a debug message");

        // Formatted output
        int hp = 42;
        int max_hp = 100;
        gba::log::info("HP: {hp}/{max}"_fmt, "hp"_arg = hp, "max"_arg = max_hp);

        // Runtime level selection
        gba::log::level lvl = gba::log::level::info;
        gba::log::write(lvl, "Message with runtime level");
        gba::log::write(lvl, "Value: {x}"_fmt, "x"_arg = 123);

        // Hex output
        unsigned int addr = 0x08000000;
        gba::log::info("ROM addr: {addr:X}"_fmt, "addr"_arg = addr);

        // Negative numbers
        int delta = -5;
        gba::log::info("Delta: {d}"_fmt, "d"_arg = delta);

        // String arguments
        const char* player = "Hero";
        gba::log::info("Player: {name}"_fmt, "name"_arg = player);

        // Large number
        gba::log::info("Frame: {n}"_fmt, "n"_arg = 1234567890);

        // Test level filtering
        gba::log::set_level(gba::log::level::warn);
        auto written = gba::log::debug("This should not appear");
        ASSERT_EQ(written, 0u);

        gba::log::set_level(gba::log::level::debug);
        written = gba::log::debug("This should appear");
        ASSERT_NZ(written);

        gba::log::info("=== Test Complete ===");
    }

    ASSERT_TRUE(true);
}
