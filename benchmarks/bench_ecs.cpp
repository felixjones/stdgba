/// @file benchmarks/bench_ecs.cpp
/// @brief gba::ecs benchmark for common spawn, iteration, and destroy workloads.
///
/// Measures the per-frame cycle cost of the most common ECS operations using
/// gba::ecs::registry (flat dense arrays, zero allocation, compile-time component list).
///
/// Components (21 bytes/entity):
///   position  - int x, y          (8 bytes)
///   velocity  - int vx, vy        (8 bytes)
///   health    - int hp             (4 bytes)
///   sprite_id - uint8_t id         (1 byte)
///
/// Scenarios measured at N in {32, 64, 128}:
///   spawn     - Nxcreate + Nx4 emplace (zero heap allocation).
///   iter_mv   - view<position,velocity>.each() Euler step, averaged 64 frames.
///   iter_full - view<position,velocity,health>.each() + health decay, avg 64.
///   destroy   - destroy(e) x N.
///
/// Run with: timeout 15 mgba-headless -S 0x1A -R r0 -t 10 build/benchmarks/bench_ecs.elf

#include <gba/ecs>
#include <gba/fixed_point>

#include <array>
#include <cstdint>

#include <gba/benchmark>

using namespace gba::literals;

namespace {

    // -- GBA-realistic component types ---------------------------------------------

    struct position {
        gba::fixed<int, 8> x{};
        gba::fixed<int, 8> y{};
    };

    struct velocity {
        gba::fixed<int, 8> vx{};
        gba::fixed<int, 8> vy{};
    };

    struct health {
        int hp;
    };

    struct sprite_id {
        std::uint8_t id;
    };

    // Extra 1-byte tags for wide destroy-path benchmarking.
    struct tag0 { std::uint8_t v; };
    struct tag1 { std::uint8_t v; };
    struct tag2 { std::uint8_t v; };
    struct tag3 { std::uint8_t v; };
    struct tag4 { std::uint8_t v; };
    struct tag5 { std::uint8_t v; };
    struct tag6 { std::uint8_t v; };
    struct tag7 { std::uint8_t v; };
    struct tag8 { std::uint8_t v; };
    struct tag9 { std::uint8_t v; };
    struct tag10 { std::uint8_t v; };
    struct tag11 { std::uint8_t v; };

    // Verify power-of-two sizes
    static_assert(std::has_single_bit(sizeof(position)));  // 8
    static_assert(std::has_single_bit(sizeof(velocity)));  // 8
    static_assert(std::has_single_bit(sizeof(health)));    // 4
    static_assert(std::has_single_bit(sizeof(sprite_id))); // 1
    static_assert(std::has_single_bit(sizeof(tag0)));

    // Compile-time velocity constant
    static constexpr gba::fixed<int, 8> vel_x{1.0};
    static constexpr gba::fixed<int, 8> vel_y{0.5};

    // Component groups for testing optimizations
    using physics = gba::ecs::group<position, velocity>;
    using full = gba::ecs::group<position, velocity, health>;
    using spawn = gba::ecs::group<position, velocity, health, sprite_id>;

    using registry_type = gba::ecs::registry<128, position, velocity, health, sprite_id>;
    using wide_registry_type = gba::ecs::registry<128,
        position, velocity, health, sprite_id,
        tag0, tag1, tag2, tag3, tag4, tag5, tag6, tag7, tag8, tag9, tag10, tag11>;

    // Volatile sinks (prevent DCE)
    volatile int sink_acc = 0;

    // Mutable benchmark context used by no-arg microbench wrappers.
    registry_type* g_with_reg = nullptr;
    gba::entity g_with_entity = gba::entity_null;

    // -- EWRAM-resident registry for realistic game scenario measurement ----------
    // A real game typically has the registry as a global/static (EWRAM BSS).
    // Stack-local registries land in IWRAM (1-cycle access) - faster but atypical.
    static registry_type g_ewram_reg;

    // -- Per-frame iteration functions --------------------------------------------

