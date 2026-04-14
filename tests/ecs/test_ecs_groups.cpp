/// @file tests/ecs/test_ecs_groups.cpp
/// @brief Tests and examples for ECS component groups feature.

#include <gba/ecs>
#include <gba/testing>

#include <cstdint>
#include <type_traits>

// -- Example components for demonstration ------------------------------------

struct position {
    int x, y;
}; // 8 bytes

struct velocity {
    int vx, vy;
}; // 8 bytes

struct acceleration {
    int ax, ay;
}; // 8 bytes

struct health {
    int hp;
}; // 4 bytes

struct armor {
    int defense;
}; // 4 bytes

struct weapon {
    int damage;
}; // 4 bytes

// Verify all sizes are powers of two
static_assert(std::has_single_bit(sizeof(position)));
static_assert(std::has_single_bit(sizeof(velocity)));
static_assert(std::has_single_bit(sizeof(acceleration)));
static_assert(std::has_single_bit(sizeof(health)));
static_assert(std::has_single_bit(sizeof(armor)));
static_assert(std::has_single_bit(sizeof(weapon)));

// -- Component groups demonstration -------------------------------------------

// Group 1: Physics components (movement and forces)
using physics = gba::ecs::group<position, velocity, acceleration>;

// Group 2: Combat components (health and equipment)
using combat = gba::ecs::group<health, armor, weapon>;

// Combined group for complete character
using character = gba::ecs::group<physics, combat>;

// Registry using groups - much cleaner than listing all components
using grouped_registry = gba::ecs::registry<64, character>;

// For comparison - equivalent flat registry
using flat_registry = gba::ecs::registry<64, position, velocity, acceleration, health, armor, weapon>;

// Overlapping groups should deduplicate by first appearance.
using overlap_a = gba::ecs::group<position, velocity, health>;
using overlap_b = gba::ecs::group<velocity, armor, position, weapon>;
using overlap_flat = gba::ecs::flatten_groups_t<overlap_a, overlap_b>;
static_assert(std::is_same_v<overlap_flat, gba::ecs::group<position, velocity, health, armor, weapon>>,
              "group flattening should deduplicate components while preserving first-seen order");

using dedup_registry = gba::ecs::registry<32, overlap_a, overlap_b>;

