/// @file bits/codegen/encoder.hpp
/// @brief ARM instruction encoding helpers for gba::codegen.
#pragma once

#include <cstddef>
#include <cstdint>
#include <source_location>

namespace gba::codegen::bits {

    extern "C" [[noreturn]] void __assert_func(const char* file, int line, const char* func, const char* expr);

    [[noreturn, gnu::cold]] inline void require_failed(
        const char* message, const std::source_location loc = std::source_location::current()) noexcept {
#ifdef NDEBUG
        (void)message;
        (void)loc;
        __builtin_trap();
#else
        __assert_func(loc.file_name(), static_cast<int>(loc.line()), loc.function_name(),
                      (message && message[0]) ? message : "gba::codegen validation failed");
#endif
    }

    [[gnu::always_inline]] constexpr void require(bool condition, const char* message,
                                                  const std::source_location loc = std::source_location::current()) {
        if (!condition) {
            if consteval {
                throw message;
            } else {
                require_failed(message, loc);
            }
        }
    }

    enum class arm_reg : std::uint8_t {
        r0 = 0,
        r1,
        r2,
        r3,
        r4,
        r5,
        r6,
        r7,
        r8,
        r9,
        r10,
        r11,
        r12,
        sp,
        lr,
        pc,
    };

    enum class arm_cond : std::uint8_t {
        eq = 0x0,
        ne = 0x1,
        cs = 0x2,
        cc = 0x3,
        mi = 0x4,
        pl = 0x5,
        vs = 0x6,
        vc = 0x7,
        hi = 0x8,
        ls = 0x9,
        ge = 0xA,
        lt = 0xB,
        gt = 0xC,
        le = 0xD,
        al = 0xE,
    };

    constexpr std::uint32_t reg_bits(const arm_reg reg) {
        return static_cast<std::uint32_t>(reg);
    }

    constexpr std::uint32_t cond_bits(const arm_cond cond) {
        return static_cast<std::uint32_t>(cond) << 28u;
    }

    constexpr std::uint32_t mov_imm(const arm_reg rd, const std::uint32_t imm) {
        require(imm <= 0xFFu, "mov_imm: immediate must fit 8 bits");
        return 0xE3A00000u | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t mov_reg(const arm_reg rd, const arm_reg rm) {
        return 0xE1A00000u | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t lsl_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
        require(shift <= 31u, "lsl_imm: shift must be in range 0..31");
        return 0xE1A00000u | (reg_bits(rd) << 12u) | (shift << 7u) | reg_bits(rm);
    }

    constexpr std::uint32_t lsr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
        require(shift >= 1u && shift <= 32u, "lsr_imm: shift must be in range 1..32");
        const auto encoded_shift = (shift == 32u) ? 0u : shift;
        return 0xE1A00020u | (reg_bits(rd) << 12u) | (encoded_shift << 7u) | reg_bits(rm);
    }

