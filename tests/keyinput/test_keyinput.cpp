/// @file test_keyinput.cpp
/// @brief Tests for keypad input handling and edge detection.

#include <gba/keyinput>
#include <gba/testing>

#include <bit>

// Helper: construct a key_control from a raw bitmask.
// Hardware register is active-low (0 = pressed), so we invert.
static constexpr gba::key_control keys_from(unsigned short raw_pressed) {
    const auto inverted = static_cast<unsigned short>(~raw_pressed & 0x3FF);
    return __builtin_bit_cast(gba::key_control, inverted);
}

int main() {
    // key combine
    {
        auto combo = gba::key_a | gba::key_b;
        auto combo2 = gba::key_a;
        combo2 |= gba::key_b;
        // Both should produce the same bitmask
        gba::test.eq(__builtin_bit_cast(int, combo), __builtin_bit_cast(int, combo2));
    }

    // key constants have correct bits
    {
        gba::test.eq(__builtin_bit_cast(int, gba::key_a), 0x0001);
        gba::test.eq(__builtin_bit_cast(int, gba::key_b), 0x0002);
        gba::test.eq(__builtin_bit_cast(int, gba::key_select), 0x0004);
        gba::test.eq(__builtin_bit_cast(int, gba::key_start), 0x0008);
        gba::test.eq(__builtin_bit_cast(int, gba::key_right), 0x0010);
        gba::test.eq(__builtin_bit_cast(int, gba::key_left), 0x0020);
        gba::test.eq(__builtin_bit_cast(int, gba::key_up), 0x0040);
        gba::test.eq(__builtin_bit_cast(int, gba::key_down), 0x0080);
        gba::test.eq(__builtin_bit_cast(int, gba::key_r), 0x0100);
        gba::test.eq(__builtin_bit_cast(int, gba::key_l), 0x0200);
    }

    // reset_combo
    {
        auto expected = gba::key_a | gba::key_b | gba::key_select | gba::key_start;
        gba::test.eq(__builtin_bit_cast(int, gba::reset_combo), __builtin_bit_cast(int, expected));
    }

    // keypad: initial state is all released
    {
        gba::keypad kp;
        gba::test.is_false(kp.held(gba::key_a));
        gba::test.is_false(kp.held(gba::key_b));
        gba::test.eq(kp.xaxis(), 0);
        gba::test.eq(kp.yaxis(), 0);
        gba::test.eq(kp.lraxis(), 0);
    }

    // keypad: pressed detection
    {
        gba::keypad kp;
        // Frame 1: nothing pressed
        kp = keys_from(0x0000);
        gba::test.is_false(kp.pressed(gba::key_a));

        // Frame 2: A pressed
        kp = keys_from(0x0001);
        gba::test.is_true(kp.pressed(gba::key_a));
        gba::test.is_false(kp.pressed(gba::key_b));

        // Frame 3: A still held - no longer "pressed" (edge only)
        kp = keys_from(0x0001);
        gba::test.is_false(kp.pressed(gba::key_a));
        gba::test.is_true(kp.held(gba::key_a));
    }

    // keypad: released detection
    {
        gba::keypad kp;
        // Frame 1: A held
        kp = keys_from(0x0001);
        // Frame 2: A released
        kp = keys_from(0x0000);
        gba::test.is_true(kp.released(gba::key_a));
        gba::test.is_false(kp.held(gba::key_a));

        // Frame 3: still released - no edge
        kp = keys_from(0x0000);
        gba::test.is_false(kp.released(gba::key_a));
    }

    // keypad: held detection
    {
        gba::keypad kp;
        kp = keys_from(0x0003); // A + B
        gba::test.is_true(kp.held(gba::key_a));
        gba::test.is_true(kp.held(gba::key_b));
        gba::test.is_true(kp.held(gba::key_a, gba::key_b)); // AND: both held
        gba::test.is_false(kp.held(gba::key_start));
    }

    // keypad: held with logical_or
    {
        gba::keypad kp;
        kp = keys_from(0x0001); // Only A
        gba::test.is_true((kp.held<std::logical_or>(gba::key_a, gba::key_b)));
        gba::test.is_false((kp.held<std::logical_and>(gba::key_a, gba::key_b)));
    }

    // keypad: xaxis (right = 0x10, left = 0x20)
    {
        gba::keypad kp;
        kp = keys_from(0x0010); // right
        gba::test.eq(kp.xaxis(), 1);

        kp = keys_from(0x0020); // left
        gba::test.eq(kp.xaxis(), -1);

        kp = keys_from(0x0030); // both = cancel
        gba::test.eq(kp.xaxis(), 0);

        kp = keys_from(0x0000); // neither
        gba::test.eq(kp.xaxis(), 0);
    }

    // keypad: yaxis (up = 0x40, down = 0x80)
    // yaxis: low bit (up) pressed = +1, high bit (down) pressed = -1
    {
        gba::keypad kp;
        kp = keys_from(0x0040); // up
        gba::test.eq(kp.yaxis(), 1);

        kp = keys_from(0x0080); // down
        gba::test.eq(kp.yaxis(), -1);

        kp = keys_from(0x00C0); // both = cancel
        gba::test.eq(kp.yaxis(), 0);

        kp = keys_from(0x0000); // neither
        gba::test.eq(kp.yaxis(), 0);
    }

    // keypad: lraxis (R = 0x100, L = 0x200)
    // lraxis: low bit (R) pressed = +1, high bit (L) pressed = -1
    {
        gba::keypad kp;
        kp = keys_from(0x0100); // R
        gba::test.eq(kp.lraxis(), 1);

        kp = keys_from(0x0200); // L
        gba::test.eq(kp.lraxis(), -1);

        kp = keys_from(0x0300); // both = cancel
        gba::test.eq(kp.lraxis(), 0);
    }

    // keypad: inverted axes
    {
        gba::keypad kp;
        kp = keys_from(0x0010); // right
        gba::test.eq(kp.i_xaxis(), -1);

        kp = keys_from(0x0020); // left
        gba::test.eq(kp.i_xaxis(), 1);

        kp = keys_from(0x0040); // up
        gba::test.eq(kp.i_yaxis(), -1);

        kp = keys_from(0x0080); // down
        gba::test.eq(kp.i_yaxis(), 1);

        kp = keys_from(0x0100); // R
        gba::test.eq(kp.i_lraxis(), -1);

        kp = keys_from(0x0200); // L
        gba::test.eq(kp.i_lraxis(), 1);
    }

    // keypad: multiple frames
    {
        gba::keypad kp;
        kp = keys_from(0x0000); // nothing
        kp = keys_from(0x0009); // A + Start
        gba::test.is_true(kp.pressed(gba::key_a));
        gba::test.is_true(kp.pressed(gba::key_start));
        gba::test.is_true(kp.held(gba::key_a, gba::key_start));

        kp = keys_from(0x0001);                     // Release Start, keep A
        gba::test.is_false(kp.pressed(gba::key_a)); // No edge
        gba::test.is_true(kp.released(gba::key_start));
        gba::test.is_true(kp.held(gba::key_a));
        gba::test.is_false(kp.held(gba::key_start));
    }
    return gba::test.finish();
}
