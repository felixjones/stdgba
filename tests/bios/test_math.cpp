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
    ASSERT_EQ(gba::as_raw(gba::ArcTan(0)), 0);
    ASSERT_EQ(gba::as_raw(gba::ArcTan(1)), 0x2000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan(-1)), 0xE000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan(32767)), 0xE000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan(-32768)), 0xE000);

    // ArcTan2 tests
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(1, 0)), 0);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(0, 1)), 0x4000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(-1, 0)), 0x8000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(0, -1)), 0xC000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(1, 1)), 0x2000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(-1, -1)), 0xA000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(-1, 1)), 0x6000);
    ASSERT_EQ(gba::as_raw(gba::ArcTan2(1, -1)), 0xE000);

    // BgAffineSet test
    {
        static constexpr gba::background_parameters bgAffineSet = {
            .tex_x = 1,
            .tex_y = 1,
            .scr_x = 120,
            .scr_y = 80,
            .sx = 1,
            .sy = 1,
            .alpha = 0
        };

        static constexpr auto expected = [] static consteval {
            gba::background_matrix e{};
            gba::BgAffineSet(&bgAffineSet, &e, 1);
            return e;
        }();

        gba::background_matrix result;
        test::do_not_optimize([&] {
            gba::BgAffineSet(&bgAffineSet, &result, 1);
        });

        ASSERT_EQ(result.p, expected.p);
        ASSERT_EQ(result.x, expected.x);
        ASSERT_EQ(result.y, expected.y);
    }

    // ObjAffineSet test
    {
        static constexpr gba::object_parameters objAffineSet = {
            .sx = 1,
            .sy = 1,
            .alpha = 0
        };

        static constexpr auto expected = [] static consteval {
            std::array<gba::fixed<short>, 4> e{};
            gba::ObjAffineSet(&objAffineSet, e.data(), 1);
            return e;
        }();

        std::array<gba::fixed<short>, 4> result;
        test::do_not_optimize([&] {
            gba::ObjAffineSet(&objAffineSet, result.data(), 1);
        });

        ASSERT_EQ(result, expected);
    }
}
