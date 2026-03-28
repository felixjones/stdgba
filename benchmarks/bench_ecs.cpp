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

#include "bench.hpp"

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

    // Verify power-of-two sizes
    static_assert(std::has_single_bit(sizeof(position)));  // 8
    static_assert(std::has_single_bit(sizeof(velocity)));  // 8
    static_assert(std::has_single_bit(sizeof(health)));    // 4
    static_assert(std::has_single_bit(sizeof(sprite_id))); // 1

    // Compile-time velocity constant
    static constexpr gba::fixed<int, 8> vel_x{1.0};
    static constexpr gba::fixed<int, 8> vel_y{0.5};

    // Volatile sinks (prevent DCE)
    volatile int sink_acc = 0;

    using registry_type = gba::ecs::registry<128, position, velocity, health, sprite_id>;

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

    // -- Scenario runner ----------------------------------------------------------

    template<int N>
    [[gnu::noinline]]
    void run_scenario() {
        static constexpr unsigned int vblank = 67000u;

        bench::with_logger([] {
            bench::log_printf(gba::log::level::info, "");
            bench::log_printf(gba::log::level::info, "--- N = %d (IWRAM stack) ---", N);
            bench::log_printf(gba::log::level::info, "  %-30s  %7s  %4s  %6s", "operation", "cycles", "vbl%", "cyc/e");
        });

        gba::ecs::registry<128, position, velocity, health, sprite_id> reg;
        std::array<gba::ecs::entity_id, N> entities{};

        // -- spawn: Nxcreate + Nx4 emplace --------------------------------
        {
            const auto spawn_cyc = bench::measure([&] {
                for (int i = 0; i < N; ++i) {
                    const auto e = reg.create();
                    entities[i] = e;
                    reg.emplace<position>(e);
                    reg.emplace<velocity>(e, vel_x, vel_y);
                    reg.emplace<health>(e, 100);
                    reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                }
            });
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "spawn (4cmp, 0 heap)", spawn_cyc,
                                  (spawn_cyc * 100u) / vblank, spawn_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- iter_mv: view<position,velocity> averaged over 64 frames -----
        {
            const auto mv_cyc = bench::measure_avg(64, iter_movement, &reg);
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "iter<pos,vel> avg64", mv_cyc,
                                  (mv_cyc * 100u) / vblank, mv_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- iter_full: view<position,velocity,health> averaged over 64 frames
        {
            const auto full_cyc = bench::measure_avg(64, iter_full_update, &reg);
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "iter<pos,vel,hp> avg64", full_cyc,
                                  (full_cyc * 100u) / vblank, full_cyc / static_cast<unsigned int>(N));
            });
        }

        // -- destroy: all N entities --------------------------------------
        {
            const auto destroy_cyc = bench::measure([&] {
                for (const auto e : entities) reg.destroy(e);
            });
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "destroy all", destroy_cyc,
                                  (destroy_cyc * 100u) / vblank, destroy_cyc / static_cast<unsigned int>(N));
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

        bench::with_logger([] {
            bench::log_printf(gba::log::level::info, "");
            bench::log_printf(gba::log::level::info, "--- N = %d (EWRAM global) ---", N);
            bench::log_printf(gba::log::level::info, "  %-30s  %7s  %4s  %6s", "operation", "cycles", "vbl%", "cyc/e");
        });

        auto& reg = g_ewram_reg;
        reg.clear();
        std::array<gba::ecs::entity_id, N> entities{};

        {
            const auto spawn_cyc = bench::measure([&] {
                for (int i = 0; i < N; ++i) {
                    const auto e = reg.create();
                    entities[i] = e;
                    reg.emplace<position>(e);
                    reg.emplace<velocity>(e, vel_x, vel_y);
                    reg.emplace<health>(e, 100);
                    reg.emplace<sprite_id>(e, static_cast<std::uint8_t>(i & 0x7F));
                }
            });
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "spawn (4cmp, 0 heap)", spawn_cyc,
                                  (spawn_cyc * 100u) / vblank, spawn_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto mv_cyc = bench::measure_avg(64, iter_movement, &reg);
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "iter<pos,vel> avg64", mv_cyc,
                                  (mv_cyc * 100u) / vblank, mv_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto full_cyc = bench::measure_avg(64, iter_full_update, &reg);
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "iter<pos,vel,hp> avg64", full_cyc,
                                  (full_cyc * 100u) / vblank, full_cyc / static_cast<unsigned int>(N));
            });
        }

        {
            const auto destroy_cyc = bench::measure([&] {
                for (const auto e : entities) reg.destroy(e);
            });
            bench::with_logger([&] {
                bench::log_printf(gba::log::level::info, "  %-30s  %7u  %3u%%  %6u", "destroy all", destroy_cyc,
                                  (destroy_cyc * 100u) / vblank, destroy_cyc / static_cast<unsigned int>(N));
            });
        }
    }

} // namespace

int main() {
    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "=== gba::ecs benchmark (cycles, each_arm) ===");
        bench::log_printf(gba::log::level::info, "  Flat dense arrays, 0 heap alloc, compile-time component list");
        bench::log_printf(gba::log::level::info, "  Components: pos(8B) vel(8B) hp(4B) sprite_id(1B) = 21B/entity");
        bench::log_printf(gba::log::level::info, "  VBlank budget: ~67000 cycles @ 16.78 MHz");
    });

    // IWRAM stack scenarios (best case - all data in 32-bit IWRAM)
    run_scenario<32>();
    run_scenario<64>();
    run_scenario<128>();

    // EWRAM global scenarios (realistic game - data in 16-bit EWRAM)
    run_ewram_scenario<128>();

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
