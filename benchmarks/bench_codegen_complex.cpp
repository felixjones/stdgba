#include <gba/benchmark>
#include <gba/codegen>

#include <cstring>

using namespace gba::codegen;
using namespace gba::literals;

namespace {

    static constexpr auto bench_row = "  {label:<52} {cycles:>7}"_fmt;
    static constexpr auto bench_header = "{title}"_fmt;

    volatile unsigned int sink_u32 = 0;

    constexpr int benchmark_iters = 128;
    constexpr int scanlines = 160;

    // Simulates per-scanline repatching of immediate fields in a tiny ISR-like block.
    static constexpr auto scanline_patch_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .orr_imm(arm_reg::r0, arm_reg::r0, imm_slot(1))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(2))
         .sub_imm(arm_reg::r0, arm_reg::r0, imm_slot(3))
         .eor_imm(arm_reg::r0, arm_reg::r0, imm_slot(4))
         .mov_imm(arm_reg::r1, imm_slot(5))
         .bx(arm_reg::lr);
    });

    // Simulates dynamic bitfield extraction kernel updates.
    static constexpr auto bitfield_patch_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r1, imm_slot(0))
         .lsr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .and_imm(arm_reg::r0, arm_reg::r0, imm_slot(1))
         .eor_imm(arm_reg::r0, arm_reg::r0, imm_slot(2))
         .bx(arm_reg::lr);
    });

    // Simulates threshold branch-heavy micro-kernel with patchable constants.
    static constexpr auto branch_threshold_block = arm_macro([](auto& b) {
        b.cmp_imm(arm_reg::r0, imm_slot(0));
        const auto hot_path = b.mark() + 3;
        b.b_if(arm_cond::hi, hot_path)
         .mov_imm(arm_reg::r0, imm_slot(1))
         .bx(arm_reg::lr)
         .mov_imm(arm_reg::r0, imm_slot(2))
         .bx(arm_reg::lr);
    });

    alignas(4) std::uint32_t code_scanline[16] = {};
    alignas(4) std::uint32_t code_bitfield[16] = {};
    alignas(4) std::uint32_t code_branch[16] = {};

    [[gnu::noinline]] void scanline_repatch_160() {
        unsigned int acc = 0;
        for (int y = 0; y < scanlines; ++y) {
            const auto y_u = static_cast<unsigned int>(y);
            apply_patches<void()>(scanline_patch_block, code_scanline, 16,
                                  y_u & 0xFFu,
                                  (y_u * 3u) & 0xFFu,
                                  (y_u + 7u) & 0xFFu,
                                  (y_u >> 1u) & 0xFFu,
                                  (y_u ^ 0x5Au) & 0xFFu,
                                  (y_u + 11u) & 0xFFu);
            acc ^= code_scanline[0];
        }
        sink_u32 = acc;
    }

    [[gnu::noinline]] void bitfield_repatch_and_execute_160() {
        unsigned int acc = 0;
        for (int i = 0; i < scanlines; ++i) {
            const auto i_u = static_cast<unsigned int>(i);
            auto fn = apply_patches<int(int)>(bitfield_patch_block, code_bitfield, 16,
                                              i_u & 7u,
                                              0x1Fu,
                                              (i_u * 9u) & 0xFFu);
            acc += static_cast<unsigned int>(fn(static_cast<int>((i_u << 5u) | (i_u & 31u))));
        }
        sink_u32 = acc;
    }

    [[gnu::noinline]] void branch_mostly_taken() {
        auto fn = apply_patches<int(int)>(branch_threshold_block, code_branch, 16, 32u, 1u, 2u);
        unsigned int acc = 0;
        for (int i = 0; i < scanlines; ++i) {
            acc += static_cast<unsigned int>(fn(128 + i));
        }
        sink_u32 = acc;
    }

    [[gnu::noinline]] void branch_mostly_not_taken() {
        auto fn = apply_patches<int(int)>(branch_threshold_block, code_branch, 16, 224u, 1u, 2u);
        unsigned int acc = 0;
        for (int i = 0; i < scanlines; ++i) {
            acc += static_cast<unsigned int>(fn(i));
        }
        sink_u32 = acc;
    }

} // namespace

int main() {
    std::memcpy(code_scanline, scanline_patch_block.data(), scanline_patch_block.size_bytes());
    std::memcpy(code_bitfield, bitfield_patch_block.data(), bitfield_patch_block.size_bytes());
    std::memcpy(code_branch, branch_threshold_block.data(), branch_threshold_block.size_bytes());

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, bench_header,
                            "title"_arg = "=== ARM Codegen Complex Benchmark ===");
        gba::benchmark::log(gba::log::level::info, bench_header,
                            "title"_arg = "Net cycles (gross minus timer overhead, ARM7TDMI @ 16.78 MHz)");
    });

    const auto overhead = gba::benchmark::default_overhead();
    gba::benchmark::with_logger([overhead] {
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "timer overhead (reference)", "cycles"_arg = overhead);
    });

    const auto c_scanline = gba::benchmark::measure_avg_net(benchmark_iters, scanline_repatch_160);
    const auto c_bitfield = gba::benchmark::measure_avg_net(benchmark_iters, bitfield_repatch_and_execute_160);
    const auto c_branch_hot = gba::benchmark::measure_avg_net(benchmark_iters, branch_mostly_taken);
    const auto c_branch_cold = gba::benchmark::measure_avg_net(benchmark_iters, branch_mostly_not_taken);

    gba::benchmark::with_logger([c_scanline, c_bitfield, c_branch_hot, c_branch_cold] {
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "scanline-like: repatch 160 rows (6 imm8 slots)", "cycles"_arg = c_scanline);
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "bitfield: repatch+execute 160 words", "cycles"_arg = c_bitfield);
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "branch workload: mostly taken", "cycles"_arg = c_branch_hot);
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "branch workload: mostly not-taken", "cycles"_arg = c_branch_cold);
    });

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, ""_fmt);
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ==="_fmt);
    });

    gba::benchmark::exit(0);
}