    [[gnu::noinline]]
    void iter_movement(registry_type* reg) {
        int acc = 0;
        reg->view<position, velocity>().each_arm([&acc](position& pos, const velocity& vel) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            acc += gba::bit_cast(pos.x);
        });
        sink_acc = acc;
    }

    [[gnu::noinline]]
    void iter_full_update(registry_type* reg) {
        int acc = 0;
        reg->view<position, velocity, health>().each_arm([&acc](position& pos, const velocity& vel, health& hp) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            if (hp.hp > 0) --hp.hp;
            acc += gba::bit_cast(pos.x) + hp.hp;
        });
        sink_acc = acc;
    }

    // -- Group-optimized iteration (should compile to identical code) -----------

    [[gnu::noinline]]
    void iter_movement_group(registry_type* reg) {
        int acc = 0;
        reg->view<physics>().each_arm([&acc](position& pos, const velocity& vel) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            acc += gba::bit_cast(pos.x);
        });
        sink_acc = acc;
    }

    [[gnu::noinline]]
    void iter_full_update_group(registry_type* reg) {
        int acc = 0;
        reg->view<full>().each_arm([&acc](position& pos, const velocity& vel, health& hp) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            if (hp.hp > 0) --hp.hp;
            acc += gba::bit_cast(pos.x) + hp.hp;
        });
        sink_acc = acc;
    }

    // -- Group batch access (should compile to identical code as manual get calls) --

    [[gnu::noinline]]
    void batch_update_manual(registry_type* reg, gba::entity entity) {
        auto& pos = reg->get<position>(entity);
        auto& vel = reg->get<velocity>(entity);
        auto& hp = reg->get<health>(entity);
        pos.x += vel.vx;
        pos.y += vel.vy;
        if (hp.hp > 0) --hp.hp;
        sink_acc = gba::bit_cast(pos.x) + hp.hp;
    }

    [[gnu::noinline]]
    void batch_update_group(registry_type* reg, gba::entity entity) {
        auto [pos, vel, hp] = reg->get<full>(entity);
        pos.x += vel.vx;
        pos.y += vel.vy;
        if (hp.hp > 0) --hp.hp;
        sink_acc = gba::bit_cast(pos.x) + hp.hp;
    }

    // -- with() microbench kernels -----------------------------------------------

    [[gnu::noinline]]
    void with_hit_thumb(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->with<position, velocity, health>(entity, [&](position& pos, const velocity& vel, health& hp) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            if (hp.hp > 0) --hp.hp;
            acc += gba::bit_cast(pos.x) + hp.hp;
        });
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::noinline]]
    void with_miss_thumb(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->with<position, velocity, health>(entity, [&](position&, const velocity&, health&) {
            ++acc;
        });
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_with"), gnu::noinline, gnu::flatten]]
    void with_hit_arm(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->with<position, velocity, health>(entity, [&](position& pos, const velocity& vel, health& hp) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            if (hp.hp > 0) --hp.hp;
            acc += gba::bit_cast(pos.x) + hp.hp;
        });
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_with"), gnu::noinline, gnu::flatten]]
    void with_miss_arm(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->with<position, velocity, health>(entity, [&](position&, const velocity&, health&) {
            ++acc;
        });
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::noinline]] void bench_with_hit_thumb() { with_hit_thumb(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_with_miss_thumb() { with_miss_thumb(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_with_hit_arm() { with_hit_arm(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_with_miss_arm() { with_miss_arm(g_with_reg, g_with_entity); }

    [[gnu::noinline]]
    void match_hit_thumb(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->template match<sprite_id, gba::ecs::group<position, velocity, health>>(
            entity,
            [&](sprite_id&) { ++acc; },
            [&](position& pos, const velocity& vel, health& hp) {
                pos.x += vel.vx;
                if (hp.hp > 0) --hp.hp;
                acc += gba::bit_cast(pos.x) + hp.hp;
            }
        );
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::noinline]]
    void match_miss_thumb(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->template match<gba::ecs::group<position, velocity, health>, health>(
            entity,
            [&](position&, const velocity&, health&) { acc += 3; },
            [&](health&) { acc += 5; }
        );
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_match"), gnu::noinline, gnu::flatten]]
    void match_hit_arm(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->template match_arm<sprite_id, gba::ecs::group<position, velocity, health>>(
            entity,
            [&](sprite_id&) { ++acc; },
            [&](position& pos, const velocity& vel, health& hp) {
                pos.x += vel.vx;
                if (hp.hp > 0) --hp.hp;
                acc += gba::bit_cast(pos.x) + hp.hp;
            }
        );
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::target("arm"), gnu::section(".iwram._gba_ecs_match"), gnu::noinline, gnu::flatten]]
    void match_miss_arm(registry_type* reg, gba::entity entity) {
        int acc = 0;
        const bool ok = reg->template match_arm<gba::ecs::group<position, velocity, health>, health>(
            entity,
            [&](position&, const velocity&, health&) { acc += 3; },
            [&](health&) { acc += 5; }
        );
        sink_acc = acc + (ok ? 1 : 0);
    }

    [[gnu::noinline]] void bench_match_hit_thumb() { match_hit_thumb(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_match_miss_thumb() { match_miss_thumb(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_match_hit_arm() { match_hit_arm(g_with_reg, g_with_entity); }
    [[gnu::noinline]] void bench_match_miss_arm() { match_miss_arm(g_with_reg, g_with_entity); }

    // -- Scenario runner ----------------------------------------------------------

    template<int N>
    [[gnu::noinline]]
    void run_scenario() {
        static constexpr unsigned int vblank = 67000u;

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- N = {n} (IWRAM stack) ---"_fmt, "n"_arg = N);
            gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {vbl:>4}  {cyce:>6}"_fmt,
                       "operation"_arg = "operation", "cycles"_arg = "cycles", "vbl"_arg = "vbl%", "cyce"_arg = "cyc/e");
        });

        gba::ecs::registry<128, position, velocity, health, sprite_id> reg;
        std::array<gba::entity, N> entities{};

        // -- spawn: Nxcreate + Nx4 emplace --------------------------------
        {
            const auto spawn_cyc = gba::benchmark::measure([&] {
                for (int i = 0; i < N; ++i) {
                    const auto e = reg.create();
                    entities[i] = e;
                    reg.emplace<position>(e);
                    reg.emplace<velocity>(e, vel_x, vel_y);
                    reg.emplace<health>(e, 100);
                    reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                }
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "spawn (4cmp, 0 heap)",
                           "cycles"_arg = spawn_cyc,
                           "pct"_arg = (spawn_cyc * 100u) / vblank,
                           "cycpe"_arg = spawn_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- iter_mv: view<position,velocity> averaged over 64 frames -----
        {
            const auto mv_cyc = gba::benchmark::measure_avg(64, iter_movement, &reg);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel> avg64",
                           "cycles"_arg = mv_cyc,
                           "pct"_arg = (mv_cyc * 100u) / vblank,
                           "cycpe"_arg = mv_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- iter_full: view<position,velocity,health> averaged over 64 frames
        {
            const auto full_cyc = gba::benchmark::measure_avg(64, iter_full_update, &reg);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel,hp> avg64",
                           "cycles"_arg = full_cyc,
                           "pct"_arg = (full_cyc * 100u) / vblank,
                           "cycpe"_arg = full_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- destroy: all N entities --------------------------------------
        {
            const auto destroy_cyc = gba::benchmark::measure([&] {
                for (const auto e : entities) reg.destroy(e);
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "destroy all",
                           "cycles"_arg = destroy_cyc,
                           "pct"_arg = (destroy_cyc * 100u) / vblank,
                           "cycpe"_arg = destroy_cyc / static_cast<unsigned int>(N));
            });
        }
    }

    /// EWRAM scenario: registry lives in global BSS (0x02xxxxxx).
    /// This is the realistic game deployment where the registry is a member of
    /// a scene/world struct or a file-scope global - data access hits the 16-bit
    /// EWRAM bus instead of the 32-bit IWRAM bus.
    template<int N>
    [[gnu::noinline]]
    void run_ewram_scenario() {
        static constexpr unsigned int vblank = 67000u;

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- N = {n} (EWRAM global) ---"_fmt, "n"_arg = N);
            gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {vbl:>4}  {cyce:>6}"_fmt,
                       "operation"_arg = "operation", "cycles"_arg = "cycles", "vbl"_arg = "vbl%", "cyce"_arg = "cyc/e");
        });

        auto& reg = g_ewram_reg;
        reg.clear();
        std::array<gba::entity, N> entities{};

        {
            const auto spawn_cyc = gba::benchmark::measure([&] {
                for (int i = 0; i < N; ++i) {
                    const auto e = reg.create();
                    entities[i] = e;
                    reg.emplace<position>(e);
                    reg.emplace<velocity>(e, vel_x, vel_y);
                    reg.emplace<health>(e, 100);
                    reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                }
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "spawn (4cmp, 0 heap)",
                           "cycles"_arg = spawn_cyc,
                           "pct"_arg = (spawn_cyc * 100u) / vblank,
                           "cycpe"_arg = spawn_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto mv_cyc = gba::benchmark::measure_avg(64, iter_movement, &reg);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel> avg64",
                           "cycles"_arg = mv_cyc,
                           "pct"_arg = (mv_cyc * 100u) / vblank,
                           "cycpe"_arg = mv_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto full_cyc = gba::benchmark::measure_avg(64, iter_full_update, &reg);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel,hp> avg64",
                           "cycles"_arg = full_cyc,
                           "pct"_arg = (full_cyc * 100u) / vblank,
                           "cycpe"_arg = full_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto destroy_cyc = gba::benchmark::measure([&] {
                for (const auto e : entities) reg.destroy(e);
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "destroy all",
                           "cycles"_arg = destroy_cyc,
                           "pct"_arg = (destroy_cyc * 100u) / vblank,
                           "cycpe"_arg = destroy_cyc / static_cast<unsigned int>(N));
            });
        }
    }

    // -- Group optimization scenario -----------------------------------------------
    // Proves that implicit group queries in view/get have ZERO overhead
    // compared to manual flat view<Cs...>() and get<C>() calls.
    [[gnu::noinline]]
    void run_group_optimization_scenario() {
        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- Group Optimization Comparison (N=128, IWRAM) ---"_fmt);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                       "operation"_arg = "operation", "cycles"_arg = "cycles", "diff"_arg = "diff");
        });

        // -- Test 0: spawn manual vs fused create_emplace -------------------------
        {
            registry_type manual_reg;
            std::array<gba::entity, 128> manual_entities{};
            const auto manual_cyc = gba::benchmark::measure([&] {
                for (int i = 0; i < 128; ++i) {
                    const auto e = manual_reg.create();
                    manual_entities[i] = e;
                    manual_reg.emplace<position>(e);
                    manual_reg.emplace<velocity>(e, vel_x, vel_y);
                    manual_reg.emplace<health>(e, 100);
                    manual_reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                }
            });

            registry_type fused_reg;
            std::array<gba::entity, 128> fused_entities{};
            const auto fused_cyc = gba::benchmark::measure([&] {
                for (int i = 0; i < 128; ++i) {
                    fused_entities[i] = fused_reg.template create_emplace<spawn>(
                        position{},
                        velocity{vel_x, vel_y},
                        health{100},
                        sprite_id{static_cast<std::uint8_t>(i & 0x7F)}
                    );
                }
            });

            const int diff = static_cast<int>(manual_cyc) - static_cast<int>(fused_cyc);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "spawn manual (create+4emplace)",
                           "cycles"_arg = manual_cyc,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "spawn fused create_emplace",
                           "cycles"_arg = fused_cyc,
                           "diff"_arg = diff);
            });
        }

        gba::ecs::registry<128, position, velocity, health, sprite_id> reg;
        std::array<gba::entity, 128> entities{};

        // Setup: create 128 entities with all components
        for (int i = 0; i < 128; ++i) {
            const auto e = reg.create();
            entities[i] = e;
            reg.emplace<position>(e);
            reg.emplace<velocity>(e, vel_x, vel_y);
            reg.emplace<health>(e, 100);
            reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
        }

        // -- Test 1: view<pos,vel> vs view<physics> averaged over 64 frames
        {
            const auto manual_cyc = gba::benchmark::measure_avg(64, iter_movement, &reg);
            const auto group_cyc = gba::benchmark::measure_avg(64, iter_movement_group, &reg);
            int diff = static_cast<int>(manual_cyc) - static_cast<int>(group_cyc);

            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel> manual avg64",
                           "cycles"_arg = manual_cyc,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "view<physics> avg64",
                           "cycles"_arg = group_cyc,
                           "diff"_arg = diff);
            });
        }

        // -- Test 2: view<pos,vel,hp> vs view<full> averaged over 64 frames
        {
            const auto manual_cyc = gba::benchmark::measure_avg(64, iter_full_update, &reg);
            const auto group_cyc = gba::benchmark::measure_avg(64, iter_full_update_group, &reg);
            int diff = static_cast<int>(manual_cyc) - static_cast<int>(group_cyc);

            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "iter<pos,vel,hp> manual avg64",
                           "cycles"_arg = manual_cyc,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "view<full> avg64",
                           "cycles"_arg = group_cyc,
                           "diff"_arg = diff);
            });
        }

        // -- Test 3: manual get<> calls vs get<full> (single entity operation)
        {
            const auto e = entities[0];  // First entity
            const auto manual_cyc = gba::benchmark::measure_avg(64, [&] { batch_update_manual(&reg, e); });
            const auto group_cyc = gba::benchmark::measure_avg(64, [&] { batch_update_group(&reg, e); });
            int diff = static_cast<int>(manual_cyc) - static_cast<int>(group_cyc);

            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "batch update manual avg64",
                           "cycles"_arg = manual_cyc,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "get<full> avg64",
                           "cycles"_arg = group_cyc,
                           "diff"_arg = diff);
            });
        }

        // -- Test 4: dense fast-path recovery after churn -------------------------
        {
            registry_type churn_reg;
            std::array<gba::entity, 128> churn_entities{};
            for (int i = 0; i < 128; ++i) {
                const auto e = churn_reg.create();
                churn_entities[i] = e;
                churn_reg.emplace<position>(e);
                churn_reg.emplace<velocity>(e, vel_x, vel_y);
                churn_reg.emplace<health>(e, 100);
            }

            const auto dense_cyc = gba::benchmark::measure_avg(64, iter_movement, &churn_reg);

            // Create holes in the low prefix, then refill to recover prefix density.
            for (int i = 0; i < 32; ++i) churn_reg.destroy(churn_entities[i]);
            for (int i = 0; i < 32; ++i) {
                [[maybe_unused]] const auto refill = churn_reg.template create_emplace<full>(
                    position{},
                    velocity{vel_x, vel_y},
                    health{100}
                );
            }

            const auto recovered_cyc = gba::benchmark::measure_avg(64, iter_movement, &churn_reg);
            const int diff = static_cast<int>(dense_cyc) - static_cast<int>(recovered_cyc);

            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "iter dense baseline avg64",
                           "cycles"_arg = dense_cyc,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "iter post-churn recovered avg64",
                           "cycles"_arg = recovered_cyc,
                           "diff"_arg = diff);
            });
        }

        // -- Test 5: destroy scaling with wide sparse/full occupancy --------------
        {
            wide_registry_type sparse_reg;
            std::array<gba::entity, 128> sparse_entities{};
            for (int i = 0; i < 128; ++i) {
                const auto e = sparse_reg.create();
                sparse_entities[i] = e;
                sparse_reg.emplace<position>(e);
                sparse_reg.emplace<health>(e, 100);
            }

            const auto sparse_destroy = gba::benchmark::measure([&] {
                for (const auto e : sparse_entities) sparse_reg.destroy(e);
            });

            wide_registry_type full_reg;
            std::array<gba::entity, 128> full_entities{};
            for (int i = 0; i < 128; ++i) {
                const auto e = full_reg.create();
                full_entities[i] = e;
                full_reg.emplace<position>(e);
                full_reg.emplace<velocity>(e, vel_x, vel_y);
                full_reg.emplace<health>(e, 100);
                full_reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                full_reg.emplace<tag0>(e, std::uint8_t{0});  full_reg.emplace<tag1>(e, std::uint8_t{1});
                full_reg.emplace<tag2>(e, std::uint8_t{2});  full_reg.emplace<tag3>(e, std::uint8_t{3});
                full_reg.emplace<tag4>(e, std::uint8_t{4});  full_reg.emplace<tag5>(e, std::uint8_t{5});
                full_reg.emplace<tag6>(e, std::uint8_t{6});  full_reg.emplace<tag7>(e, std::uint8_t{7});
                full_reg.emplace<tag8>(e, std::uint8_t{8});  full_reg.emplace<tag9>(e, std::uint8_t{9});
                full_reg.emplace<tag10>(e, std::uint8_t{10}); full_reg.emplace<tag11>(e, std::uint8_t{11});
            }

            const auto full_destroy = gba::benchmark::measure([&] {
                for (const auto e : full_entities) full_reg.destroy(e);
            });

            const int diff = static_cast<int>(full_destroy) - static_cast<int>(sparse_destroy);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "destroy wide sparse (2/16 cmp)",
                           "cycles"_arg = sparse_destroy,
                           "diff"_arg = "baseline");
            });
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                           "operation"_arg = "destroy wide full (16/16 cmp)",
                           "cycles"_arg = full_destroy,
                           "diff"_arg = diff);
            });
        }
    }

    // -- with() positive/negative microbenchmark ---------------------------------
    [[gnu::noinline]]
    void run_with_microbenchmark() {
        static constexpr int iters = 512;

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- with() Microbenchmark (hit vs miss) ---"_fmt);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "operation", "cycles"_arg = "cycles", "diff"_arg = "diff");
        });

        registry_type hit_reg;
        const auto hit_entity = hit_reg.create();
        hit_reg.emplace<position>(hit_entity);
        hit_reg.emplace<velocity>(hit_entity, vel_x, vel_y);
        hit_reg.emplace<health>(hit_entity, 100);

        registry_type miss_reg;
        const auto miss_entity = miss_reg.create();
        miss_reg.emplace<position>(miss_entity);
        miss_reg.emplace<velocity>(miss_entity, vel_x, vel_y);

        g_with_reg = &hit_reg;
        g_with_entity = hit_entity;
        const auto hit_thumb = gba::benchmark::measure_avg_net(iters, [] { bench_with_hit_thumb(); });
        const auto hit_arm = gba::benchmark::measure_avg_net(iters, [] { bench_with_hit_arm(); });

        g_with_reg = &miss_reg;
        g_with_entity = miss_entity;
        const auto miss_thumb = gba::benchmark::measure_avg_net(iters, [] { bench_with_miss_thumb(); });
        const auto miss_arm = gba::benchmark::measure_avg_net(iters, [] { bench_with_miss_arm(); });

        const int hit_diff = static_cast<int>(hit_thumb) - static_cast<int>(hit_arm);
        const int miss_diff = static_cast<int>(miss_thumb) - static_cast<int>(miss_arm);

        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "with hit (Thumb caller)",
                                "cycles"_arg = hit_thumb,
                                "diff"_arg = "baseline");
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "with hit (ARM caller)",
                                "cycles"_arg = hit_arm,
                                "diff"_arg = hit_diff);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "with miss (Thumb caller)",
                                "cycles"_arg = miss_thumb,
                                "diff"_arg = "baseline");
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "with miss (ARM caller)",
                                "cycles"_arg = miss_arm,
                                "diff"_arg = miss_diff);
        });
    }

    [[gnu::noinline]]
    void run_remove_microbenchmark() {
        static constexpr int iters = 128;
        static constexpr int n = 64;

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- remove() Microbenchmark (N=64) ---"_fmt);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "operation", "cycles"_arg = "cycles", "diff"_arg = "diff");
        });

        std::array<gba::entity, n> entities{};
        std::array<health*, n> health_ptrs{};

        registry_type reg_remove;
        for (int i = 0; i < n; ++i) {
            const auto e = reg_remove.create();
            entities[i] = e;
            reg_remove.emplace<health>(e, 100);
        }

        registry_type reg_unchecked_id;
        for (int i = 0; i < n; ++i) {
            const auto e = reg_unchecked_id.create();
            entities[i] = e;
            reg_unchecked_id.emplace<health>(e, 100);
        }

        registry_type reg_unchecked_ref;
        std::array<gba::entity, n> ref_entities{};
        for (int i = 0; i < n; ++i) {
            const auto e = reg_unchecked_ref.create();
            ref_entities[i] = e;
            auto& h = reg_unchecked_ref.emplace<health>(e, 100);
            health_ptrs[i] = &h;
        }

        const auto checked = gba::benchmark::measure_avg_net(iters, [&] {
            for (int i = 0; i < n; ++i) {
                reg_remove.remove<health>(entities[i]);
                reg_remove.emplace<health>(entities[i], 100);
            }
        });

        const auto unchecked_ref = gba::benchmark::measure_avg_net(iters, [&] {
            for (int i = 0; i < n; ++i) {
                reg_unchecked_ref.remove_unchecked<health>(*health_ptrs[i]);
                reg_unchecked_ref.emplace<health>(ref_entities[i], 100);
            }
        });

        const int diff_ref = static_cast<int>(checked) - static_cast<int>(unchecked_ref);

        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "remove<health> (baseline)",
                                "cycles"_arg = checked,
                                "diff"_arg = "baseline");
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "remove_unchecked<health>(ref)",
                                "cycles"_arg = unchecked_ref,
                                "diff"_arg = diff_ref);
        });
    }

    [[gnu::noinline]]
    void run_match_microbenchmark() {
        static constexpr int iters = 512;

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- match() Microbenchmark (hit vs miss) ---"_fmt);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "operation", "cycles"_arg = "cycles", "diff"_arg = "diff");
        });

        registry_type hit_reg;
        const auto hit_entity = hit_reg.create();
        hit_reg.emplace<position>(hit_entity);
        hit_reg.emplace<velocity>(hit_entity, vel_x, vel_y);
        hit_reg.emplace<health>(hit_entity, 100);

        registry_type miss_reg;
        const auto miss_entity = miss_reg.create();
        miss_reg.emplace<position>(miss_entity);
        miss_reg.emplace<velocity>(miss_entity, vel_x, vel_y);

        g_with_reg = &hit_reg;
        g_with_entity = hit_entity;
        const auto hit_thumb = gba::benchmark::measure_avg_net(iters, [] { bench_match_hit_thumb(); });
        const auto hit_arm = gba::benchmark::measure_avg_net(iters, [] { bench_match_hit_arm(); });

        g_with_reg = &miss_reg;
        g_with_entity = miss_entity;
        const auto miss_thumb = gba::benchmark::measure_avg_net(iters, [] { bench_match_miss_thumb(); });
        const auto miss_arm = gba::benchmark::measure_avg_net(iters, [] { bench_match_miss_arm(); });

        const int hit_diff = static_cast<int>(hit_thumb) - static_cast<int>(hit_arm);
        const int miss_diff = static_cast<int>(miss_thumb) - static_cast<int>(miss_arm);

        gba::benchmark::with_logger([&] {
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "match hit (Thumb caller)",
                                "cycles"_arg = hit_thumb,
                                "diff"_arg = "baseline");
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "match hit (ARM caller)",
                                "cycles"_arg = hit_arm,
                                "diff"_arg = hit_diff);
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "match miss (Thumb caller)",
                                "cycles"_arg = miss_thumb,
                                "diff"_arg = "baseline");
            gba::benchmark::log(gba::log::level::info, "  {operation:<36}  {cycles:>7}  {diff:>6}"_fmt,
                                "operation"_arg = "match miss (ARM caller)",
                                "cycles"_arg = miss_arm,
                                "diff"_arg = miss_diff);
        });
    }

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== gba::ecs benchmark (cycles, each_arm) ===");
        gba::benchmark::log(gba::log::level::info, "  Flat dense arrays, 0 heap alloc, compile-time component list");
        gba::benchmark::log(gba::log::level::info, "  Components: pos(8B) vel(8B) hp(4B) sprite_id(1B) = 21B/entity");
        gba::benchmark::log(gba::log::level::info, "  VBlank budget: ~67000 cycles @ 16.78 MHz");
    });

    // IWRAM stack scenarios (best case - all data in 32-bit IWRAM)
    run_scenario<32>();
    run_scenario<64>();
    run_scenario<128>();

    // EWRAM global scenarios (realistic game - data in 16-bit EWRAM)
    run_ewram_scenario<128>();

    // Group optimization scenarios (proves zero overhead)
    run_group_optimization_scenario();

    // with() microbenchmark (positive/negative paths)
    run_with_microbenchmark();

    // match() microbenchmark (conditional selection paths)
    run_match_microbenchmark();

    // remove() microbenchmark (checked vs unchecked)
    run_remove_microbenchmark();

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
