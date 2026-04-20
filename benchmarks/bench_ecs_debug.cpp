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

#include <gba/benchmark>

using namespace gba::literals;

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
    using cleanup = gba::ecs::group<health, sprite_id>;

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

        gba::benchmark::with_logger([] {
            gba::benchmark::log(gba::log::level::info, "");
            gba::benchmark::log(gba::log::level::info, "--- N = {n} ---"_fmt, "n"_arg = N);
            gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {vbl:>4}  {cyce:>6}"_fmt,
                       "operation"_arg = "operation", "cycles"_arg = "cycles", "vbl"_arg = "vbl%", "cyce"_arg = "cyc/e");
        });

        gba::ecs::registry<128, position, velocity, health, sprite_id> reg;
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

        // Remove path microbenchmark focused on debug-runtime overhead.
        {
            gba::ecs::registry<128, position, velocity, health, sprite_id> checked_reg;
            gba::ecs::registry<128, position, velocity, health, sprite_id> unchecked_reg;
            std::array<gba::entity, N> checked_entities{};
            std::array<gba::entity, N> unchecked_entities{};

            for (int i = 0; i < N; ++i) {
                const auto ce = checked_reg.create();
                checked_entities[i] = ce;
                checked_reg.emplace<health>(ce, 100);
                checked_reg.emplace<sprite_id>(ce, static_cast<std::uint8_t>(i & 0x7F));

                const auto ue = unchecked_reg.create();
                unchecked_entities[i] = ue;
                unchecked_reg.emplace<health>(ue, 100);
                unchecked_reg.emplace<sprite_id>(ue, static_cast<std::uint8_t>(i & 0x7F));
            }

            const auto checked_remove = gba::benchmark::measure([&] {
                for (int i = 0; i < N; ++i) {
                    checked_reg.remove<health>(checked_entities[i]);
                    checked_reg.remove<sprite_id>(checked_entities[i]);
                    checked_reg.emplace<health>(checked_entities[i], 100);
                    checked_reg.emplace<sprite_id>(checked_entities[i], static_cast<std::uint8_t>(i & 0x7F));
                }
            });

            const auto unchecked_remove = gba::benchmark::measure([&] {
                for (int i = 0; i < N; ++i) {
                    unchecked_reg.remove_unchecked<cleanup>(unchecked_entities[i]);
                    unchecked_reg.emplace<health>(unchecked_entities[i], 100);
                    unchecked_reg.emplace<sprite_id>(unchecked_entities[i], static_cast<std::uint8_t>(i & 0x7F));
                }
            });

            const int diff = static_cast<int>(checked_remove) - static_cast<int>(unchecked_remove);
            gba::benchmark::with_logger([&] {
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "remove x2 checked",
                           "cycles"_arg = checked_remove,
                           "pct"_arg = (checked_remove * 100u) / vblank,
                           "cycpe"_arg = checked_remove / static_cast<unsigned int>(N));
                gba::benchmark::log(gba::log::level::info, "  {operation:<30}  {cycles:>7}  {pct:>3}%  {cycpe:>6}"_fmt,
                           "operation"_arg = "remove_unchecked<cleanup>",
                           "cycles"_arg = unchecked_remove,
                           "pct"_arg = (unchecked_remove * 100u) / vblank,
                           "cycpe"_arg = unchecked_remove / static_cast<unsigned int>(N));
                gba::benchmark::log(gba::log::level::info, "  remove delta (checked - unchecked): {d}"_fmt,
                           "d"_arg = diff);
            });
        }
    }

} // namespace

int main() {
    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "=== gba::ecs DEBUG (-O0) benchmark (cycles) ===");
        gba::benchmark::log(gba::log::level::info, "  Flat dense arrays, 0 heap alloc, compile-time component list");
        gba::benchmark::log(gba::log::level::info, "  VBlank budget: ~67000 cycles @ 16.78 MHz");
    });

    run_scenario<32>();
    run_scenario<64>();
    run_scenario<128>();

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, "");
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ===");
    });

    gba::benchmark::exit(0);
}
