/// @file tests/ecs/test_ecs.cpp
/// @brief Comprehensive tests for gba::ecs::registry.

#include <gba/ecs>
#include <gba/testing>

#include <cstdint>

// Section: Test component types (all power-of-two sizes)

struct pos_t {
    int x, y;
}; // 8 bytes
struct vel_t {
    int vx, vy;
}; // 8 bytes
struct hp_t {
    int hp;
}; // 4 bytes
struct tag_t {
    std::uint8_t id;
}; // 1 byte

// Verify sizes are powers of two at compile time
static_assert(std::has_single_bit(sizeof(pos_t)));
static_assert(std::has_single_bit(sizeof(vel_t)));
static_assert(std::has_single_bit(sizeof(hp_t)));
static_assert(std::has_single_bit(sizeof(tag_t)));

using test_registry = gba::ecs::registry<32, pos_t, vel_t, hp_t, tag_t>;

int main() {
    // Section: Entity lifecycle

    gba::test("create entity", [] {
        test_registry reg;
        auto e = reg.create();
        gba::test.expect.is_true(reg.valid(e), "created entity is valid");
        gba::test.expect.eq(reg.size(), std::size_t{1}, "size is 1");
    });

    gba::test("create multiple entities", [] {
        test_registry reg;
        auto e0 = reg.create();
        auto e1 = reg.create();
        auto e2 = reg.create();
        gba::test.expect.neq(e0, e1, "distinct ids");
        gba::test.expect.neq(e1, e2, "distinct ids");
        gba::test.expect.eq(reg.size(), std::size_t{3}, "size is 3");
    });

    gba::test("destroy entity", [] {
        test_registry reg;
        auto e = reg.create();
        reg.destroy(e);
        gba::test.expect.is_false(reg.valid(e), "destroyed entity is invalid");
        gba::test.expect.eq(reg.size(), std::size_t{0}, "size is 0");
    });

    gba::test("null entity is never valid", [] {
        test_registry reg;
        gba::test.expect.is_false(reg.valid(gba::entity_null), "null is invalid");
    });

    gba::test("generation invalidation", [] {
        test_registry reg;
        auto e1 = reg.create();
        reg.destroy(e1);
        auto e2 = reg.create(); // reuses slot, new generation
        gba::test.expect.is_false(reg.valid(e1), "old gen is invalid");
        gba::test.expect.is_true(reg.valid(e2), "new gen is valid");
        gba::test.expect.eq(e1.slot, e2.slot, "same slot");
        gba::test.expect.neq(e1.generation, e2.generation, "different gen");
    });

    gba::test("slot reuse after destroy", [] {
        test_registry reg;
        auto e1 = reg.create();
        auto slot1 = e1.slot;
        reg.emplace<hp_t>(e1, 100);
        reg.destroy(e1);
        auto e2 = reg.create();
        gba::test.expect.eq(e2.slot, slot1, "slot reused");
        // Component mask must be clean (no residual hp_t)
        gba::test.expect.is_false(reg.all_of<hp_t>(e2), "no residual component");
    });

    gba::test("clear all entities", [] {
        test_registry reg;
        auto e0 = reg.create();
        auto e1 = reg.create();
        reg.emplace<pos_t>(e0, 1, 2);
        reg.emplace<vel_t>(e1, 3, 4);
        reg.clear();
        gba::test.expect.eq(reg.size(), std::size_t{0}, "size is 0");
        gba::test.expect.is_false(reg.valid(e0), "e0 invalid after clear");
        gba::test.expect.is_false(reg.valid(e1), "e1 invalid after clear");
    });

    // Section: Component operations

    gba::test("emplace and get", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e, 10, 20);
        gba::test.expect.eq(reg.get<pos_t>(e).x, 10, "pos.x");
        gba::test.expect.eq(reg.get<pos_t>(e).y, 20, "pos.y");
    });

    gba::test("emplace all four components", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e, 1, 2);
        reg.emplace<vel_t>(e, 3, 4);
        reg.emplace<hp_t>(e, 100);
        reg.emplace<tag_t>(e, std::uint8_t{7});
        gba::test.expect.eq(reg.get<pos_t>(e).x, 1);
        gba::test.expect.eq(reg.get<vel_t>(e).vx, 3);
        gba::test.expect.eq(reg.get<hp_t>(e).hp, 100);
        gba::test.expect.eq(reg.get<tag_t>(e).id, std::uint8_t{7});
    });

    gba::test("emplace default construct", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e);
        gba::test.expect.eq(reg.get<pos_t>(e).x, 0, "default x");
        gba::test.expect.eq(reg.get<pos_t>(e).y, 0, "default y");
    });

    gba::test("remove component", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<hp_t>(e, 50);
        gba::test.expect.is_true(reg.all_of<hp_t>(e), "has hp before remove");
        reg.remove<hp_t>(e);
        gba::test.expect.is_false(reg.all_of<hp_t>(e), "no hp after remove");
    });

    gba::test("remove_unchecked by component reference", [] {
        test_registry reg;
        auto e = reg.create();
        auto& hp = reg.emplace<hp_t>(e, 88);
        gba::test.expect.is_true(reg.all_of<hp_t>(e), "has hp before ref remove_unchecked");
        reg.remove_unchecked<hp_t>(hp);
        gba::test.expect.is_false(reg.all_of<hp_t>(e), "no hp after ref remove_unchecked");
    });

    gba::test("remove_unchecked variadic by entity_id", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e, 1, 2);
        reg.emplace<vel_t>(e, 3, 4);
        reg.emplace<hp_t>(e, 55);

        gba::test.expect.is_true(reg.all_of<pos_t, vel_t, hp_t>(e), "all components present before variadic remove");
        reg.remove_unchecked<pos_t, hp_t>(e);
        gba::test.expect.is_false(reg.all_of<pos_t>(e), "pos removed by variadic remove_unchecked");
        gba::test.expect.is_true(reg.all_of<vel_t>(e), "vel kept after variadic remove_unchecked");
        gba::test.expect.is_false(reg.all_of<hp_t>(e), "hp removed by variadic remove_unchecked");
    });

    gba::test("try_get optional access", [] {
        test_registry reg;
        auto e = reg.create();

        gba::test.expect.is_true(reg.try_get<pos_t>(e) == nullptr, "missing component returns nullptr");

        reg.emplace<pos_t>(e, 12, 34);
        auto* p = reg.try_get<pos_t>(e);
        gba::test.expect.is_true(p != nullptr, "present component returns pointer");
        gba::test.expect.eq(p->x, 12, "pointer exposes value");

        const auto& creg = reg;
        auto* cp = creg.try_get<pos_t>(e);
        gba::test.expect.is_true(cp != nullptr, "const try_get returns pointer");
        gba::test.expect.eq(cp->y, 34, "const pointer exposes value");

        reg.destroy(e);
        gba::test.expect.is_true(reg.try_get<pos_t>(e) == nullptr, "invalid entity returns nullptr");
    });

    gba::test("with optional callback access", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e, 5, 6);

        bool called = false;
        const bool ok = reg.with<pos_t>(e, [&](pos_t& p) {
            called = true;
            p.x += 10;
        });
        gba::test.expect.is_true(ok, "with returns true when component exists");
        gba::test.expect.is_true(called, "with invokes callback");
        gba::test.expect.eq(reg.get<pos_t>(e).x, 15, "with can mutate component");

        called = false;
        const bool missing = reg.with<hp_t>(e, [&](hp_t&) { called = true; });
        gba::test.expect.is_false(missing, "with returns false when component missing");
        gba::test.expect.is_false(called, "with does not invoke callback when missing");

        const auto& creg = reg;
        bool const_called = false;
        const bool const_ok = creg.with<pos_t>(e, [&](const pos_t& p) {
            const_called = true;
            gba::test.expect.eq(p.y, 6, "const with sees component value");
        });
        gba::test.expect.is_true(const_ok, "const with returns true when component exists");
        gba::test.expect.is_true(const_called, "const with invokes callback");
    });

    gba::test("all_of / any_of", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<pos_t>(e, 0, 0);
        reg.emplace<vel_t>(e, 0, 0);
        gba::test.expect.is_true(reg.all_of<pos_t>(e), "has pos");
        gba::test.expect.is_true(reg.all_of<vel_t>(e), "has vel");
        gba::test.expect.is_true((reg.all_of<pos_t, vel_t>(e)), "has both");
        gba::test.expect.is_false((reg.all_of<pos_t, vel_t, hp_t>(e)), "missing hp");
        gba::test.expect.is_true((reg.any_of<pos_t, hp_t>(e)), "any: has pos");
        gba::test.expect.is_false(reg.any_of<hp_t>(e), "any: no hp");
    });

    // Section: View iteration

    gba::test("view 2-component each", [] {
        test_registry reg;
        for (int i = 0; i < 8; ++i) {
            auto e = reg.create();
            reg.emplace<pos_t>(e, i, 0);
            reg.emplace<vel_t>(e, 1, 1);
        }
        int count = 0;
        reg.view<pos_t, vel_t>().each([&count](pos_t& p, const vel_t& v) {
            p.x += v.vx;
            p.y += v.vy;
            ++count;
        });
        gba::test.expect.eq(count, 8, "iterated 8 entities");
    });

    gba::test("view 3-component each", [] {
        test_registry reg;
        for (int i = 0; i < 4; ++i) {
            auto e = reg.create();
            reg.emplace<pos_t>(e, i, 0);
            reg.emplace<vel_t>(e, 1, 1);
            reg.emplace<hp_t>(e, 100);
        }
        int count = 0;
        reg.view<pos_t, vel_t, hp_t>().each([&count](pos_t& p, const vel_t& v, hp_t& h) {
            p.x += v.vx;
            if (h.hp > 0) --h.hp;
            ++count;
        });
        gba::test.expect.eq(count, 4, "iterated 4 entities");
    });

    gba::test("view skips non-matching entities", [] {
        test_registry reg;
        // 3 entities with pos+vel, 2 with only pos
        for (int i = 0; i < 5; ++i) {
            auto e = reg.create();
            reg.emplace<pos_t>(e, i, 0);
            if (i < 3) reg.emplace<vel_t>(e, 1, 1);
        }
        int count = 0;
        reg.view<pos_t, vel_t>().each([&count](pos_t&, vel_t&) { ++count; });
        gba::test.expect.eq(count, 3, "only 3 match");
    });

    gba::test("view skips destroyed entities", [] {
        test_registry reg;
        auto e0 = reg.create();
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<pos_t>(e0, 0, 0);
        reg.emplace<pos_t>(e1, 1, 1);
        reg.emplace<pos_t>(e2, 2, 2);
        reg.destroy(e1);
        int count = 0;
        reg.view<pos_t>().each([&count](pos_t&) { ++count; });
        gba::test.expect.eq(count, 2, "skips destroyed e1");
    });

    gba::test("view with entity_id callback", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<hp_t>(e, 42);
        int count = 0;
        reg.view<hp_t>().each([&](hp_t& h) {
            if (h.hp == 42) ++count;
        });
        gba::test.expect.eq(count, 1, "each iteration found expected hp");
    });

    gba::test("view range-based for", [] {
        test_registry reg;
        for (int i = 0; i < 4; ++i) {
            auto e = reg.create();
            reg.emplace<pos_t>(e, i * 10, 0);
            reg.emplace<vel_t>(e, 1, 2);
        }
        int sum_x = 0;
        for (auto [pos, vel] : reg.view<pos_t, vel_t>()) {
            pos.x += vel.vx;
            sum_x += pos.x;
        }
        // Original x: 0, 10, 20, 30. After +1 each: 1, 11, 21, 31. Sum = 64.
        gba::test.expect.eq(sum_x, 64, "range-for modifies components");
    });

    gba::test("view empty registry", [] {
        test_registry reg;
        int count = 0;
        reg.view<pos_t>().each([&count](pos_t&) { ++count; });
        gba::test.expect.eq(count, 0, "empty registry");
    });

    gba::test("view single component", [] {
        test_registry reg;
        auto e = reg.create();
        reg.emplace<tag_t>(e, std::uint8_t{99});
        int count = 0;
        reg.view<tag_t>().each([&count](const tag_t& t) {
            if (t.id == 99) ++count;
        });
        gba::test.expect.eq(count, 1, "single-component view");
    });

    // Section: Constexpr support

    gba::test("constexpr create + emplace + get", [] {
        static constexpr auto result = [] {
            gba::ecs::registry<8, int, short> reg;
            auto e = reg.create();
            reg.emplace<int>(e, 42);
            reg.emplace<short>(e, short{7});
            return reg.get<int>(e) * 100 + reg.get<short>(e);
        }();
        static_assert(result == 4207);
        gba::test.expect.eq(result, 4207, "constexpr registry");
    });

    gba::test("constexpr view count", [] {
        static constexpr auto count = [] {
            gba::ecs::registry<16, int, short> reg;
            for (int i = 0; i < 10; ++i) {
                auto e = reg.create();
                reg.emplace<int>(e, i);
                if (i % 2 == 0) reg.emplace<short>(e, static_cast<short>(i));
            }
            int n = 0;
            reg.view<int, short>().each([&n](int&, short&) { ++n; });
            return n;
        }();
        static_assert(count == 5);
        gba::test.expect.eq(count, 5, "constexpr view count");
    });

    gba::test("constexpr destroy + generation", [] {
        static constexpr auto gen_changed = [] {
            gba::ecs::registry<4, int> reg;
            auto e1 = reg.create();
            auto gen1 = e1.generation;
            reg.destroy(e1);
            auto e2 = reg.create();
            return e2.generation != gen1;
        }();
        static_assert(gen_changed);
        gba::test.is_true(gen_changed, "constexpr gen increment");
    });

    // Section: Pad utility

    gba::test("pad utility sizes", [] {
        struct padded_1b {
            std::uint8_t v;
        };
        struct padded_4b {
            std::uint8_t v;
            gba::ecs::pad<3> _;
        };
        static_assert(sizeof(padded_1b) == 1);
        static_assert(sizeof(padded_4b) == 4);
        static_assert(std::has_single_bit(sizeof(padded_4b)));
        gba::test.is_true(true);
    });

    // Section: Size tracking edge cases

    gba::test("size after create-destroy-create cycle", [] {
        test_registry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        gba::test.expect.eq(reg.size(), std::size_t{2});
        reg.destroy(e1);
        gba::test.expect.eq(reg.size(), std::size_t{1});
        auto e3 = reg.create();
        gba::test.expect.eq(reg.size(), std::size_t{2});
        gba::test.expect.is_true(reg.valid(e2));
        gba::test.expect.is_true(reg.valid(e3));
    });

    gba::test("clear then repopulate", [] {
        test_registry reg;
        for (int i = 0; i < 10; ++i) {
            auto e = reg.create();
            reg.emplace<pos_t>(e, i, i);
        }
        reg.clear();
        gba::test.expect.eq(reg.size(), std::size_t{0});

        // Repopulate
        auto e = reg.create();
        reg.emplace<pos_t>(e, 99, 99);
        gba::test.expect.eq(reg.size(), std::size_t{1});
        gba::test.expect.eq(reg.get<pos_t>(e).x, 99);
    });

    // Section: Full capacity

    gba::test("fill to capacity", [] {
        gba::ecs::registry<8, int> reg;
        std::array<gba::entity, 8> entities;
        for (int i = 0; i < 8; ++i) {
            entities[i] = reg.create();
            reg.emplace<int>(entities[i], i * 10);
        }
        gba::test.expect.eq(reg.size(), std::size_t{8});

        int sum = 0;
        reg.view<int>().each([&sum](int& v) { sum += v; });
        gba::test.expect.eq(sum, 280);
        gba::test.expect.eq(sum, 280);
    });

    return gba::test.finish();
}
