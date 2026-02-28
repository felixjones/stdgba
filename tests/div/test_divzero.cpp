/// @file tests/div/test_divzero.cpp
/// @brief Test for division by zero handler display.
///
/// This test triggers a division by zero to verify the crash screen displays correctly.
/// It should show:
/// - Red "DIVISION BY ZERO" header
/// - LR register value
/// - CPSR register value
/// - IME state
///
/// Run this test manually in an emulator to visually verify the output.

int main() {
    volatile int const zero = 0;
    volatile int const x = 42;

    // Deliberate division by zero to test the handler
    volatile int const result = x / zero;
    (void)result;

    // Should never reach here
    return 0;
}