int main() {
    // -- Test that grouped and flat registries work identically ---------------

    gba::test("group flattening preserves registry semantics", [] {
        grouped_registry g_world;
        flat_registry f_world;

        // Create entities in both registries
        auto g_entity = g_world.create();
        auto f_entity = f_world.create();

        // Emplace same components in both
        g_world.emplace<position>(g_entity, 10, 20);
        f_world.emplace<position>(f_entity, 10, 20);

        g_world.emplace<velocity>(g_entity, 1, 2);
        f_world.emplace<velocity>(f_entity, 1, 2);

        g_world.emplace<health>(g_entity, 100);
        f_world.emplace<health>(f_entity, 100);

        // Both should behave identically
        gba::test.expect.eq(g_world.size(), std::size_t{1}, "grouped registry size is 1");
        gba::test.expect.eq(f_world.size(), std::size_t{1}, "flat registry size is 1");

        // Views should work the same
        int g_count = 0;
        g_world.view<position, velocity>().each([&](position&, const velocity&) { ++g_count; });
        gba::test.expect.eq(g_count, 1, "grouped registry view works");

        int f_count = 0;
        f_world.view<position, velocity>().each([&](position&, const velocity&) { ++f_count; });
        gba::test.expect.eq(f_count, 1, "flat registry view works");
    });

    // -- Test mixed group and component syntax --------------------------------

    using mixed = gba::ecs::group<physics, health, weapon>;
    using mixed_registry = gba::ecs::registry<32, mixed>;

    gba::test("mixed groups with individual components", [] {
        mixed_registry world;
        auto entity = world.create();

        world.emplace<position>(entity, 50, 75);
        world.emplace<velocity>(entity, 2, 3);
        world.emplace<acceleration>(entity, 0, 1);
        world.emplace<health>(entity, 80);
        world.emplace<weapon>(entity, 15);

        gba::test.expect.is_true(world.all_of<position, velocity, health, weapon>(entity),
                                 "entity has all components");

        // Verify we can access components through the mixed registry
        auto& pos = world.get<position>(entity);
        gba::test.expect.eq(pos.x, 50, "position x is correct");

        auto& h = world.get<health>(entity);
        gba::test.expect.eq(h.hp, 80, "health is correct");
    });

    // -- Test complex physics simulation with groups ---------------------------

    gba::test("physics simulation with groups", [] {
        grouped_registry world;

        // Create a few entities
        auto e1 = world.create();
        auto e2 = world.create();

        // Entity 1: high speed, high acceleration
        world.emplace<position>(e1, 0, 0);
        world.emplace<velocity>(e1, 10, 5);
        world.emplace<acceleration>(e1, 1, 2);
        world.emplace<health>(e1, 100);

        // Entity 2: no acceleration, low velocity
        world.emplace<position>(e2, 100, 100);
        world.emplace<velocity>(e2, 1, 1);
        world.emplace<acceleration>(e2, 0, 0);
        world.emplace<health>(e2, 50);

        // Apply physics: update velocities then positions
        world.view<velocity, acceleration>().each([](velocity& v, const acceleration& a) {
            v.vx += a.ax;
            v.vy += a.ay;
        });

        world.view<position, velocity>().each([](position& p, const velocity& v) {
            p.x += v.vx;
            p.y += v.vy;
        });

        // Verify results
        auto& e1_pos = world.get<position>(e1);
        gba::test.expect.eq(e1_pos.x, 11, "entity 1 x position after physics step");
        gba::test.expect.eq(e1_pos.y, 7, "entity 1 y position after physics step");

        auto& e2_pos = world.get<position>(e2);
        gba::test.expect.eq(e2_pos.x, 101, "entity 2 x position after physics step");
        gba::test.expect.eq(e2_pos.y, 101, "entity 2 y position after physics step");
    });

    // -- Test constexpr with groups -------------------------------------------

    gba::test("constexpr registry with groups", [] {
        constexpr auto test_result = [] {
            gba::ecs::registry<8, physics> world;
            auto e = world.create();
            world.emplace<position>(e, 5, 10);
            world.emplace<velocity>(e, 1, 2);

            int sum = 0;
            for (auto [p, v] : world.view<position, velocity>()) {
                sum = p.x + p.y + v.vx + v.vy;
            }
            return sum;
        }();
        gba::test.expect.eq(test_result, 18, "constexpr group registry computed correctly");
    });

    gba::test("constexpr grouped registry supports multi-entity aggregate", [] {
        constexpr auto total = [] {
            gba::ecs::registry<8, physics, health> world;

            auto e0 = world.create();
            world.emplace<position>(e0, 1, 2);
            world.emplace<velocity>(e0, 3, 4);
            world.emplace<acceleration>(e0, 5, 6);
            world.emplace<health>(e0, 10);

            auto e1 = world.create();
            world.emplace<position>(e1, 7, 8);
            world.emplace<velocity>(e1, 9, 10);
            world.emplace<acceleration>(e1, 11, 12);

            int acc = 0;
            world.view<physics>().each([&](const position& p, const velocity& v, const acceleration& a) {
                acc += p.x + v.vx + a.ax;
            });

            world.view<health>().each([&](const health& h) { acc += h.hp; });
            return acc;
        }();

        gba::test.expect.eq(total, 46, "constexpr grouped views produce deterministic aggregate");
    });

    // -- Test implicit view<group> optimization ----------------------------------

    gba::test("view with group optimized accessor", [] {
        grouped_registry world;
        auto e1 = world.create();
        auto e2 = world.create();

        // Entity 1: full components
        world.emplace<position>(e1, 10, 20);
        world.emplace<velocity>(e1, 1, 2);
        world.emplace<acceleration>(e1, 0, 1);
        world.emplace<health>(e1, 100);

        // Entity 2: partial components
        world.emplace<position>(e2, 50, 60);
        world.emplace<velocity>(e2, 2, 3);

        // Use view<group> to iterate all components in physics
        int physics_count = 0;
        world.view<physics>().each([&](position&, velocity&, acceleration&) {
            ++physics_count;
        });
        gba::test.expect.eq(physics_count, 1, "view<group> only matched entity with full group");

        // Compare with manual expansion - should be identical
        int manual_count = 0;
        world.view<position, velocity, acceleration>().each([&](position&, velocity&, acceleration&) {
            ++manual_count;
        });
        gba::test.expect.eq(manual_count, physics_count,
                           "view<group> expands to same results as manual view");
    });

    // -- Test implicit get<group>() optimization ---------------------------------

    gba::test("get with group batch component accessor", [] {
        grouped_registry world;
        auto entity = world.create();

        // Emplace all physics components
        world.emplace<position>(entity, 100, 200);
        world.emplace<velocity>(entity, 10, 20);
        world.emplace<acceleration>(entity, 1, 2);

        // Use get<group> for batch access
        auto [pos, vel, acc] = world.get<physics>(entity);

        gba::test.expect.eq(pos.x, 100, "get<group> position x is correct");
        gba::test.expect.eq(pos.y, 200, "get<group> position y is correct");
        gba::test.expect.eq(vel.vx, 10, "get<group> velocity vx is correct");
        gba::test.expect.eq(vel.vy, 20, "get<group> velocity vy is correct");
        gba::test.expect.eq(acc.ax, 1, "get<group> acceleration ax is correct");
        gba::test.expect.eq(acc.ay, 2, "get<group> acceleration ay is correct");

        // Modify through the tuple references
        pos.x = 999;
        vel.vx = 999;

        // Verify changes persisted
        gba::test.expect.eq(world.get<position>(entity).x, 999, "get<group> modifications persist");
        gba::test.expect.eq(world.get<velocity>(entity).vx, 999, "get<group> modifications persist");
    });

    gba::test("view supports mixed groups and individual components", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 3, 4);
        world.emplace<velocity>(e, 1, 2);
        world.emplace<acceleration>(e, 5, 6);
        world.emplace<health>(e, 42);

        int count = 0;
        world.view<physics, health>().each([
            &
        ](position&, velocity&, acceleration&, health&) { ++count; });
        gba::test.expect.eq(count, 1, "mixed view query matches entity");
    });

    gba::test("get supports mixed groups and individual components", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 7, 8);
        world.emplace<velocity>(e, 9, 10);
        world.emplace<acceleration>(e, 11, 12);
        world.emplace<health>(e, 99);

        auto [pos, vel, acc, hp] = world.get<physics, health>(e);
        gba::test.expect.eq(pos.x, 7, "mixed get position works");
        gba::test.expect.eq(vel.vx, 9, "mixed get velocity works");
        gba::test.expect.eq(acc.ax, 11, "mixed get acceleration works");
        gba::test.expect.eq(hp.hp, 99, "mixed get health works");
    });

    gba::test("try_get supports single and group queries", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 21, 22);
        world.emplace<velocity>(e, 23, 24);
        world.emplace<acceleration>(e, 25, 26);

        auto* pos = world.try_get<position>(e);
        gba::test.expect.is_true(pos != nullptr, "single try_get returns pointer when present");
        gba::test.expect.eq(pos->x, 21, "single try_get value matches");

        auto* hp = world.try_get<health>(e);
        gba::test.expect.is_true(hp == nullptr, "single try_get returns nullptr when missing");

        auto [p, v, a, h] = world.try_get<physics, health>(e);
        gba::test.expect.is_true(p != nullptr && v != nullptr && a != nullptr, "group members are present");
        gba::test.expect.is_true(h == nullptr, "mixed query missing component returns nullptr pointer");
    });

    gba::test("remove_unchecked supports mixed groups and components", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 10, 20);
        world.emplace<velocity>(e, 1, 2);
        world.emplace<acceleration>(e, 3, 4);
        world.emplace<health>(e, 99);

        world.remove_unchecked<physics, health>(e);
        gba::test.expect.is_false(world.all_of<position>(e), "position removed via group remove_unchecked");
        gba::test.expect.is_false(world.all_of<velocity>(e), "velocity removed via group remove_unchecked");
        gba::test.expect.is_false(world.all_of<acceleration>(e), "acceleration removed via group remove_unchecked");
        gba::test.expect.is_false(world.all_of<health>(e), "health removed via mixed remove_unchecked");
    });

    gba::test("with supports mixed groups and entity_id callback", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 30, 40);
        world.emplace<velocity>(e, 2, 3);
        world.emplace<acceleration>(e, 1, 1);
        world.emplace<health>(e, 90);

        bool called = false;
        const bool ok = world.with<physics, health>(e, [&](gba::ecs::entity_id id, position& p, velocity& v, acceleration& a, health& h) {
            called = (id == e);
            p.x += v.vx + a.ax;
            h.hp -= 10;
        });
        gba::test.expect.is_true(ok, "with returns true when all requested components exist");
        gba::test.expect.is_true(called, "with invokes callback with matching entity id");
        gba::test.expect.eq(world.get<position>(e).x, 33, "with can mutate grouped component");
        gba::test.expect.eq(world.get<health>(e).hp, 80, "with can mutate mixed component");

        world.remove<health>(e);
        bool should_not_call = false;
        const bool missing = world.with<physics, health>(e, [&](position&, velocity&, acceleration&, health&) {
            should_not_call = true;
        });
        gba::test.expect.is_false(missing, "with returns false if any requested component is missing");
        gba::test.expect.is_false(should_not_call, "with skips callback when any component missing");
    });

    gba::test("match supports ordered case queries with groups", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 1, 2);
        world.emplace<velocity>(e, 3, 4);
        world.emplace<acceleration>(e, 5, 6);
        world.emplace<health>(e, 50);

        int branch_sum = 0;
        const bool matched = world.match<physics, gba::ecs::group<physics, health>>(
            e,
            [&](position&, velocity&, acceleration&) {
                branch_sum += 1;
                world.remove<health>(e);
            },
            [&](position&, velocity&, acceleration&, health&) { branch_sum += 2; }
        );

        gba::test.expect.is_true(matched, "match returns true when at least one case runs");
        gba::test.expect.eq(branch_sum, 3, "all cases matched at entry execute despite intermediate removal");
    });

    gba::test("match returns false for missing component cases", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 7, 8);
        world.emplace<velocity>(e, 9, 10);

        int branch = 0;
        const bool matched = world.match<gba::ecs::group<physics, health>, health>(
            e,
            [&](position&, velocity&, acceleration&, health&) { branch = 1; },
            [&](health&) { branch = 2; }
        );

        gba::test.expect.is_false(matched, "match returns false when no case matches");
        gba::test.expect.eq(branch, 0, "no callback executes on no-match");
    });

    gba::test("match supports single component and single lambda", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<health>(e, 64);

        bool called = false;
        const bool matched = world.match<health>(e, [&](health& h) {
            called = true;
            h.hp -= 1;
        });

        gba::test.expect.is_true(matched, "single-case match returns true");
        gba::test.expect.is_true(called, "single-case match invokes callback");
        gba::test.expect.eq(world.get<health>(e).hp, 63, "single-case match can mutate component");
    });

    gba::test("match does not include components added mid-dispatch", [] {
        grouped_registry world;
        auto e = world.create();
        world.emplace<position>(e, 11, 22);
        world.emplace<velocity>(e, 1, 1);
        world.emplace<acceleration>(e, 0, 0);

        int branches = 0;
        const bool matched = world.match<physics, gba::ecs::group<physics, health>>(
            e,
            [&](position&, velocity&, acceleration&) {
                ++branches;
                world.emplace<health>(e, 77);
            },
            [&](position&, velocity&, acceleration&, health&) {
                branches += 10;
            }
        );

        gba::test.expect.is_true(matched, "match reports true when initial case matches");
        gba::test.expect.eq(branches, 1, "later case does not run when it did not match at entry");
        gba::test.expect.is_true(world.all_of<health>(e), "component was added by callback");
    });

    gba::test("create_emplace supports mixed groups and components", [] {
        mixed_registry world;
        const auto e = world.create_emplace<physics, health, weapon>(
            position{20, 30},
            velocity{2, 3},
            acceleration{1, 1},
            health{77},
            weapon{12}
        );

        gba::test.expect.is_true(world.all_of<position, velocity, acceleration, health, weapon>(e),
                                 "mixed create_emplace initialized all components");
    });

    // -- Test batch group operations with implicit group query ---------------------

    gba::test("batch group operations using implicit groups", [] {
        grouped_registry world;

        // Create a few entities
        auto e1 = world.create();
        auto e2 = world.create();
        auto e3 = world.create();

        // Setup physics for e1 and e2
        world.emplace<position>(e1, 0, 0);
        world.emplace<velocity>(e1, 5, 0);
        world.emplace<acceleration>(e1, 0, 0);

        world.emplace<position>(e2, 100, 100);
        world.emplace<velocity>(e2, -2, -1);
        world.emplace<acceleration>(e2, 0, 0);

        // e3 only has part of the group
        world.emplace<position>(e3, 50, 50);

        // Batch operation: apply acceleration to velocity
        world.view<physics>().each([](position&, velocity& v, const acceleration& a) {
            v.vx += a.ax;
            v.vy += a.ay;
        });

        // Alternatively, iterate and use get<group> for access:
        int applied = 0;
        for (auto e : {e1, e2, e3}) {
            if (world.all_of<position, velocity, acceleration>(e)) {
                applied++;
            }
        }
        gba::test.expect.eq(applied, 2, "batch operation counted correct entities");
    });

    gba::test("group view stays stable when removing current entity during iteration", [] {
        grouped_registry world;

        auto e0 = world.create();
        auto e1 = world.create();
        auto e2 = world.create();

        world.emplace<position>(e0, 0, 0);
        world.emplace<velocity>(e0, 1, 0);
        world.emplace<acceleration>(e0, 0, 0);

        world.emplace<position>(e1, 10, 0);
        world.emplace<velocity>(e1, 1, 0);
        world.emplace<acceleration>(e1, 0, 0);

        world.emplace<position>(e2, 20, 0);
        world.emplace<velocity>(e2, 1, 0);
        world.emplace<acceleration>(e2, 0, 0);

        int visited = 0;
        int removed = 0;
        world.view<physics>().each([&](gba::ecs::entity_id id, position&, velocity&, acceleration&) {
            ++visited;
            world.destroy(id);
            ++removed;
        });

        gba::test.expect.eq(visited, 3, "all physics entities visited exactly once before removal");
        gba::test.expect.eq(removed, 3, "all visited entities removed");
        gba::test.expect.eq(world.size(), std::size_t{0}, "world empty after in-iteration removals");
        gba::test.expect.is_false(world.valid(e0), "e0 invalid after in-iteration remove");
        gba::test.expect.is_false(world.valid(e1), "e1 invalid after in-iteration remove");
        gba::test.expect.is_false(world.valid(e2), "e2 invalid after in-iteration remove");
    });

    gba::test("overlapping group dedup stress remains query-stable", [] {
        dedup_registry world;
        constexpr int n = 24;

        for (int i = 0; i < n; ++i) {
            auto e = world.create();
            world.emplace<position>(e, i, i + 1);
            world.emplace<velocity>(e, i + 2, i + 3);
            world.emplace<health>(e, 100 - i);
            world.emplace<armor>(e, 10 + i);
            world.emplace<weapon>(e, 20 + i);
        }

        int full_count = 0;
        int armor_sum = 0;
        world.view<position, velocity, health, armor, weapon>().each([&](const position&, const velocity&, const health&, const armor& a, const weapon&) {
            ++full_count;
            armor_sum += a.defense;
        });

        gba::test.expect.eq(full_count, n, "deduplicated registry stores one instance per component in stress case");
        gba::test.expect.eq(armor_sum, 516, "deduplicated stress query yields expected aggregate");
    });

    gba::test("grouped registry clear invalidates entities and supports repopulation", [] {
        grouped_registry world;

        auto e0 = world.create();
        auto e1 = world.create();
        world.emplace<position>(e0, 1, 2);
        world.emplace<velocity>(e0, 3, 4);
        world.emplace<acceleration>(e0, 5, 6);
        world.emplace<health>(e1, 99);

        world.clear();

        gba::test.expect.eq(world.size(), std::size_t{0}, "clear resets grouped registry size");
        gba::test.expect.is_false(world.valid(e0), "clear invalidates old grouped entity e0");
        gba::test.expect.is_false(world.valid(e1), "clear invalidates old grouped entity e1");

        int physics_count = 0;
        world.view<physics>().each([&](const position&, const velocity&, const acceleration&) { ++physics_count; });
        gba::test.expect.eq(physics_count, 0, "group views are empty after clear");

        auto e2 = world.create();
        world.emplace<position>(e2, 7, 8);
        world.emplace<velocity>(e2, 9, 10);
        world.emplace<acceleration>(e2, 11, 12);
        gba::test.expect.is_true(world.valid(e2), "registry can be repopulated after clear");
        gba::test.expect.is_true(world.all_of<physics>(e2), "repopulated entity can satisfy grouped query");
    });

    gba::test("overlapping groups are deduplicated in registry", [] {
        dedup_registry world;
        auto e = world.create();
        world.emplace<position>(e, 1, 2);
        world.emplace<velocity>(e, 3, 4);
        world.emplace<health>(e, 5);
        world.emplace<armor>(e, 6);
        world.emplace<weapon>(e, 7);

        gba::test.expect.is_true(world.all_of<position, velocity, health, armor, weapon>(e),
                                 "entity contains deduplicated overlapping-group components");

        int count = 0;
        world.view<position, velocity, armor>().each([&](const position&, const velocity&, const armor&) { ++count; });
        gba::test.expect.eq(count, 1, "view works with deduplicated overlapping groups");
    });

    return 0;
}
