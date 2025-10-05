#include <gba/bios>
#include <gba/fixed_point>

#include <mgba_test.hpp>

int main() {
    {
        const auto [num, denom] = gba::Div(9, 2);
        ASSERT_EQ(num, 4);
        ASSERT_EQ(denom, 1);
    }

    {
        const auto [num, denom] = gba::DivArm(3, 17);
        ASSERT_EQ(num, 5);
        ASSERT_EQ(denom, 2);
    }

    ASSERT_EQ(gba::Sqrt(16), 4);

    {
        static constexpr auto expected = gba::fixed<unsigned short>{1.77};
        ASSERT_EQ(gba::Sqrt(gba::fixed<unsigned short>{3.14}), expected);
    }

    // ArcTan tests
    ASSERT_EQ(gba::ArcTan(0), 0);
    ASSERT_EQ(gba::ArcTan(1), 0x2000);
    ASSERT_EQ(gba::ArcTan(-1), -0x2000);
    ASSERT_EQ(gba::ArcTan(32767), -0x2000);
    ASSERT_EQ(gba::ArcTan(-32768), -0x2000);

    // ArcTan2 tests
    ASSERT_EQ(gba::ArcTan2(1, 0), 0);
    ASSERT_EQ(gba::ArcTan2(0, 1), 0x4000);
    ASSERT_EQ(gba::ArcTan2(-1, 0), -0x8000);
    ASSERT_EQ(gba::ArcTan2(0, -1), -0x4000);
    ASSERT_EQ(gba::ArcTan2(1, 1), 0x2000);
    ASSERT_EQ(gba::ArcTan2(-1, -1), -0x6000);
    ASSERT_EQ(gba::ArcTan2(-1, 1), 0x6000);
    ASSERT_EQ(gba::ArcTan2(1, -1), -0x2000);
}
