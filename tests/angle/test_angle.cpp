#include <gba/angle>

#include <numbers>

#include <mgba_test.hpp>

int main() {
    using namespace gba;
    using namespace gba::literals;

    // angle (32-bit intermediate) construction tests

    {
        // Default construction
        angle a;
        ASSERT_EQ(bit_cast(a), 0u);
    }

    {
        // Construction from raw value
        angle a{0x40000000u}; // 90 degrees
        ASSERT_EQ(bit_cast(a), 0x40000000u);
    }

    {
        // Compile-time construction from radians
        static constexpr angle angle_90 = std::numbers::pi / 2.0;
        ASSERT_EQ(bit_cast(angle_90), 0x40000000u);

        static constexpr angle angle_180 = std::numbers::pi;
        ASSERT_EQ(bit_cast(angle_180), 0x80000000u);

        static constexpr angle angle_270 = 3.0 * std::numbers::pi / 2.0;
        ASSERT_EQ(bit_cast(angle_270), 0xC0000000u);
    }

    {
        // Construction from literals
        static constexpr angle a = 90_deg;
        ASSERT_EQ(bit_cast(a), 0x40000000u);

        static constexpr angle b = 180_deg;
        ASSERT_EQ(bit_cast(b), 0x80000000u);

        // Radians literal - use exact value for comparison
        static constexpr angle c = 1.5707963267948966_rad; // pi/2
        ASSERT_EQ(bit_cast(c), 0x40000000u);
    }

    // packed_angle (storage) tests

    {
        // packed_angle<16> storage
        packed_angle16 a16 = 90_deg;
        ASSERT_EQ(bit_cast(a16), 0x4000u);

        // packed_angle<8> storage
        packed_angle8 a8 = 90_deg;
        ASSERT_EQ(bit_cast(a8), 0x40u);
    }

    {
        // Conversion packed_angle -> angle (implicit)
        packed_angle16 stored = 90_deg;
        angle full = stored;
        ASSERT_EQ(bit_cast(full), 0x40000000u);
    }

    {
        // Conversion angle -> packed_angle (implicit assignment)
        angle full{0x40000000u};
        packed_angle16 stored = full;
        ASSERT_EQ(bit_cast(stored), 0x4000u);
    }

    // Arithmetic tests (on angle intermediate type)

    {
        // Addition with wraparound
        angle a{0xFFFFFFFFu};
        a += angle{2u};
        ASSERT_EQ(bit_cast(a), 1u); // Wraps around
    }

    {
        // Subtraction with wraparound
        angle a{1u};
        a -= angle{2u};
        ASSERT_EQ(bit_cast(a), 0xFFFFFFFFu); // Wraps around
    }

    {
        // Addition operator
        angle a = 90_deg;
        angle b = 90_deg;
        angle c = a + b;
        ASSERT_EQ(bit_cast(c), 0x80000000u); // 180 degrees
    }

    {
        // Subtraction operator
        angle a = 180_deg;
        angle b = 90_deg;
        angle c = a - b;
        ASSERT_EQ(bit_cast(c), 0x40000000u); // 90 degrees
    }

    {
        // Negation
        angle a = 90_deg;
        angle b = -a;
        ASSERT_EQ(bit_cast(b), 0xC0000000u); // -90 = 270 degrees
    }

    {
        // Scalar multiplication
        angle a = 45_deg;
        angle b = a * 2;
        ASSERT_EQ(bit_cast(b), 0x40000000u); // 90 degrees
    }

    {
        // Scalar division
        angle a = 90_deg;
        angle b = a / 2;
        ASSERT_EQ(bit_cast(b), 0x20000000u); // 45 degrees
    }

    // Bit shift tests

    {
        // Left shift
        angle a{0x00000001u};
        a <<= 8;
        ASSERT_EQ(bit_cast(a), 0x00000100u);
    }

    {
        // Right shift
        angle a{0x00000100u};
        a >>= 8;
        ASSERT_EQ(bit_cast(a), 0x00000001u);
    }

    {
        // Shift operators
        angle a{0x12345678u};
        angle b = a << 4;
        angle c = a >> 4;
        ASSERT_EQ(bit_cast(b), 0x23456780u);
        ASSERT_EQ(bit_cast(c), 0x01234567u);
    }

    // Comparison tests

    {
        angle a = 90_deg;
        angle b = 90_deg;
        angle c = 180_deg;

        ASSERT_TRUE(a == b);
        ASSERT_FALSE(a == c);
        ASSERT_TRUE(a != c);
    }

    {
        // Comparison with raw value
        angle a = 90_deg;
        ASSERT_EQ(bit_cast(a), 0x40000000u);
    }

    // Helper function tests

    {
        // lut_index
        angle a = 45_deg;
        // 45 degrees = 0.125 turns = 0x20000000
        // For 11-bit table: 0x20000000 >> 21 = 256
        unsigned int idx = lut_index<11>(a);
        ASSERT_EQ(idx, 256u);

        // 180 degrees
        angle b = 180_deg;
        idx = lut_index<11>(b);
        ASSERT_EQ(idx, 1024u); // Half of 2048
    }

    {
        // as_signed
        angle a{0x7FFFFFFFu}; // Just below half-rotation (positive when signed)
        int s = as_signed(a);
        ASSERT_EQ(s, 0x7FFFFFFF);

        angle b{0x80000000u}; // Exactly half-rotation (negative when signed)
        s = as_signed(b);
        ASSERT_EQ(s, static_cast<int>(0x80000000u));
    }

    {
        // ccw_distance / cw_distance
        angle start = 0_deg;
        angle end = 90_deg;

        unsigned int ccw = ccw_distance(start, end);
        unsigned int cw = cw_distance(start, end);

        ASSERT_EQ(ccw, 0x40000000u); // 90 degrees CCW
        ASSERT_EQ(cw, 0xC0000000u);  // 270 degrees CW
    }

    {
        // is_ccw_between
        angle start = 0_deg;
        angle end = 180_deg;
        angle test_in = 90_deg;
        angle test_out = 270_deg;

        ASSERT_TRUE(is_ccw_between(start, end, test_in));
        ASSERT_FALSE(is_ccw_between(start, end, test_out));
    }

    // Type alias tests

    {
        // Ensure type aliases are correct
        static_assert(std::is_same_v<packed_angle32, packed_angle<32>>);
        static_assert(std::is_same_v<packed_angle16, packed_angle<16>>);
        static_assert(std::is_same_v<packed_angle8, packed_angle<8>>);
    }

    {
        // Test bits
        static_assert(angle::bits == 32);
        static_assert(packed_angle16::bits == 16);
        static_assert(packed_angle8::bits == 8);
    }

    {
        // Storage types
        static_assert(sizeof(packed_angle16) == 2);
        static_assert(sizeof(packed_angle8) == 1);
        static_assert(sizeof(angle) == 4);
    }

    return 0;
}
