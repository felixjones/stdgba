/**
 * @file tests/debug/test_assert.cpp
 * @brief Test for assert handler display.
 *
 * This test triggers an assert failure to verify the crash screen displays correctly.
 * It should show:
 * - Red "ASSERT FAILED" header
 * - The failed expression in yellow
 * - File, line, and function info in white
 *
 * Run this test manually in an emulator to visually verify the output.
 */

#include <cassert>

int main() {
    // This will trigger the assert handler
    int x = 5;
    int y = 10;

    // Deliberate failure to test the display
    assert(x > y && "x should be greater than y");

    // Should never reach here
    return 0;
}
