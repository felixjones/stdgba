#include <mgba_test.hpp>

#include <gba/angle>

#include <numbers>

int main() {
    using namespace gba;

    // ========================================================================
    // Construction tests
    // ========================================================================

    {
        // Default construction
        angle32 a;
        ASSERT_EQ(static_cast<uint32_t>(a), 0u);
    }

    {
        // Construction from raw value
        angle32 a = 0x40000000u;  // 90 degrees
        ASSERT_EQ(static_cast<uint32_t>(a), 0x40000000u);
    }

    {
        // Compile-time construction from radians
        static constexpr auto angle_90 = angle32(std::numbers::pi / 2.0);
        ASSERT_EQ(static_cast<uint32_t>(angle_90), 0x40000000u);

        static constexpr auto angle_180 = angle32(std::numbers::pi);
        ASSERT_EQ(static_cast<uint32_t>(angle_180), 0x80000000u);

        static constexpr auto angle_270 = angle32(3.0 * std::numbers::pi / 2.0);
        ASSERT_EQ(static_cast<uint32_t>(angle_270), 0xC0000000u);
    }

    {
        // Different precisions
        angle16 a16 = 0x4000u;  // 90 degrees in 16-bit
        ASSERT_EQ(static_cast<uint16_t>(a16), 0x4000u);
    }

    // ========================================================================
    // Implicit conversion tests
    // ========================================================================

    {
        // angle -> uint (implicit)
        angle32 a = 0x12345678u;
        uint32_t raw = a;  // Should compile (implicit conversion)
        ASSERT_EQ(raw, 0x12345678u);
    }

    {
        // uint -> angle (implicit)
        uint32_t raw = 0x87654321u;
        angle32 a = raw;  // Should compile (implicit conversion)
        ASSERT_EQ(static_cast<uint32_t>(a), 0x87654321u);
    }

    // ========================================================================
    // Arithmetic tests
    // ========================================================================

    {
        // Addition with wraparound
        angle32 a = 0xFFFFFFFFu;
        a += 2u;
        ASSERT_EQ(static_cast<uint32_t>(a), 1u);  // Wraps around
    }

    {
        // Subtraction with wraparound
        angle32 a = 1u;
        a -= 2u;
        ASSERT_EQ(static_cast<uint32_t>(a), 0xFFFFFFFFu);  // Wraps around
    }

    {
        // Addition operator
        angle32 a = 0x40000000u;  // 90 degrees
        angle32 b = 0x40000000u;  // 90 degrees
        angle32 c = a + b;
        ASSERT_EQ(static_cast<uint32_t>(c), 0x80000000u);  // 180 degrees
    }

    {
        // Subtraction operator
        angle32 a = 0x80000000u;  // 180 degrees
        angle32 b = 0x40000000u;  // 90 degrees
        angle32 c = a - b;
        ASSERT_EQ(static_cast<uint32_t>(c), 0x40000000u);  // 90 degrees
    }

    {
        // Negation
        angle32 a = 0x40000000u;  // 90 degrees
        angle32 b = -a;
        ASSERT_EQ(static_cast<uint32_t>(b), 0xC0000000u);  // -90 = 270 degrees
    }

    {
        // Mixed angle/int arithmetic (use explicit rep to avoid ambiguity)
        angle32 a = 0x40000000u;
        a += static_cast<angle32::rep>(0x40000000u);
        ASSERT_EQ(static_cast<uint32_t>(a), 0x80000000u);

        angle32 b = gba::bit_cast<angle32>(gba::as_raw(a) + static_cast<angle32::rep>(0x40000000u));
        ASSERT_EQ(static_cast<uint32_t>(b), 0xC0000000u);
    }

    // ========================================================================
    // Bit shift tests
    // ========================================================================

    {
        // Left shift
        angle32 a = 0x00000001u;
        a <<= 8;
        ASSERT_EQ(static_cast<uint32_t>(a), 0x00000100u);
    }

    {
        // Right shift
        angle32 a = 0x00000100u;
        a >>= 8;
        ASSERT_EQ(static_cast<uint32_t>(a), 0x00000001u);
    }

    {
        // Shift operators (non-assignment) via helpers to avoid ambiguity
        angle32 a = 0x12345678u;
        angle32 b = shift_left(a, 4);
        angle32 c = shift_right(a, 4);
        ASSERT_EQ(static_cast<uint32_t>(b), 0x23456780u);
        ASSERT_EQ(static_cast<uint32_t>(c), 0x01234567u);
    }

    // ========================================================================
    // Comparison tests
    // ========================================================================

    {
        angle32 a = 100u;
        angle32 b = 100u;
        angle32 c = 200u;

        ASSERT_TRUE(a == b);
        ASSERT_FALSE(a == c);
        ASSERT_TRUE(a != c);
    }

    {
        // Comparison with raw int (equality via raw helper)
        angle32 a = 100u;
        ASSERT_TRUE(gba::as_raw(a) == 100u);
        ASSERT_FALSE(gba::as_raw(a) == 200u);
    }

    // ========================================================================
    // Helper function tests
    // ========================================================================

    {
        // bit_cast (construct from raw)
        uint32_t raw = 0x12345678u;
        angle32 a = bit_cast<angle32>(raw);
        ASSERT_EQ(static_cast<uint32_t>(a), 0x12345678u);
    }

    {
        // as_raw (extract raw)
        angle32 a = 0x87654321u;
        uint32_t raw = as_raw(a);
        ASSERT_EQ(raw, 0x87654321u);
    }

    {
        // angle_cast (precision conversion)
        angle16 a16 = 0x4000u;  // 90 degrees in 16-bit (0x4000 / 0x10000 = 0.25 turns)
        angle32 a32 = angle_cast<angle32>(a16);
        ASSERT_EQ(static_cast<uint32_t>(a32), 0x40000000u);  // 90 degrees in 32-bit

        // Downscale
        angle32 b32 = 0x40000000u;
        angle16 b16 = angle_cast<angle16>(b32);
        ASSERT_EQ(static_cast<uint16_t>(b16), 0x4000u);
    }

    {
        // lut_index
        angle32 a = 0x08000000u;  // 45 degrees
        // For 11-bit table (2048 entries): shift by (32-11)=21 bits
        // 0x08000000 >> 21 = 0x040 = 64 (which is 45°/360° * 2048 = 256/4 = 64... wait let me recalculate)
        // 0x08000000 = 134217728
        // 134217728 / 2^32 = 0.03125 turns = 11.25 degrees
        // For an 11-bit LUT: 0.03125 * 2048 = 64
        unsigned int idx = lut_index<11>(a);
        ASSERT_EQ(idx, 64u);

        // Another test: 180 degrees
        angle32 b = 0x80000000u;
        idx = lut_index<11>(b);
        ASSERT_EQ(idx, 1024u);  // Half of 2048
    }

    {
        // as_signed
        angle32 a = 0x7FFFFFFFu;  // Just below half-rotation (positive when signed)
        int32_t s = as_signed(a);
        ASSERT_EQ(s, 0x7FFFFFFF);

        angle32 b = 0x80000000u;  // Exactly half-rotation (negative when signed)
        s = as_signed(b);
        ASSERT_EQ(s, static_cast<int32_t>(0x80000000u));
    }

    {
        // in_signed_range
        angle32 center = 0u;  // 0 degrees
        angle32 pos_limit = 0x10000000u;  // ~22.5 degrees
        angle32 neg_limit = 0xF0000000u;  // ~-22.5 degrees (as signed)

        angle32 in_range = 0x08000000u;  // ~11.25 degrees
        angle32 out_range = 0x40000000u;  // 90 degrees

        ASSERT_TRUE(in_signed_range(in_range, pos_limit, neg_limit));
        ASSERT_FALSE(in_signed_range(out_range, pos_limit, neg_limit));
    }

    {
        // from_degrees
        auto a = from_degrees<angle32>(90.0);
        ASSERT_EQ(static_cast<uint32_t>(a), 0x40000000u);

        auto b = from_degrees<angle32>(180.0);
        ASSERT_EQ(static_cast<uint32_t>(b), 0x80000000u);

        auto c = from_degrees<angle32>(270.0);
        ASSERT_EQ(static_cast<uint32_t>(c), 0xC0000000u);
    }

    {
        // from_radians
        auto a = from_radians<angle32>(std::numbers::pi / 2.0);
        ASSERT_EQ(static_cast<uint32_t>(a), 0x40000000u);

        auto b = from_radians<angle32>(std::numbers::pi);
        ASSERT_EQ(static_cast<uint32_t>(b), 0x80000000u);
    }

    // ========================================================================
    // Type aliases tests
    // ========================================================================

    {
        // Ensure angle32 and angle16 are the expected types
        static_assert(std::is_same_v<angle32, angle<uint32_t>>);
        static_assert(std::is_same_v<angle16, angle<uint16_t>>);
    }

    {
        // Test that angle32::bits is correct
        static_assert(angle32::bits == 32);
        static_assert(angle16::bits == 16);
    }

    return 0;
}
