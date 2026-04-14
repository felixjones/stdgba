#include <gba/codegen>
#include <gba/benchmark>

#include <cstring>

using namespace gba::codegen;
using namespace gba::literals;

void bench_repatch_arm_apply_1patch(const compiled_block<2>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept;
void bench_repatch_arm_apply_2patch(const compiled_block<3>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept;
void bench_repatch_arm_apply_4patch(const compiled_block<5>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept;
void bench_repatch_arm_apply_8patch(const compiled_block<9>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept;

void bench_repatch_arm_direct_imm8(std::uint32_t* destination,
                                   const void* patches,
                                   std::uint32_t patch_count,
                                   const std::uint32_t* args_a,
                                   const std::uint32_t* args_b) noexcept;

namespace {

    static constexpr auto bench_row = "  {label:<44} {cycles:>7}"_fmt;
    static constexpr auto bench_header = "{title}"_fmt;

    volatile unsigned int sink_u32 = 0;
    volatile void* sink_ptr = nullptr;

    // Template with 1 immediate patch
    static constexpr auto single_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0)).bx(arm_reg::lr);
    });

    // Template with 2 immediate patches
    static constexpr auto double_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(1))
         .bx(arm_reg::lr);
    });

    // Template with 4 patches (mixed types)
    static constexpr auto quad_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .mov_imm(arm_reg::r1, imm_slot(1))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(2))
         .sub_imm(arm_reg::r1, arm_reg::r1, imm_slot(3))
         .bx(arm_reg::lr);
    });

    // Template with 8 immediate patches to force the assembly fallback path.
    static constexpr auto oct_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .mov_imm(arm_reg::r1, imm_slot(1))
         .mov_imm(arm_reg::r2, imm_slot(2))
         .mov_imm(arm_reg::r3, imm_slot(3))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(4))
         .add_imm(arm_reg::r1, arm_reg::r1, imm_slot(5))
         .sub_imm(arm_reg::r2, arm_reg::r2, imm_slot(6))
         .sub_imm(arm_reg::r3, arm_reg::r3, imm_slot(7))
         .bx(arm_reg::lr);
    });

    static constexpr auto oct_word_patch = arm_macro([](auto& b) {
        b.word(word_slot(0))
         .word(word_slot(1))
         .word(word_slot(2))
         .word(word_slot(3))
         .word(word_slot(4))
         .word(word_slot(5))
         .word(word_slot(6))
         .word(word_slot(7));
    });

    // Template with signed12 offset patches
    static constexpr auto offset_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .ldr_imm(arm_reg::r1, arm_reg::r2, s12_slot(1))
         .str_imm(arm_reg::r3, arm_reg::r4, s12_slot(2))
         .bx(arm_reg::lr);
    });

    // Template with branch patch
    static constexpr auto branch_patch = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .b_to(b_slot(1))
         .bx(arm_reg::lr);
    });

    constexpr int iters = 256;

    // Allocate aligned code buffers
    alignas(4) std::uint32_t code1[8] = {};
    alignas(4) std::uint32_t code2[8] = {};
    alignas(4) std::uint32_t code3[8] = {};
    alignas(4) std::uint32_t code4[8] = {};
    alignas(4) std::uint32_t code5[8] = {};
    alignas(4) std::uint32_t code6[12] = {};
    alignas(4) std::uint32_t code7[12] = {};

    alignas(4) std::uint32_t args2_imm8[2] = {42u, 10u};
    alignas(4) std::uint32_t args4_imm8[4] = {1u, 2u, 3u, 4u};
    alignas(4) std::uint32_t args8_imm8[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    alignas(4) std::uint32_t args8_word_a[8] = {0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u};
    alignas(4) std::uint32_t args8_word_b[8] = {0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u};
    alignas(4) std::uint32_t args1_repatch_a[1] = {25u};
    alignas(4) std::uint32_t args1_repatch_b[1] = {100u};
    alignas(4) std::uint32_t args2_repatch_a[2] = {25u, 17u};
    alignas(4) std::uint32_t args2_repatch_b[2] = {100u, 200u};
    alignas(4) std::uint32_t args4_repatch_a[4] = {1u, 2u, 3u, 4u};
    alignas(4) std::uint32_t args4_repatch_b[4] = {5u, 6u, 7u, 8u};
    alignas(4) std::uint32_t args8_repatch_a[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    alignas(4) std::uint32_t args8_repatch_b[8] = {9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
    alignas(4) std::uint32_t args_mixed[3] = {42u, 100u, static_cast<std::uint32_t>(-50)};
    alignas(4) std::uint32_t args_branch[2] = {42u, static_cast<std::uint32_t>(-2)};

    // Copy/install baseline (user-driven memcpy)
    [[gnu::noinline]]
    void copy_install_baseline() {
        std::memcpy(code1, single_patch.data(), single_patch.size_bytes());
        sink_u32 = code1[0];
    }

    // Stage 2: Apply 1 patch
    [[gnu::noinline]]
    void stage2_apply_1patch() {
        apply_patches<void()>(single_patch, code1, 8, 42u);
        sink_u32 = code1[0];
    }

    // Stage 2: Apply 2 patches
    [[gnu::noinline]]
    void stage2_apply_2patch() {
        apply_patches<void()>(double_patch, code2, 8, 42u, 10u);
        sink_u32 = code2[1];
    }

    // Stage 2: Apply 4 patches
    [[gnu::noinline]]
    void stage2_apply_4patch() {
        apply_patches<void()>(quad_patch, code3, 8, 1u, 2u, 3u, 4u);
        sink_u32 = code3[3];
    }

    // Stage 2: Apply 8 patches (assembly fallback path)
    [[gnu::noinline]]
    void stage2_apply_8patch() {
        apply_patches<void()>(oct_patch, code6, 12, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);
        sink_u32 = code6[7];
    }

    [[gnu::noinline]]
    void stage2_apply_wordpatch_8() {
        apply_word_patches(oct_word_patch, code7, 12, [](const std::size_t patch_index) {
            return 0x10u + static_cast<std::uint32_t>(patch_index);
        });
        sink_u32 = code7[7];
    }

    // Stage 2: Apply mixed patch types
    [[gnu::noinline]]
    void stage2_apply_mixed() {
        apply_patches<void()>(offset_patch, code4, 8, 42u, 100, -50);
        sink_u32 = code4[2];
    }

    // Stage 2: Apply branch offset patch
    [[gnu::noinline]]
    void stage2_apply_branch() {
        apply_patches<void()>(branch_patch, code5, 8, 42u, -2);
        sink_u32 = code5[1];
    }

    // Stage 2: direct low-level IWRAM loops for wrapper overhead comparison
    [[gnu::noinline]]
    void stage2_direct_imm8_2patch() {
        _stdgba_apply_patches_imm8(code2, double_patch.patches.data(),
                                   static_cast<std::uint32_t>(double_patch.patch_count), args2_imm8);
        sink_u32 = code2[1];
    }

    [[gnu::noinline]]
    void stage2_direct_imm8_4patch() {
        _stdgba_apply_patches_imm8(code3, quad_patch.patches.data(),
                                   static_cast<std::uint32_t>(quad_patch.patch_count), args4_imm8);
        sink_u32 = code3[3];
    }

    [[gnu::noinline]]
    void stage2_direct_imm8_8patch() {
        _stdgba_apply_patches_imm8(code6, oct_patch.patches.data(),
                                   static_cast<std::uint32_t>(oct_patch.patch_count), args8_imm8);
        sink_u32 = code6[7];
    }

    [[gnu::noinline]]
    void stage2_direct_core_mixed() {
        _stdgba_apply_patches_core(code4, offset_patch.patches.data(),
                                   static_cast<std::uint32_t>(offset_patch.patch_count), args_mixed);
        sink_u32 = code4[2];
    }

    [[gnu::noinline]]
    void stage2_direct_core_branch() {
        _stdgba_apply_patches_core(code5, branch_patch.patches.data(),
                                   static_cast<std::uint32_t>(branch_patch.patch_count), args_branch);
        sink_u32 = code5[1];
    }

    // Combined: memcpy + patch (1 patch)
    [[gnu::noinline]]
    void combined_1patch() {
        std::memcpy(code1, single_patch.data(), single_patch.size_bytes());
        apply_patches<void()>(single_patch, code1, 8, 42u);
        sink_u32 = code1[0];
    }

    // Combined: memcpy + patch (2 patches)
    [[gnu::noinline]]
    void combined_2patch() {
        std::memcpy(code2, double_patch.data(), double_patch.size_bytes());
        apply_patches<void()>(double_patch, code2, 8, 42u, 10u);
        sink_u32 = code2[1];
    }

    // Combined: memcpy + patch (4 patches)
    [[gnu::noinline]]
    void combined_4patch() {
        std::memcpy(code3, quad_patch.data(), quad_patch.size_bytes());
        apply_patches<void()>(quad_patch, code3, 8, 1u, 2u, 3u, 4u);
        sink_u32 = code3[3];
    }

    [[gnu::noinline]]
    void combined_8patch() {
        std::memcpy(code6, oct_patch.data(), oct_patch.size_bytes());
        apply_patches<void()>(oct_patch, code6, 12, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);
        sink_u32 = code6[7];
    }

    [[gnu::noinline]]
    void repatch_wordpatch_8() {
        apply_word_patches(oct_word_patch, code7, 12, [](const std::size_t patch_index) {
            return 0x10u + static_cast<std::uint32_t>(patch_index);
        });
        apply_word_patches(oct_word_patch, code7, 12, [](const std::size_t patch_index) {
            return 0x20u + static_cast<std::uint32_t>(patch_index);
        });
        sink_u32 = code7[7];
    }

    [[gnu::noinline]]
    void repatch_wordpatch_8_packed() {
        apply_patches_packed<void()>(oct_word_patch, code7, 12, &args8_word_a[0], static_cast<std::size_t>(8));
        apply_patches_packed<void()>(oct_word_patch, code7, 12, &args8_word_b[0], static_cast<std::size_t>(8));
        sink_u32 = code7[7];
    }

    // Re-patching scenario
    [[gnu::noinline]]
    void repatch_1patch() {
        apply_patches<void()>(single_patch, code1, 8, 25u);
        apply_patches<void()>(single_patch, code1, 8, 100u);
        sink_u32 = code1[0];
    }

    [[gnu::noinline]]
    void repatch_2patch() {
        apply_patches<void()>(double_patch, code2, 8, 25u, 17u);
        apply_patches<void()>(double_patch, code2, 8, 100u, 200u);
        sink_u32 = code2[1];
    }

    [[gnu::noinline]]
    void repatch_4patch() {
        apply_patches<void()>(quad_patch, code3, 8, 1u, 2u, 3u, 4u);
        apply_patches<void()>(quad_patch, code3, 8, 5u, 6u, 7u, 8u);
        sink_u32 = code3[3];
    }

    [[gnu::noinline]]
    void repatch_8patch() {
        apply_patches<void()>(oct_patch, code6, 12, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);
        apply_patches<void()>(oct_patch, code6, 12, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u);
        sink_u32 = code6[7];
    }

    void repatch_arm_apply_1patch() {
        bench_repatch_arm_apply_1patch(single_patch, code1, args1_repatch_a, args1_repatch_b);
        sink_u32 = code1[0];
    }

    void repatch_arm_apply_2patch() {
        bench_repatch_arm_apply_2patch(double_patch, code2, args2_repatch_a, args2_repatch_b);
        sink_u32 = code2[1];
    }

    void repatch_arm_apply_4patch() {
        bench_repatch_arm_apply_4patch(quad_patch, code3, args4_repatch_a, args4_repatch_b);
        sink_u32 = code3[3];
    }

    void repatch_arm_apply_8patch() {
        bench_repatch_arm_apply_8patch(oct_patch, code6, args8_repatch_a, args8_repatch_b);
        sink_u32 = code6[7];
    }

    void repatch_arm_direct_1patch() {
        bench_repatch_arm_direct_imm8(code1, single_patch.patches.data(),
                                      static_cast<std::uint32_t>(single_patch.patch_count),
                                      args1_repatch_a, args1_repatch_b);
        sink_u32 = code1[0];
    }

    void repatch_arm_direct_2patch() {
        bench_repatch_arm_direct_imm8(code2, double_patch.patches.data(),
                                      static_cast<std::uint32_t>(double_patch.patch_count),
                                      args2_repatch_a, args2_repatch_b);
        sink_u32 = code2[1];
    }

    void repatch_arm_direct_4patch() {
        bench_repatch_arm_direct_imm8(code3, quad_patch.patches.data(),
                                      static_cast<std::uint32_t>(quad_patch.patch_count),
                                      args4_repatch_a, args4_repatch_b);
        sink_u32 = code3[3];
    }

    void repatch_arm_direct_8patch() {
        bench_repatch_arm_direct_imm8(code6, oct_patch.patches.data(),
                                      static_cast<std::uint32_t>(oct_patch.patch_count),
                                      args8_repatch_a, args8_repatch_b);
        sink_u32 = code6[7];
    }

    // Typed function-pointer return from apply_patches
    [[gnu::noinline]]
    void typed_return_apply_patches() {
        auto fn = apply_patches<void()>(single_patch, code1, 8, 42u);
        sink_ptr = reinterpret_cast<void*>(fn);
    }

}

int main() {
    // Pre-copy for stage 2 tests (user drives the copy with memcpy)
    std::memcpy(code1, single_patch.data(), single_patch.size_bytes());
    std::memcpy(code2, double_patch.data(), double_patch.size_bytes());
    std::memcpy(code3, quad_patch.data(),   quad_patch.size_bytes());
    std::memcpy(code4, offset_patch.data(), offset_patch.size_bytes());
    std::memcpy(code5, branch_patch.data(), branch_patch.size_bytes());
    std::memcpy(code6, oct_patch.data(),    oct_patch.size_bytes());
    std::memcpy(code7, oct_word_patch.data(), oct_word_patch.size_bytes());

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, bench_header, "title"_arg = "=== ARM Codegen Benchmark ===");
        gba::benchmark::log(gba::log::level::info, bench_header,
                            "title"_arg = "Net cycles (gross minus timer overhead, ARM7TDMI @ 16.78 MHz)");
    });

    const auto overhead = gba::benchmark::default_overhead();
    gba::benchmark::with_logger([overhead] {
        gba::benchmark::log(gba::log::level::info, bench_row,
                            "label"_arg = "timer overhead (reference)", "cycles"_arg = overhead);
    });

    // Copy/install baseline
    {
        const auto cycles = gba::benchmark::measure_avg_net(iters, copy_install_baseline);
        gba::benchmark::with_logger([cycles] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "copy/install: memcpy baseline", "cycles"_arg = cycles);
        });
    }

    // Stage 2: Apply patches
    {
        const auto c1 = gba::benchmark::measure_avg_net(iters, stage2_apply_1patch);
        const auto c2 = gba::benchmark::measure_avg_net(iters, stage2_apply_2patch);
        const auto c4 = gba::benchmark::measure_avg_net(iters, stage2_apply_4patch);
        const auto c8 = gba::benchmark::measure_avg_net(iters, stage2_apply_8patch);
        const auto c8_words = gba::benchmark::measure_avg_net(iters, stage2_apply_wordpatch_8);
        gba::benchmark::with_logger([c1, c2, c4, c8, c8_words] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (1 imm8)", "cycles"_arg = c1);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (2 imm8)", "cycles"_arg = c2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (4 imm8)", "cycles"_arg = c4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (8 imm8)", "cycles"_arg = c8);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_word_patches (8 word32)", "cycles"_arg = c8_words);
        });
    }

    // Stage 2: Mixed patch types
    {
        const auto mixed = gba::benchmark::measure_avg_net(iters, stage2_apply_mixed);
        const auto branch = gba::benchmark::measure_avg_net(iters, stage2_apply_branch);
        gba::benchmark::with_logger([mixed, branch] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (1 imm8 + 2 s12)", "cycles"_arg = mixed);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: apply_patches (1 imm8 + 1 branch)", "cycles"_arg = branch);
        });
    }

    // Stage 2: direct low-level loop costs
    {
        const auto imm8_2 = gba::benchmark::measure_avg_net(iters, stage2_direct_imm8_2patch);
        const auto imm8_4 = gba::benchmark::measure_avg_net(iters, stage2_direct_imm8_4patch);
        const auto imm8_8 = gba::benchmark::measure_avg_net(iters, stage2_direct_imm8_8patch);
        const auto core_mixed = gba::benchmark::measure_avg_net(iters, stage2_direct_core_mixed);
        const auto core_branch = gba::benchmark::measure_avg_net(iters, stage2_direct_core_branch);
        gba::benchmark::with_logger([imm8_2, imm8_4, imm8_8, core_mixed, core_branch] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: _stdgba_apply_patches_imm8 (2)", "cycles"_arg = imm8_2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: _stdgba_apply_patches_imm8 (4)", "cycles"_arg = imm8_4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: _stdgba_apply_patches_imm8 (8)", "cycles"_arg = imm8_8);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: _stdgba_apply_patches_core (mixed)", "cycles"_arg = core_mixed);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "stage 2: _stdgba_apply_patches_core (branch)", "cycles"_arg = core_branch);
        });
    }

    // Copy + patch combined
    {
        const auto c1 = gba::benchmark::measure_avg_net(iters, combined_1patch);
        const auto c2 = gba::benchmark::measure_avg_net(iters, combined_2patch);
        const auto c4 = gba::benchmark::measure_avg_net(iters, combined_4patch);
        const auto c8 = gba::benchmark::measure_avg_net(iters, combined_8patch);
        gba::benchmark::with_logger([c1, c2, c4, c8] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "copy + patch: memcpy + apply_patches (1)", "cycles"_arg = c1);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "copy + patch: memcpy + apply_patches (2)", "cycles"_arg = c2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "copy + patch: memcpy + apply_patches (4)", "cycles"_arg = c4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "copy + patch: memcpy + apply_patches (8)", "cycles"_arg = c8);
        });
    }

    // Re-patching
    {
        const auto r1 = gba::benchmark::measure_avg_net(iters / 2, repatch_1patch);
        const auto r2 = gba::benchmark::measure_avg_net(iters / 2, repatch_2patch);
        const auto r4 = gba::benchmark::measure_avg_net(iters / 2, repatch_4patch);
        const auto r8 = gba::benchmark::measure_avg_net(iters / 2, repatch_8patch);
        const auto r8_words = gba::benchmark::measure_avg_net(iters / 2, repatch_wordpatch_8);
        const auto r8_words_packed = gba::benchmark::measure_avg_net(iters / 2, repatch_wordpatch_8_packed);
        gba::benchmark::with_logger([r1, r2, r4, r8, r8_words, r8_words_packed] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: apply_patches twice (1 patch)", "cycles"_arg = r1);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: apply_patches twice (2 patches)", "cycles"_arg = r2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: apply_patches twice (4 patches)", "cycles"_arg = r4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: apply_patches twice (8 patches)", "cycles"_arg = r8);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: apply_word_patches twice (8)", "cycles"_arg = r8_words);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch: packed word patches twice (8)", "cycles"_arg = r8_words_packed);
        });
    }

    // Re-patching from ARM-side callers in IWRAM
    {
        const auto r1 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_apply_1patch);
        const auto r2 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_apply_2patch);
        const auto r4 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_apply_4patch);
        const auto r8 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_apply_8patch);
        gba::benchmark::with_logger([r1, r2, r4, r8] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: apply_patches twice (1)", "cycles"_arg = r1);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: apply_patches twice (2)", "cycles"_arg = r2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: apply_patches twice (4)", "cycles"_arg = r4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: apply_patches twice (8)", "cycles"_arg = r8);
        });
    }

    {
        const auto r1 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_direct_1patch);
        const auto r2 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_direct_2patch);
        const auto r4 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_direct_4patch);
        const auto r8 = gba::benchmark::measure_avg_net(iters / 2, repatch_arm_direct_8patch);
        gba::benchmark::with_logger([r1, r2, r4, r8] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: _apply_patches_imm8 x2 (1)", "cycles"_arg = r1);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: _apply_patches_imm8 x2 (2)", "cycles"_arg = r2);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: _apply_patches_imm8 x2 (4)", "cycles"_arg = r4);
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "re-patch ARM: _apply_patches_imm8 x2 (8)", "cycles"_arg = r8);
        });
    }

    // Typed function-pointer return
    {
        const auto apply = gba::benchmark::measure_avg_net(iters, typed_return_apply_patches);
        gba::benchmark::with_logger([apply] {
            gba::benchmark::log(gba::log::level::info, bench_row,
                                "label"_arg = "typed return: apply_patches<void()>", "cycles"_arg = apply);
        });
    }

    gba::benchmark::with_logger([] {
        gba::benchmark::log(gba::log::level::info, ""_fmt);
        gba::benchmark::log(gba::log::level::info, "=== benchmark complete ==="_fmt);
    });

    gba::benchmark::exit(0);

    return 0;
}
