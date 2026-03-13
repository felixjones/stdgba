#include <gba/benchmark>
#include <gba/bios>
#include <gba/fixed_point>
#include <gba/testing>

int main() {
    {
        const auto [num, denom] = gba::Div(9, 2);
        gba::test.eq(num, 4);
        gba::test.eq(denom, 1);
    }

    {
        const auto [num, denom] = gba::DivArm(3, 17);
        gba::test.eq(num, 5);
        gba::test.eq(denom, 2);
    }

    gba::test.eq(gba::Sqrt(16), 4);

    {
        static constexpr auto expected = gba::fixed<unsigned short>{1.77};
        gba::test.eq(gba::Sqrt(gba::fixed<unsigned short>{3.14}), expected);
    }

    // ArcTan tests
    gba::test.eq(gba::bit_cast(gba::ArcTan(0)), 0);
    gba::test.eq(gba::bit_cast(gba::ArcTan(1)), 0x2000);
    gba::test.eq(gba::bit_cast(gba::ArcTan(-1)), 0xE000);
    gba::test.eq(gba::bit_cast(gba::ArcTan(32767)), 0xE000);
    gba::test.eq(gba::bit_cast(gba::ArcTan(-32768)), 0xE000);

    // ArcTan2 tests
    gba::test.eq(gba::bit_cast(gba::ArcTan2(1, 0)), 0);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(0, 1)), 0x4000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(-1, 0)), 0x8000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(0, -1)), 0xC000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(1, 1)), 0x2000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(-1, -1)), 0xA000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(-1, 1)), 0x6000);
    gba::test.eq(gba::bit_cast(gba::ArcTan2(1, -1)), 0xE000);

    // BgAffineSet test
    {
        using namespace gba::literals;
        static constexpr gba::background_parameters bgAffineSet = {
            .tex_x = 1, .tex_y = 1, .scr_x = 120, .scr_y = 80, .sx = 1, .sy = 1, .alpha = 0_deg};

        static constexpr auto expected = [] static consteval {
            gba::background_matrix e{};
            gba::BgAffineSet(&bgAffineSet, &e, 1);
            return e;
        }();

        gba::background_matrix result;
        gba::benchmark::do_not_optimize([&] { gba::BgAffineSet(&bgAffineSet, &result, 1); });

        gba::test.eq(result.p, expected.p);
        gba::test.eq(result.x, expected.x);
        gba::test.eq(result.y, expected.y);
    }

    // ObjAffineSet test
    {
        using namespace gba::literals;
        static constexpr gba::object_parameters objAffineSet = {.sx = 1, .sy = 1, .alpha = 0_deg};

        static constexpr auto expected = [] static consteval {
            std::array<gba::fixed<short>, 4> e{};
            gba::ObjAffineSet(&objAffineSet, e.data(), 1);
            return e;
        }();

        std::array<gba::fixed<short>, 4> result;
        gba::benchmark::do_not_optimize([&] { gba::ObjAffineSet(&objAffineSet, result.data(), 1); });

        gba::test.eq(result, expected);
    }
    return gba::test.finish();
}
