#include <gba/codegen>

using namespace gba::codegen;

[[gnu::target("arm"), gnu::section(".iwram._bench_codegen_repatch_arm"), gnu::noinline]]
void bench_repatch_arm_apply_1patch(const compiled_block<2>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept {
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_a, std::size_t{1});
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_b, std::size_t{1});
}

[[gnu::target("arm"), gnu::section(".iwram._bench_codegen_repatch_arm"), gnu::noinline]]
void bench_repatch_arm_apply_2patch(const compiled_block<3>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept {
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_a, std::size_t{2});
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_b, std::size_t{2});
}

[[gnu::target("arm"), gnu::section(".iwram._bench_codegen_repatch_arm"), gnu::noinline]]
void bench_repatch_arm_apply_4patch(const compiled_block<5>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept {
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_a, std::size_t{4});
    apply_patches_packed<void()>(block, code, std::size_t{8}, args_b, std::size_t{4});
}

[[gnu::target("arm"), gnu::section(".iwram._bench_codegen_repatch_arm"), gnu::noinline]]
void bench_repatch_arm_apply_8patch(const compiled_block<9>& block,
                                    std::uint32_t* code,
                                    const std::uint32_t* args_a,
                                    const std::uint32_t* args_b) noexcept {
    apply_patches_packed<void()>(block, code, std::size_t{12}, args_a, std::size_t{8});
    apply_patches_packed<void()>(block, code, std::size_t{12}, args_b, std::size_t{8});
}

[[gnu::target("arm"), gnu::section(".iwram._bench_codegen_repatch_arm"), gnu::noinline]]
void bench_repatch_arm_direct_imm8(std::uint32_t* destination,
                                   const void* patches,
                                   std::uint32_t patch_count,
                                   const std::uint32_t* args_a,
                                   const std::uint32_t* args_b) noexcept {
    _stdgba_apply_patches_imm8(destination, patches, patch_count, args_a);
    _stdgba_apply_patches_imm8(destination, patches, patch_count, args_b);
}