    constexpr std::uint32_t asr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
        require(shift >= 1u && shift <= 32u, "asr_imm: shift must be in range 1..32");
        const auto encoded_shift = (shift == 32u) ? 0u : shift;
        return 0xE1A00040u | (reg_bits(rd) << 12u) | (encoded_shift << 7u) | reg_bits(rm);
    }

    constexpr std::uint32_t add_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "add_imm: immediate must fit 8 bits");
        return 0xE2800000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t add_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0800000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t orr_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE1800000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t sub_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "sub_imm: immediate must fit 8 bits");
        return 0xE2400000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t sub_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0400000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t ldr_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -4095 && offset <= 4095, "ldr_imm: offset must fit signed 12 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE5100000u : 0xE5900000u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | magnitude;
    }

    constexpr std::uint32_t str_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -4095 && offset <= 4095, "str_imm: offset must fit signed 12 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE5000000u : 0xE5800000u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | magnitude;
    }

    constexpr std::uint32_t ldrb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -4095 && offset <= 4095, "ldrb_imm: offset must fit signed 12 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE5500000u : 0xE5D00000u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | magnitude;
    }

    constexpr std::uint32_t strb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -4095 && offset <= 4095, "strb_imm: offset must fit signed 12 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE5400000u : 0xE5C00000u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | magnitude;
    }

    constexpr std::uint32_t ldrb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE7D00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t strb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE7C00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t ldrh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -255 && offset <= 255, "ldrh_imm: offset must fit signed 8 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE15000B0u : 0xE1D000B0u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | ((magnitude & 0xF0u) << 4u) | (magnitude & 0x0Fu);
    }

    constexpr std::uint32_t strh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -255 && offset <= 255, "strh_imm: offset must fit signed 8 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE14000B0u : 0xE1C000B0u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | ((magnitude & 0xF0u) << 4u) | (magnitude & 0x0Fu);
    }

    constexpr std::uint32_t ldmia(const arm_reg rn, const std::uint16_t reg_list, const bool writeback = false) {
        require(reg_list != 0u, "ldmia: register list must not be empty");
        return 0xE8900000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | reg_list;
    }

    constexpr std::uint32_t stmia(const arm_reg rn, const std::uint16_t reg_list, const bool writeback = false) {
        require(reg_list != 0u, "stmia: register list must not be empty");
        return 0xE8800000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | reg_list;
    }

    constexpr std::uint32_t cmp_imm(const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "cmp_imm: immediate must fit 8 bits");
        return 0xE3500000u | (reg_bits(rn) << 16u) | imm;
    }

    constexpr std::uint32_t cmp_reg(const arm_reg rn, const arm_reg rm) {
        return 0xE1500000u | (reg_bits(rn) << 16u) | reg_bits(rm);
    }

    constexpr std::uint32_t bx(const arm_reg rm) {
        return 0xE12FFF10u | reg_bits(rm);
    }

    constexpr std::uint32_t nop() {
        return 0xE1A00000u;
    }

    constexpr std::uint32_t orr_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "orr_imm: immediate must fit 8 bits");
        return 0xE3800000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t and_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "and_imm: immediate must fit 8 bits");
        return 0xE2000000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t and_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0000000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t eor_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "eor_imm: immediate must fit 8 bits");
        return 0xE2200000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t eor_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0200000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t bic_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "bic_imm: immediate must fit 8 bits");
        return 0xE3C00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t bic_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE1C00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t mvn_imm(const arm_reg rd, const std::uint32_t imm) {
        require(imm <= 0xFFu, "mvn_imm: immediate must fit 8 bits");
        return 0xE3E00000u | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t mvn_reg(const arm_reg rd, const arm_reg rm) {
        return 0xE1E00000u | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t rsb_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "rsb_imm: immediate must fit 8 bits");
        return 0xE2600000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t rsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0600000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t adc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "adc_imm: immediate must fit 8 bits");
        return 0xE2A00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t adc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0A00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t sbc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "sbc_imm: immediate must fit 8 bits");
        return 0xE2C00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | imm;
    }

    constexpr std::uint32_t sbc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE0C00000u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t ror_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
        require(shift >= 1u && shift <= 31u, "ror_imm: shift must be in range 1..31");
        return 0xE1A00060u | (reg_bits(rd) << 12u) | (shift << 7u) | reg_bits(rm);
    }

    constexpr std::uint32_t lsl_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        return 0xE1A00010u | (reg_bits(rd) << 12u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }

    constexpr std::uint32_t lsr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        return 0xE1A00030u | (reg_bits(rd) << 12u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }

    constexpr std::uint32_t asr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        return 0xE1A00050u | (reg_bits(rd) << 12u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }

    constexpr std::uint32_t ror_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        return 0xE1A00070u | (reg_bits(rd) << 12u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }

    constexpr std::uint32_t ldrh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE19000B0u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t strh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE18000B0u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t ldrsb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -255 && offset <= 255, "ldrsb_imm: offset must fit signed 8 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE15000D0u : 0xE1D000D0u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | ((magnitude & 0xF0u) << 4u) | (magnitude & 0x0Fu);
    }

    constexpr std::uint32_t ldrsh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
        require(offset >= -255 && offset <= 255, "ldrsh_imm: offset must fit signed 8 bits");
        const auto magnitude = static_cast<std::uint32_t>(offset < 0 ? -offset : offset);
        const auto base = offset < 0 ? 0xE15000F0u : 0xE1D000F0u;
        return base | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | ((magnitude & 0xF0u) << 4u) | (magnitude & 0x0Fu);
    }

    constexpr std::uint32_t ldrsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE19000D0u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t ldrsh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
        return 0xE19000F0u | (reg_bits(rn) << 16u) | (reg_bits(rd) << 12u) | reg_bits(rm);
    }

    constexpr std::uint32_t cmn_imm(const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "cmn_imm: immediate must fit 8 bits");
        return 0xE3700000u | (reg_bits(rn) << 16u) | imm;
    }

    constexpr std::uint32_t cmn_reg(const arm_reg rn, const arm_reg rm) {
        return 0xE1700000u | (reg_bits(rn) << 16u) | reg_bits(rm);
    }

    constexpr std::uint32_t tst_imm(const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "tst_imm: immediate must fit 8 bits");
        return 0xE3100000u | (reg_bits(rn) << 16u) | imm;
    }

    constexpr std::uint32_t tst_reg(const arm_reg rn, const arm_reg rm) {
        return 0xE1100000u | (reg_bits(rn) << 16u) | reg_bits(rm);
    }

    constexpr std::uint32_t teq_imm(const arm_reg rn, const std::uint32_t imm) {
        require(imm <= 0xFFu, "teq_imm: immediate must fit 8 bits");
        return 0xE3300000u | (reg_bits(rn) << 16u) | imm;
    }

    constexpr std::uint32_t teq_reg(const arm_reg rn, const arm_reg rm) {
        return 0xE1300000u | (reg_bits(rn) << 16u) | reg_bits(rm);
    }

    constexpr std::uint32_t push(const std::uint16_t regs) {
        require(regs != 0u, "push: register list must not be empty");
        return 0xE92D0000u | regs;
    }

    constexpr std::uint32_t pop(const std::uint16_t regs) {
        require(regs != 0u, "pop: register list must not be empty");
        return 0xE8BD0000u | regs;
    }

    constexpr std::uint32_t ldmib(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "ldmib: register list must not be empty");
        return 0xE9900000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t ldmda(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "ldmda: register list must not be empty");
        return 0xE8100000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t ldmdb(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "ldmdb: register list must not be empty");
        return 0xE9100000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t stmib(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "stmib: register list must not be empty");
        return 0xE9800000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t stmda(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "stmda: register list must not be empty");
        return 0xE8000000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t stmdb(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
        require(regs != 0u, "stmdb: register list must not be empty");
        return 0xE9000000u | (writeback ? (1u << 21u) : 0u) | (reg_bits(rn) << 16u) | regs;
    }

    constexpr std::uint32_t mul(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        require(rd != rm, "mul: Rd must not equal Rm (ARM7TDMI constraint)");
        return 0xE0000090u | (reg_bits(rd) << 16u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }

    constexpr std::uint32_t mla(const arm_reg rd, const arm_reg rm, const arm_reg rs, const arm_reg rn) {
        require(rd != rm, "mla: Rd must not equal Rm (ARM7TDMI constraint)");
        return 0xE0200090u | (reg_bits(rd) << 16u) | (reg_bits(rn) << 12u) | (reg_bits(rs) << 8u) | reg_bits(rm);
    }


    constexpr std::uint32_t bl_to(const std::size_t current_index, const std::size_t target_index) {
        const auto current = static_cast<long long>(current_index);
        const auto target = static_cast<long long>(target_index);
        const auto offset_words = target - (current + 2);
        require(offset_words >= -(1LL << 23) && offset_words < (1LL << 23), "bl_to: branch target out of ARM BL range");
        const auto encoded = static_cast<std::uint32_t>(offset_words) & 0x00FFFFFFu;
        return 0xEB000000u | encoded;
    }

    constexpr std::uint32_t blx(const arm_reg rm) {
        return 0xE12FFF30u | reg_bits(rm);
    }

    constexpr std::uint32_t b_if_to(const arm_cond cond, const std::size_t current_index,
                                    const std::size_t target_index) {
        const auto current = static_cast<long long>(current_index);
        const auto target = static_cast<long long>(target_index);
        const auto offset_words = target - (current + 2);

        require(offset_words >= -(1LL << 23) && offset_words < (1LL << 23),
                "b_if_to: branch target out of ARM B range");

        const auto encoded = static_cast<std::uint32_t>(offset_words) & 0x00FFFFFFu;
        return cond_bits(cond) | 0x0A000000u | encoded;
    }

    constexpr std::uint32_t b_to(const std::size_t current_index, const std::size_t target_index) {
        const auto current = static_cast<long long>(current_index);
        const auto target = static_cast<long long>(target_index);
        const auto offset_words = target - (current + 2);

        require(offset_words >= -(1LL << 23) && offset_words < (1LL << 23), "b_to: branch target out of ARM B range");

        const auto encoded = static_cast<std::uint32_t>(offset_words) & 0x00FFFFFFu;
        return 0xEA000000u | encoded;
    }

} // namespace gba::codegen::bits
