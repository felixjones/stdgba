#include <gba/codegen>
#include <gba/testing>

#include <cstring>

namespace {

    using namespace gba::codegen;
    using namespace gba::literals;

    static constexpr auto block = [] {
        auto b = arm_macro_builder<4>{};
        b.mov_imm(arm_reg::r0, 0x2A).bx(arm_reg::lr);
        return b.compile();
    }();

    static_assert(block.count == 2);
    static_assert(block.size() == 2);
    static_assert(block.size_bytes() == 8);
    static_assert(static_cast<std::uint32_t>(block[0]) == 0xE3A0002Au);
    static_assert(static_cast<std::uint32_t>(block[1]) == 0xE12FFF1Eu);

    static constexpr auto branch_block = [] {
        auto b = arm_macro_builder<3>{};
        const auto top = b.mark();
        b.b_to(top).bx(arm_reg::lr);
        return b.compile();
    }();

    static_assert(branch_block.count == 2);
    static_assert(static_cast<std::uint32_t>(branch_block[0]) == 0xEAFFFFFEu);

    static constexpr auto inferred_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, 7).bx(arm_reg::lr);
    });

    static_assert(inferred_block.count == 2);
    static_assert(static_cast<std::uint32_t>(inferred_block[0]) == 0xE3A00007u);

    static constexpr auto patched_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .add_imm(arm_reg::r1, arm_reg::r1, imm_slot(1))
         .bx(arm_reg::lr);
    });

    static constexpr auto named_reuse_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, "x"_arg)
         .add_imm(arm_reg::r0, arm_reg::r0, "x"_arg)
         .bx(arm_reg::lr);
    });

    static_assert(named_reuse_block.count == 3);
    static_assert(named_reuse_block.patch_count == 2);
    static_assert(named_reuse_block.arg_count == 1);

    static_assert(patched_block.count == 3);
    static_assert(patched_block.patch_count == 2);
    static_assert(patched_block.arg_count == 2);
    static_assert(static_cast<std::uint32_t>(patched_block[0]) == 0xE3A00000u);
    static_assert(static_cast<std::uint32_t>(patched_block[1]) == 0xE2811000u);

    static constexpr auto offset_patched_block = arm_macro([](auto& b) {
        b.ldr_imm(arm_reg::r2, arm_reg::r3, s12_slot(0))
         .str_imm(arm_reg::r4, arm_reg::r5, s12_slot(1))
         .bx(arm_reg::lr);
    });

    static_assert(offset_patched_block.count == 3);
    static_assert(offset_patched_block.patch_count == 2);
    static_assert(offset_patched_block.arg_count == 2);
    static_assert(static_cast<std::uint32_t>(offset_patched_block[0]) == 0xE5932000u);
    static_assert(static_cast<std::uint32_t>(offset_patched_block[1]) == 0xE5854000u);

    static constexpr auto branch_patched_block = arm_macro([](auto& b) {
        b.b_to(b_slot(0)).bx(arm_reg::lr);
    });

    static_assert(branch_patched_block.count == 2);
    static_assert(branch_patched_block.patch_count == 1);
    static_assert(branch_patched_block.arg_count == 1);
    static_assert(static_cast<std::uint32_t>(branch_patched_block[0]) == 0xEA000000u);

    // Execution test: simple return immediate
    static constexpr auto return_imm_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0)).bx(arm_reg::lr);
    });

    static_assert(return_imm_block.count == 2);
    static_assert(return_imm_block.patch_count == 1);

    // Execution test: return sum of two immediates
    static constexpr auto return_sum_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(1))
         .bx(arm_reg::lr);
    });

    static_assert(return_sum_block.count == 3);
    static_assert(return_sum_block.patch_count == 2);

    static constexpr auto repatch_quad_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .mov_imm(arm_reg::r1, imm_slot(1))
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(2))
         .sub_imm(arm_reg::r1, arm_reg::r1, imm_slot(3))
         .bx(arm_reg::lr);
    });

    static constexpr auto repatch_oct_block = arm_macro([](auto& b) {
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

    static constexpr auto runtime_arg_identity_block = arm_macro([](auto& b) {
        b.bx(arm_reg::lr);
    });

    static_assert(runtime_arg_identity_block.count == 1);

    static constexpr auto runtime_arg_add_block = arm_macro([](auto& b) {
        b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    static_assert(runtime_arg_add_block.count == 2);
    static_assert(static_cast<std::uint32_t>(runtime_arg_add_block[0]) == 0xE0800001u);

    static constexpr auto runtime_arg_sub_block = arm_macro([](auto& b) {
        b.sub_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    static_assert(runtime_arg_sub_block.count == 2);
    static_assert(static_cast<std::uint32_t>(runtime_arg_sub_block[0]) == 0xE0400001u);

    static constexpr auto runtime_arg_scale_add_block = arm_macro([](auto& b) {
        b.add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0)
         .add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r0)
         .add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0))
         .bx(arm_reg::lr);
    });

    static_assert(runtime_arg_scale_add_block.count == 4);
    static_assert(runtime_arg_scale_add_block.patch_count == 1);

    static_assert(static_cast<std::uint32_t>(mov_reg(arm_reg::r2, arm_reg::r0)) == 0xE1A02000u);

    static_assert(static_cast<std::uint32_t>(nop_instr()) == 0xE1A00000u);
    static_assert(static_cast<std::uint32_t>(add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE0800001u);
    static_assert(static_cast<std::uint32_t>(sub_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE0400001u);

    static constexpr auto instruction_patched_block = arm_macro([](auto& b) {
        b.mov_imm(arm_reg::r0, imm_slot(0))
         .mov_imm(arm_reg::r1, imm_slot(1))
         .instruction(instr_slot(2))
         .bx(arm_reg::lr);
    });

    static constexpr auto word_patched_block = arm_macro([](auto& b) {
        b.word(word_slot(0))
         .literal_word(literal_slot(1))
         .word(word_slot(2))
         .literal_word(literal_slot(3));
    });

    static_assert(instruction_patched_block.count == 4);
    static_assert(instruction_patched_block.patch_count == 3);
    static_assert(instruction_patched_block.arg_count == 3);
    static_assert(static_cast<std::uint32_t>(instruction_patched_block[2]) == 0u);

    static_assert(word_patched_block.count == 4);
    static_assert(word_patched_block.patch_count == 4);
    static_assert(word_patched_block.arg_count == 4);
    static_assert(word_patched_block.all_patches_word32());
    static_assert(word_patched_block.patch_word_index(0) == 0);
    static_assert(word_patched_block.patch_arg_index(3) == 3);
    static_assert(word_patched_block.patch_type(1) == patch_kind::instruction);

    // Section: Extended encoding static_asserts

    // Shift/rotate
    static_assert(static_cast<std::uint32_t>(lsl_imm(arm_reg::r0, arm_reg::r0, 1)) == 0xE1A00080u);
    static_assert(static_cast<std::uint32_t>(lsr_imm(arm_reg::r0, arm_reg::r0, 1)) == 0xE1A000A0u);
    static_assert(static_cast<std::uint32_t>(asr_imm(arm_reg::r0, arm_reg::r0, 1)) == 0xE1A000C0u);
    static_assert(static_cast<std::uint32_t>(ror_imm(arm_reg::r0, arm_reg::r0, 1)) == 0xE1A000E0u);
    static_assert(static_cast<std::uint32_t>(lsl_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE1A00110u);
    static_assert(static_cast<std::uint32_t>(lsr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE1A00130u);
    static_assert(static_cast<std::uint32_t>(asr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE1A00150u);
    static_assert(static_cast<std::uint32_t>(ror_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE1A00170u);

    // Bitwise
    static_assert(static_cast<std::uint32_t>(orr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE1800001u);
    static_assert(static_cast<std::uint32_t>(orr_imm(arm_reg::r0, arm_reg::r1, 0xFFu)) == 0xE38100FFu);
    static_assert(static_cast<std::uint32_t>(and_imm(arm_reg::r0, arm_reg::r1, 0x0Fu)) == 0xE201000Fu);
    static_assert(static_cast<std::uint32_t>(and_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE0010002u);
    static_assert(static_cast<std::uint32_t>(eor_imm(arm_reg::r0, arm_reg::r1, 0x01u)) == 0xE2210001u);
    static_assert(static_cast<std::uint32_t>(eor_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE0210002u);
    static_assert(static_cast<std::uint32_t>(bic_imm(arm_reg::r0, arm_reg::r1, 0x0Fu)) == 0xE3C1000Fu);
    static_assert(static_cast<std::uint32_t>(bic_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE1C10002u);
    static_assert(static_cast<std::uint32_t>(mvn_imm(arm_reg::r0, 0u)) == 0xE3E00000u);
    static_assert(static_cast<std::uint32_t>(mvn_reg(arm_reg::r0, arm_reg::r1)) == 0xE1E00001u);

    // Arithmetic extras
    static_assert(static_cast<std::uint32_t>(rsb_imm(arm_reg::r0, arm_reg::r1, 0u)) == 0xE2610000u);
    static_assert(static_cast<std::uint32_t>(rsb_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE0610002u);
    static_assert(static_cast<std::uint32_t>(adc_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE0A00001u);
    static_assert(static_cast<std::uint32_t>(sbc_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)) == 0xE0C00001u);

    // Comparison
    static_assert(static_cast<std::uint32_t>(cmp_imm(arm_reg::r0, 0)) == 0xE3500000u);
    static_assert(static_cast<std::uint32_t>(cmp_reg(arm_reg::r0, arm_reg::r1)) == 0xE1500001u);
    static_assert(static_cast<std::uint32_t>(cmn_imm(arm_reg::r0, 0u)) == 0xE3700000u);
    static_assert(static_cast<std::uint32_t>(cmn_reg(arm_reg::r0, arm_reg::r1)) == 0xE1700001u);
    static_assert(static_cast<std::uint32_t>(tst_imm(arm_reg::r0, 0xFFu)) == 0xE31000FFu);
    static_assert(static_cast<std::uint32_t>(tst_reg(arm_reg::r0, arm_reg::r1)) == 0xE1100001u);
    static_assert(static_cast<std::uint32_t>(teq_imm(arm_reg::r0, 0u)) == 0xE3300000u);
    static_assert(static_cast<std::uint32_t>(teq_reg(arm_reg::r0, arm_reg::r1)) == 0xE1300001u);

    // Halfword load/store
    static_assert(static_cast<std::uint32_t>(ldrh_imm(arm_reg::r0, arm_reg::r1, 0)) == 0xE1D100B0u);
    static_assert(static_cast<std::uint32_t>(strh_imm(arm_reg::r1, arm_reg::r2, 0)) == 0xE1C210B0u);
    static_assert(static_cast<std::uint32_t>(ldrh_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE19100B2u);
    static_assert(static_cast<std::uint32_t>(strh_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE18100B2u);
    static_assert(static_cast<std::uint32_t>(ldrsb_imm(arm_reg::r0, arm_reg::r1, 0)) == 0xE1D100D0u);
    static_assert(static_cast<std::uint32_t>(ldrsh_imm(arm_reg::r0, arm_reg::r1, 0)) == 0xE1D100F0u);
    static_assert(static_cast<std::uint32_t>(ldrsb_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE19100D2u);
    static_assert(static_cast<std::uint32_t>(ldrsh_reg(arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE19100F2u);

    // LDM/STM
    static_assert(static_cast<std::uint32_t>(ldmia(arm_reg::r2, reg_list(arm_reg::r0, arm_reg::r1))) == 0xE8920003u);
    static_assert(static_cast<std::uint32_t>(stmia(arm_reg::r3, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE8830006u);
    static_assert(static_cast<std::uint32_t>(ldmib(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE9900006u);
    static_assert(static_cast<std::uint32_t>(ldmda(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE8100006u);
    static_assert(static_cast<std::uint32_t>(ldmdb(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE9100006u);
    static_assert(static_cast<std::uint32_t>(stmib(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE9800006u);
    static_assert(static_cast<std::uint32_t>(stmda(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE8000006u);
    static_assert(static_cast<std::uint32_t>(stmdb(arm_reg::r0, reg_list(arm_reg::r1, arm_reg::r2))) == 0xE9000006u);

    // Push / pop
    static_assert(static_cast<std::uint32_t>(push(reg_list(arm_reg::r4, arm_reg::r5))) == 0xE92D0030u);
    static_assert(static_cast<std::uint32_t>(pop(reg_list(arm_reg::r4, arm_reg::r5))) == 0xE8BD0030u);
    // pop {pc} is the canonical ARM function return (equivalent to ldmia sp!, {pc})
    static_assert(static_cast<std::uint32_t>(pop(reg_list(arm_reg::pc))) == 0xE8BD8000u);

    // Multiply
    static_assert(static_cast<std::uint32_t>(mul(arm_reg::r2, arm_reg::r0, arm_reg::r1)) == 0xE0020190u);
    // mla(r3, r0, r1, r2): Rd=r3, Rm=r0, Rs=r1, Rn=r2 => r3 = r0*r1 + r2
    static_assert(static_cast<std::uint32_t>(mla(arm_reg::r3, arm_reg::r0, arm_reg::r1, arm_reg::r2)) == 0xE0232190u);

    // Branch with link
    static_assert(static_cast<std::uint32_t>(blx(arm_reg::lr)) == 0xE12FFF3Eu);

    static constexpr auto runtime_arg_shift_orr_block = arm_macro([](auto& b) {
        b.lsl_imm(arm_reg::r0, arm_reg::r0, 1)
         .orr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    static_assert(runtime_arg_shift_orr_block.count == 3);

    // New execution blocks

    // AND: r0 = r0 & r1
    static constexpr auto and_reg_block = arm_macro([](auto& b) {
        b.and_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // ORR: r0 = r0 | r1
    static constexpr auto orr_reg_block = arm_macro([](auto& b) {
        b.orr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // EOR: r0 = r0 ^ r1
    static constexpr auto eor_reg_block = arm_macro([](auto& b) {
        b.eor_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // BIC: r0 = r0 & ~r1
    static constexpr auto bic_reg_block = arm_macro([](auto& b) {
        b.bic_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // MVN: r0 = ~r1
    static constexpr auto mvn_reg_block = arm_macro([](auto& b) {
        b.mvn_reg(arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // MUL: r0 = r1 * r0  (rd=r0, rm=r1, rs=r0; r0!=r1)
    static constexpr auto mul_block = arm_macro([](auto& b) {
        b.mul(arm_reg::r0, arm_reg::r1, arm_reg::r0)
         .bx(arm_reg::lr);
    });

    static_assert(mul_block.count == 2);
    static_assert(static_cast<std::uint32_t>(mul_block[0]) == 0xE0000091u);

    // ROR imm: r0 = r0 ROR #1
    static constexpr auto ror_imm_block = arm_macro([](auto& b) {
        b.ror_imm(arm_reg::r0, arm_reg::r0, 1)
         .bx(arm_reg::lr);
    });

    // LSR by register: r0 = r0 >> r1
    static constexpr auto lsr_reg_block = arm_macro([](auto& b) {
        b.lsr_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // RSB: r0 = 100 - r0
    static constexpr auto rsb_block = arm_macro([](auto& b) {
        b.rsb_imm(arm_reg::r0, arm_reg::r0, 100)
         .bx(arm_reg::lr);
    });

    // TST + conditional: return 1 if r0 has bit 0 set, 2 otherwise
    static constexpr auto tst_branch_block = arm_macro([](auto& b) {
        b.tst_imm(arm_reg::r0, 1);
        const auto true_case = b.mark() + 3;
        b.b_if(arm_cond::ne, true_case)  // branch if bit set
         .mov_imm(arm_reg::r0, 2)
         .bx(arm_reg::lr)
         .mov_imm(arm_reg::r0, 1)
         .bx(arm_reg::lr);
    });

    static_assert(tst_branch_block.count == 6);

    // PUSH/POP: save r4, compute r0 = r0 + 10 (using saved r4), restore r4
    // Uses bx lr for return (not pop {pc}) to stay compatible with the ARM/Thumb ABI
    static constexpr auto push_pop_block = arm_macro([](auto& b) {
        b.push(reg_list(arm_reg::r4))
         .mov_reg(arm_reg::r4, arm_reg::r0)
         .add_imm(arm_reg::r0, arm_reg::r4, 10)
         .pop(reg_list(arm_reg::r4))
         .bx(arm_reg::lr);
    });

    static_assert(push_pop_block.count == 5);
    static_assert(static_cast<std::uint32_t>(push_pop_block[0]) == 0xE92D0010u);  // push {r4}
    static_assert(static_cast<std::uint32_t>(push_pop_block[4]) == 0xE12FFF1Eu);  // bx lr

    // LDRSB: load signed byte and sign-extend
    static constexpr auto ldrsb_block = arm_macro([](auto& b) {
        b.mov_reg(arm_reg::r2, arm_reg::r0)
         .ldrsb_imm(arm_reg::r0, arm_reg::r2, 0)
         .bx(arm_reg::lr);
    });

    // LDRH with register offset: r0 = halfword at [r0 + r1]
    static constexpr auto ldrh_reg_block = arm_macro([](auto& b) {
        b.ldrh_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // STRH with register offset: (uint16_t*)(r0 + r1) = r2
    static constexpr auto strh_reg_block = arm_macro([](auto& b) {
        b.strh_reg(arm_reg::r2, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    // ORR_IMM: r0 = r0 | 0x80
    static constexpr auto orr_imm_block = arm_macro([](auto& b) {
        b.orr_imm(arm_reg::r0, arm_reg::r0, 0x80)
         .bx(arm_reg::lr);
    });

    // AND_IMM: r0 = r0 & 0x0F
    static constexpr auto and_imm_block = arm_macro([](auto& b) {
        b.and_imm(arm_reg::r0, arm_reg::r0, 0x0F)
         .bx(arm_reg::lr);
    });

    // EOR_IMM: r0 = r0 ^ 0xFF
    static constexpr auto eor_imm_block = arm_macro([](auto& b) {
        b.eor_imm(arm_reg::r0, arm_reg::r0, 0xFF)
         .bx(arm_reg::lr);
    });

    static constexpr auto conditional_select_block = arm_macro([](auto& b) {
        b.cmp_imm(arm_reg::r0, 0);
        const auto true_case = b.mark() + 3;
        b.b_if(arm_cond::eq, true_case)
         .mov_imm(arm_reg::r0, 2)
         .bx(arm_reg::lr)
         .mov_imm(arm_reg::r0, 1)
         .bx(arm_reg::lr);
    });

    static_assert(conditional_select_block.count == 6);

    static constexpr auto ldrh_block = arm_macro([](auto& b) {
        b.mov_reg(arm_reg::r2, arm_reg::r0)
         .ldrh_imm(arm_reg::r0, arm_reg::r2, 0)
         .bx(arm_reg::lr);
    });

    static constexpr auto strh_block = arm_macro([](auto& b) {
        b.mov_reg(arm_reg::r2, arm_reg::r0)
         .strh_imm(arm_reg::r1, arm_reg::r2, 0)
         .bx(arm_reg::lr);
    });

    static constexpr auto ldmia_sum_block = arm_macro([](auto& b) {
        b.mov_reg(arm_reg::r2, arm_reg::r0)
         .ldmia(arm_reg::r2, reg_list(arm_reg::r0, arm_reg::r1))
         .add_reg(arm_reg::r0, arm_reg::r0, arm_reg::r1)
         .bx(arm_reg::lr);
    });

    static constexpr auto stmia_pair_block = arm_macro([](auto& b) {
        b.mov_reg(arm_reg::r3, arm_reg::r0)
         .stmia(arm_reg::r3, reg_list(arm_reg::r1, arm_reg::r2))
         .bx(arm_reg::lr);
    });

} // namespace

int main() {
    using namespace gba::codegen;

    gba::test("builder encodes expected words", [] {
        gba::test.eq(block.count, static_cast<std::size_t>(2));
        gba::test.eq(static_cast<std::uint32_t>(block[0]), 0xE3A0002Au);
        gba::test.eq(static_cast<std::uint32_t>(block[1]), 0xE12FFF1Eu);
    });

    gba::test("branch encoding uses mark index", [] {
        gba::test.eq(branch_block.count, static_cast<std::size_t>(2));
        gba::test.eq(static_cast<std::uint32_t>(branch_block[0]), 0xEAFFFFFEu);
    });

    gba::test("arm_macro infers capacity for block", [] {
        gba::test.eq(inferred_block.count, static_cast<std::size_t>(2));
        gba::test.eq(static_cast<std::uint32_t>(inferred_block[0]), 0xE3A00007u);
    });

    gba::test("arm_macro supports runtime imm patch slots", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());
        apply_patches<void()>(patched_block, code, 4, 5u, 9u);

        gba::test.eq(code[0], 0xE3A00005u);
        gba::test.eq(code[1], 0xE2811009u);
    });

          gba::test("compiled_block::patcher supports shared named args (\"x\"_arg)", [] {
            alignas(4) std::uint32_t code[4] = {};
            std::memcpy(code, named_reuse_block.data(), named_reuse_block.size_bytes());

            const auto patcher = named_reuse_block.patcher();
            patcher(code, "x"_arg = 7u);

            // mov r0, #7; add r0, r0, #7
            gba::test.eq(code[0], 0xE3A00007u);
            gba::test.eq(code[1], 0xE2800007u);

            auto fn = reinterpret_cast<int (*)()>(code);
            gba::test.eq(fn(), 14);
          });

          gba::test("compiled_block::patcher<Sig>() returns typed patcher whose op() returns fn ptr", [] {
            alignas(4) std::uint32_t code[4] = {};
            std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());

            constexpr auto typed_patch = return_imm_block.patcher<int()>();
            const auto fn = typed_patch(code, 99u);  // returns int(*)() directly

            gba::test.is_true(fn != nullptr);
            gba::test.eq(fn(), 99);
          });

          gba::test("compiled_block::patcher<Sig>() typed patcher re-patch produces new fn ptr", [] {
            alignas(4) std::uint32_t code[4] = {};
            std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());

            constexpr auto typed_patch = return_imm_block.patcher<int()>();
            const auto fn42 = typed_patch(code, 42u);
            gba::test.eq(fn42(), 42);
            const auto fn77 = typed_patch(code, 77u);
            gba::test.eq(fn77(), 77);
          });

          gba::test("compiled_block::patcher<Sig>() typed patcher supports named args", [] {
            alignas(4) std::uint32_t code[4] = {};
            std::memcpy(code, named_reuse_block.data(), named_reuse_block.size_bytes());

            constexpr auto typed_patch = named_reuse_block.patcher<int()>();
            const auto fn = typed_patch(code, "x"_arg = 5u);  // returns int(*)() directly
            gba::test.eq(fn(), 10);  // 5 + 5
          });

          gba::test("block_patcher<Block>::typed<Sig>() returns typed patcher whose op() returns fn ptr", [] {
            alignas(4) std::uint32_t code[4] = {};
            std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());

            constexpr auto typed_patch = block_patcher<return_imm_block>{}.typed<int()>();
            const auto fn = typed_patch(code, 55u);  // returns int(*)() directly

            gba::test.is_true(fn != nullptr);
            gba::test.eq(fn(), 55);
          });

    gba::test("arm_macro supports runtime signed12 patch slots", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, offset_patched_block.data(), offset_patched_block.size_bytes());
        apply_patches<void()>(offset_patched_block, code, 4, 12, -7);

        gba::test.eq(code[0], 0xE593200Cu);
        gba::test.eq(code[1], 0xE5054007u);
    });

    gba::test("apply_patches returns non-null raw function pointer", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());
        const auto fn = apply_patches<void()>(patched_block, code, 4, 3u, 4u);

        gba::test.is_true(fn != nullptr);
        gba::test.eq(code[0], 0xE3A00003u);
        gba::test.eq(code[1], 0xE2811004u);
    });

    gba::test("arm_macro supports runtime branch patch slots", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, branch_patched_block.data(), branch_patched_block.size_bytes());
        apply_patches<void()>(branch_patched_block, code, 4, -2);

        gba::test.eq(code[0], 0xEAFFFFFEu);
    });

    gba::test("block data/size accessors cover instruction region", [] {
        // data() points to the first word, size_bytes() = count * 4
        gba::test.eq(patched_block.size(), static_cast<std::size_t>(3));
        gba::test.eq(patched_block.size_bytes(), static_cast<std::size_t>(12));
        gba::test.is_true(patched_block.data() != nullptr);
        gba::test.eq(static_cast<std::uint32_t>(patched_block.data()[0]),
                     static_cast<std::uint32_t>(patched_block[0]));
    });

    gba::test("copy then apply_patches updates template words", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());

        // Unpatched template words in place
        gba::test.eq(code[0], 0xE3A00000u);
        gba::test.eq(code[1], 0xE2811000u);

        apply_patches<void()>(patched_block, code, 4, 5u, 9u);
        gba::test.eq(code[0], 0xE3A00005u);
        gba::test.eq(code[1], 0xE2811009u);
    });

    gba::test("re-patching updates previously patched words", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());

        apply_patches<void()>(patched_block, code, 4, 5u, 9u);
        gba::test.eq(code[0], 0xE3A00005u);

        apply_patches<void()>(patched_block, code, 4, 10u, 20u);
        gba::test.eq(code[0], 0xE3A0000Au);
        gba::test.eq(code[1], 0xE2811014u);
    });

    gba::test("re-patching updates single-patch code", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());

        apply_patches<void()>(return_imm_block, code, 4, 42u);
        gba::test.eq(code[0], 0xE3A0002Au);

        apply_patches<void()>(return_imm_block, code, 4, 100u);
        gba::test.eq(code[0], 0xE3A00064u);
    });

    gba::test("re-patching updates four imm8 patches", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, repatch_quad_block.data(), repatch_quad_block.size_bytes());

        apply_patches<void()>(repatch_quad_block, code, 8, 1u, 2u, 3u, 4u);
        apply_patches<void()>(repatch_quad_block, code, 8, 5u, 6u, 7u, 8u);

        gba::test.eq(code[0], 0xE3A00005u);
        gba::test.eq(code[1], 0xE3A01006u);
        gba::test.eq(code[2], 0xE2800007u);
        gba::test.eq(code[3], 0xE2411008u);
    });

    gba::test("re-patching updates eight imm8 patches", [] {
        alignas(4) std::uint32_t code[12] = {};
        std::memcpy(code, repatch_oct_block.data(), repatch_oct_block.size_bytes());

        apply_patches<void()>(repatch_oct_block, code, 12, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);
        apply_patches<void()>(repatch_oct_block, code, 12, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u);

        gba::test.eq(code[0], 0xE3A00009u);
        gba::test.eq(code[3], 0xE3A0300Cu);
        gba::test.eq(code[4], 0xE280000Du);
        gba::test.eq(code[7], 0xE2433010u);
    });

    gba::test("apply_patches raw function pointer matches patched bytes", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());

        auto fn = apply_patches<void()>(patched_block, code, 4, 3u, 4u);
        gba::test.is_true(fn != nullptr);
        gba::test.eq(code[0], 0xE3A00003u);
        gba::test.eq(code[1], 0xE2811004u);
    });

    gba::test("apply_patches supports prepacked argument arrays", [] {
        alignas(4) std::uint32_t code[4] = {};
        alignas(4) const std::uint32_t args[2] = {5u, 9u};
        std::memcpy(code, patched_block.data(), patched_block.size_bytes());

        auto fn = apply_patches_packed<void()>(patched_block, code, 4, args, static_cast<std::size_t>(2));
        gba::test.is_true(fn != nullptr);
        gba::test.eq(code[0], 0xE3A00005u);
        gba::test.eq(code[1], 0xE2811009u);
    });

    gba::test("apply_word_patches streams full-word values from callback", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, word_patched_block.data(), word_patched_block.size_bytes());

        apply_word_patches(word_patched_block, code, 8, [](const std::size_t patch_index) {
            return static_cast<std::uint32_t>(0xA0u + patch_index);
        });

        gba::test.eq(code[0], 0xA0u);
        gba::test.eq(code[1], 0xA1u);
        gba::test.eq(code[2], 0xA2u);
        gba::test.eq(code[3], 0xA3u);
    });

    gba::test("compiled_block patch metadata accessors expose stable patch positions", [] {
        gba::test.eq(word_patched_block.patch_count, static_cast<std::size_t>(4));
        gba::test.eq(word_patched_block.patch_word_index(0), static_cast<std::size_t>(0));
        gba::test.eq(word_patched_block.patch_word_index(3), static_cast<std::size_t>(3));
        gba::test.eq(word_patched_block.patch_arg_index(2), static_cast<std::size_t>(2));
        gba::test.is_true(word_patched_block.all_patches_word32());
        gba::test.eq(static_cast<unsigned>(word_patched_block.patch_type(1)),
                     static_cast<unsigned>(patch_kind::instruction));
    });

    gba::test("generated function executes: return immediate", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());
        auto fn = apply_patches<int()>(return_imm_block, code, 4, 42u);
        gba::test.eq(fn(), 42);
    });

    gba::test("generated function executes: return different immediate", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, return_imm_block.data(), return_imm_block.size_bytes());
        auto fn = apply_patches<int()>(return_imm_block, code, 4, 100u);
        gba::test.eq(fn(), 100);
    });

    gba::test("generated function executes: return sum of immediates", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, return_sum_block.data(), return_sum_block.size_bytes());
        auto fn = apply_patches<int()>(return_sum_block, code, 8, 30u, 12u);
        gba::test.eq(fn(), 42);
    });

    gba::test("generated function executes: int(int) AAPCS identity", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, runtime_arg_identity_block.data(), runtime_arg_identity_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(42), 42);
        gba::test.eq(fn(7), 7);
    });

    gba::test("generated function executes: int(int,int) AAPCS add", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, runtime_arg_add_block.data(), runtime_arg_add_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(30, 12), 42);
        gba::test.eq(fn(100, 23), 123);
    });

    gba::test("generated function executes: int(int,int) AAPCS sub", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, runtime_arg_sub_block.data(), runtime_arg_sub_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(50, 8), 42);
        gba::test.eq(fn(10, 3), 7);
    });

    gba::test("generated function executes: mixed runtime arg and patch-time constant", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, runtime_arg_scale_add_block.data(), runtime_arg_scale_add_block.size_bytes());
        auto fn = apply_patches<int(int)>(runtime_arg_scale_add_block, code, 8, 2u);
        gba::test.eq(fn(10), 42);
        gba::test.eq(fn(5), 22);
    });

    gba::test("re-patch and execute with different values", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, return_sum_block.data(), return_sum_block.size_bytes());

        auto fn1 = apply_patches<int()>(return_sum_block, code, 8, 25u, 17u);
        gba::test.eq(fn1(), 42);

        auto fn2 = apply_patches<int()>(return_sum_block, code, 8, 100u, 200u);
        gba::test.eq(fn2(), 300);
    });

    gba::test("generated function executes: shift and orr", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, runtime_arg_shift_orr_block.data(), runtime_arg_shift_orr_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(20, 2), 42);
        gba::test.eq(fn(5, 2), 10 | 2);
    });

    gba::test("generated function executes: conditional flow via cmp and branch", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, conditional_select_block.data(), conditional_select_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(0), 1);
        gba::test.eq(fn(5), 2);
    });

    gba::test("generated function executes: ldrh halfword load", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, ldrh_block.data(), ldrh_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(std::uint16_t*)>(code);
        alignas(2) std::uint16_t value = 42;
        gba::test.eq(fn(&value), 42);
    });

    gba::test("generated function executes: strh halfword store", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, strh_block.data(), strh_block.size_bytes());
        const auto fn = reinterpret_cast<void (*)(std::uint16_t*, int)>(code);
        alignas(2) std::uint16_t value = 0;
        fn(&value, 42);
        gba::test.eq(value, static_cast<std::uint16_t>(42));
    });

    gba::test("generated function executes: ldmia load-multiple sum", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, ldmia_sum_block.data(), ldmia_sum_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(std::uint32_t*)>(code);
        alignas(4) std::uint32_t values[2] = {10u, 32u};
        gba::test.eq(fn(values), 42);
    });

    gba::test("generated function executes: stmia store-multiple", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, stmia_pair_block.data(), stmia_pair_block.size_bytes());
        const auto fn = reinterpret_cast<void (*)(std::uint32_t*, int, int)>(code);
        alignas(4) std::uint32_t values[2] = {0u, 0u};
        fn(values, 11, 31);
        gba::test.eq(values[0], 11u);
        gba::test.eq(values[1], 31u);
    });

    gba::test("arm_macro supports runtime whole-instruction patch slots", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, instruction_patched_block.data(), instruction_patched_block.size_bytes());
        apply_patches<void()>(instruction_patched_block, code, 8, 10u, 3u,
                              add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1));

        gba::test.eq(code[0], 0xE3A0000Au);
        gba::test.eq(code[1], 0xE3A01003u);
        gba::test.eq(code[2], 0xE0800001u);
    });

    gba::test("generated function executes: whole-instruction patch nop/add/sub", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, instruction_patched_block.data(), instruction_patched_block.size_bytes());

        auto nop_fn = apply_patches<int()>(instruction_patched_block, code, 8, 10u, 3u, nop_instr());
        gba::test.eq(nop_fn(), 10);

        auto add_fn = apply_patches<int()>(instruction_patched_block, code, 8, 10u, 3u,
                                           add_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1));
        gba::test.eq(add_fn(), 13);

        auto sub_fn = apply_patches<int()>(instruction_patched_block, code, 8, 10u, 3u,
                                           sub_reg_instr(arm_reg::r0, arm_reg::r0, arm_reg::r1));
        gba::test.eq(sub_fn(), 7);
    });

    // Section: Instruction execution tests

    gba::test("generated function executes: and_reg bitwise AND", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, and_reg_block.data(), and_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0xFF, 0x0F), 0x0F);
        gba::test.eq(fn(0b1010, 0b1100), 0b1000);
    });

    gba::test("generated function executes: orr_reg bitwise OR", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, orr_reg_block.data(), orr_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0x10, 0x02), 0x12);
        gba::test.eq(fn(0b1010, 0b0101), 0b1111);
    });

    gba::test("generated function executes: eor_reg bitwise XOR", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, eor_reg_block.data(), eor_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0xFF, 0x0F), 0xF0);
        gba::test.eq(fn(0b1010, 0b1100), 0b0110);
    });

    gba::test("generated function executes: bic_reg bit clear", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, bic_reg_block.data(), bic_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0xFF, 0x0F), 0xF0);
        gba::test.eq(fn(0b1111, 0b0011), 0b1100);
    });

    gba::test("generated function executes: mvn_reg bitwise NOT", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, mvn_reg_block.data(), mvn_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0, 0), ~0);
        gba::test.eq(fn(0, 42), ~42);
    });

    gba::test("generated function executes: mul multiply", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, mul_block.data(), mul_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(6, 7), 42);
        gba::test.eq(fn(3, 14), 42);
    });

    gba::test("generated function executes: ror_imm rotate right by 1", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, ror_imm_block.data(), ror_imm_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(2), 1);
        gba::test.eq(fn(1), static_cast<int>(0x80000000u));
    });

    gba::test("generated function executes: lsr_reg shift right by register", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, lsr_reg_block.data(), lsr_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int, int)>(code);
        gba::test.eq(fn(0x40, 1), 0x20);
        gba::test.eq(fn(256, 3), 32);
    });

    gba::test("generated function executes: rsb_imm reverse subtract", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, rsb_block.data(), rsb_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(58), 42);
        gba::test.eq(fn(100), 0);
    });

    gba::test("generated function executes: tst + conditional branch on bit", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, tst_branch_block.data(), tst_branch_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(0b0001), 1);
        gba::test.eq(fn(0b0010), 2);
        gba::test.eq(fn(0b1001), 1);
    });

    gba::test("generated function executes: push/pop save and restore registers", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, push_pop_block.data(), push_pop_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(32), 42);
        gba::test.eq(fn(0), 10);
    });

    gba::test("generated function executes: ldrsb sign-extends byte", [] {
        alignas(4) std::uint32_t code[8] = {};
        std::memcpy(code, ldrsb_block.data(), ldrsb_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(std::uint8_t*)>(code);
        alignas(1) std::uint8_t pos = 42u;
        gba::test.eq(fn(&pos), 42);
        alignas(1) std::uint8_t neg = 0xFEu;
        gba::test.eq(fn(&neg), -2);
    });

    gba::test("generated function executes: ldrh_reg halfword load with register offset", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, ldrh_reg_block.data(), ldrh_reg_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(std::uint16_t*, int)>(code);
        alignas(2) std::uint16_t values[2] = {100, 42};
        gba::test.eq(fn(values, 0), 100);
        gba::test.eq(fn(values, 2), 42);
    });

    gba::test("generated function executes: strh_reg halfword store with register offset", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, strh_reg_block.data(), strh_reg_block.size_bytes());
        const auto fn = reinterpret_cast<void (*)(std::uint16_t*, int, int)>(code);
        alignas(2) std::uint16_t values[2] = {0, 0};
        fn(values, 0, 77);
        fn(values, 2, 88);
        gba::test.eq(values[0], static_cast<std::uint16_t>(77));
        gba::test.eq(values[1], static_cast<std::uint16_t>(88));
    });

    gba::test("generated function executes: orr_imm set high bit", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, orr_imm_block.data(), orr_imm_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(0x0F), 0x8F);
        gba::test.eq(fn(0x80), 0x80);
    });

    gba::test("generated function executes: and_imm mask low nibble", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, and_imm_block.data(), and_imm_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(0xAB), 0x0B);
        gba::test.eq(fn(0xFF), 0x0F);
    });

    gba::test("generated function executes: eor_imm flip low byte", [] {
        alignas(4) std::uint32_t code[4] = {};
        std::memcpy(code, eor_imm_block.data(), eor_imm_block.size_bytes());
        const auto fn = reinterpret_cast<int (*)(int)>(code);
        gba::test.eq(fn(0x00), 0xFF);
        gba::test.eq(fn(0xFF), 0x00);
        gba::test.eq(fn(0xAA), 0x55);
    });

    return gba::test.finish();
}
