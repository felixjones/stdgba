/// @file benchmarks/bench_ecs_debug.cpp
/// @brief gba::ecs -O0 debug benchmark.
///
/// Identical to bench_ecs.cpp but compiled at -O0 to measure unoptimized codegen.
/// Useful for checking whether debug builds stay responsive during development.
///
/// Run with: timeout 15 mgba-headless -S 0x1A -R r0 -t 10 build/benchmarks/bench_ecs_debug.elf

#include <gba/ecs>
#include <gba/fixed_point>

#include <array>
#include <cstdint>

#include "bench.hpp"

namespace {

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

    static constexpr gba::fixed<int, 8> vel_x{1.0};
    static constexpr gba::fixed<int, 8> vel_y{0.5};

    volatile int sink_acc = 0;

    [[gnu::noinline]]
    void iter_movement(gba::ecs::registry<128, position, velocity, health, sprite_id>* reg) {
        int acc = 0;
        reg->view<position, velocity>().each([&acc](position& pos, const velocity& vel) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            acc += gba::bit_cast(pos.x);
        });
        sink_acc = acc;
    }

    [[gnu::noinline]]
    void iter_full_update(gba::ecs::registry<128, position, velocity, health, sprite_id>* reg) {
        int acc = 0;
        reg->view<position, velocity, health>().each([&acc](position& pos, const velocity& vel, health& hp) {
            pos.x += vel.vx;
            pos.y += vel.vy;
            if (hp.hp > 0) --hp.hp;
            acc += gba::bit_cast(pos.x) + hp.hp;
        });
        sink_acc = acc;
    }

    template<int N>
    [[gnu::noinline]]
    void run_scenario() {
        static constexpr unsigned int vblank = 67000u;

        bench::with_logger([] {
            bench::log_printf(gba::log::level::info, "");
            bench::log_printf(gba::log::level::info, "--- N = %d ---", N);
            bench::log_printf(gba::log::level::info, "  %-30s  %7s  %4s  %6s", "operation", "cycles", "vbl%", "cyc/e");
        });

        gba::ecs::registry<128, position, velocity, health, sprite_id> reg;
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
        bench::log_printf(gba::log::level::info, "=== gba::ecs DEBUG (-O0) benchmark (cycles) ===");
        bench::log_printf(gba::log::level::info, "  Flat dense arrays, 0 heap alloc, compile-time component list");
        bench::log_printf(gba::log::level::info, "  VBlank budget: ~67000 cycles @ 16.78 MHz");
    });

    run_scenario<32>();
    run_scenario<64>();
    run_scenario<128>();

    bench::with_logger([] {
        bench::log_printf(gba::log::level::info, "");
        bench::log_printf(gba::log::level::info, "=== benchmark complete ===");
    });

    bench::exit(0);
}
