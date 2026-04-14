/// @file bits/codegen/compiler.hpp
/// @brief Builder and compiled-block types for gba::codegen.
#pragma once

#include <gba/bits/codegen/encoder.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace gba::codegen {

    using arm_reg = bits::arm_reg;
    using arm_cond = bits::arm_cond;

    struct arm_word {
        std::uint32_t value{};

        constexpr explicit(false) operator std::uint32_t() const noexcept { return value; }
    };

    struct imm_arg {
        std::uint8_t position{};
    };

    struct s12_arg {
        std::uint8_t position{};
    };

    struct b_arg {
        std::uint8_t position{};
    };

    struct instr_arg {
        std::uint8_t position{};
    };

    template<typename... Regs>
    consteval std::uint16_t reg_list(Regs... regs) {
        static_assert((std::is_same_v<Regs, arm_reg> && ...), "reg_list: all arguments must be arm_reg");
        const auto mask = (0u | ... | (1u << bits::reg_bits(regs)));
        bits::require(mask != 0u, "reg_list: register list must not be empty");
        bits::require(mask <= 0xFFFFu, "reg_list: register list out of range");
        return static_cast<std::uint16_t>(mask);
    }

    // Checked instruction helpers (for whole-instruction patch slots)
    [[nodiscard]] constexpr arm_word nop_instr() noexcept { return arm_word{bits::nop()}; }

    [[nodiscard]] constexpr arm_word add_reg_instr(const arm_reg rd, const arm_reg rn,
                                                   const arm_reg rm) noexcept {
        return arm_word{bits::add_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word sub_reg_instr(const arm_reg rd, const arm_reg rn,
                                                   const arm_reg rm) noexcept {
        return arm_word{bits::sub_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word orr_reg_instr(const arm_reg rd, const arm_reg rn,
                                                   const arm_reg rm) noexcept {
        return arm_word{bits::orr_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word and_reg_instr(const arm_reg rd, const arm_reg rn,
                                                   const arm_reg rm) noexcept {
        return arm_word{bits::and_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word eor_reg_instr(const arm_reg rd, const arm_reg rn,
                                                   const arm_reg rm) noexcept {
        return arm_word{bits::eor_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word lsl_imm_instr(const arm_reg rd, const arm_reg rm,
                                                   const unsigned shift) noexcept {
        return arm_word{bits::lsl_imm(rd, rm, shift)};
    }

    [[nodiscard]] constexpr arm_word lsr_imm_instr(const arm_reg rd, const arm_reg rm,
                                                   const unsigned shift) noexcept {
        return arm_word{bits::lsr_imm(rd, rm, shift)};
    }

    [[nodiscard]] constexpr arm_word mul_instr(const arm_reg rd, const arm_reg rm,
                                              const arm_reg rs) {
        return arm_word{bits::mul(rd, rm, rs)};
    }

    // Expose raw encoders directly for constexpr usage
    using bits::orr_imm;
    using bits::orr_reg;
    using bits::and_imm;
    using bits::and_reg;
    using bits::eor_imm;
    using bits::eor_reg;
    using bits::bic_imm;
    using bits::bic_reg;
    using bits::mvn_imm;
    using bits::mvn_reg;
    using bits::rsb_imm;
    using bits::rsb_reg;
    using bits::adc_imm;
    using bits::adc_reg;
    using bits::sbc_imm;
    using bits::sbc_reg;
    using bits::ror_imm;
    using bits::lsl_reg;
    using bits::lsr_reg;
    using bits::asr_reg;
    using bits::ror_reg;
    using bits::ldrh_reg;
    using bits::strh_reg;
    using bits::ldrsb_imm;
    using bits::ldrsh_imm;
    using bits::ldrsb_reg;
    using bits::ldrsh_reg;
    using bits::cmn_imm;
    using bits::cmn_reg;
    using bits::tst_imm;
    using bits::tst_reg;
    using bits::teq_imm;
    using bits::teq_reg;
    using bits::push;
    using bits::pop;
    using bits::ldmib;
    using bits::ldmda;
    using bits::ldmdb;
    using bits::stmib;
    using bits::stmda;
    using bits::stmdb;
    using bits::mul;
    using bits::mla;
    using bits::blx;

    consteval imm_arg imm_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "imm_slot: position out of range (0-31)");
        return imm_arg{static_cast<std::uint8_t>(n)};
    }

    consteval s12_arg s12_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "s12_slot: position out of range (0-31)");
        return s12_arg{static_cast<std::uint8_t>(n)};
    }

    consteval b_arg b_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "b_slot: position out of range (0-31)");
        return b_arg{static_cast<std::uint8_t>(n)};
    }

    consteval instr_arg instr_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "instr_slot: position out of range (0-31)");
        return instr_arg{static_cast<std::uint8_t>(n)};
    }

    using word_arg = instr_arg;
    using literal_arg = instr_arg;

    consteval word_arg word_slot(const int n) { return instr_slot(n); }

    consteval literal_arg literal_slot(const int n) { return instr_slot(n); }

    enum class patch_kind : std::uint8_t {
        imm8,
        signed12,
        branch_offset,
        instruction,
    };

    struct patch_slot {
        std::uint8_t word_index{};
        std::uint8_t arg_index{};
        patch_kind kind{};
        std::uint32_t base_word{};
    };

    template<std::size_t Capacity>
    struct compiled_block {
        std::array<arm_word, Capacity> words{};
        std::size_t count{};
        std::array<patch_slot, Capacity> patches{};
        std::size_t patch_count{};
        std::size_t arg_count{};
        std::uint8_t patch_kind_mask{};

        /// Pointer to the first instruction word.
        [[nodiscard]] constexpr const arm_word* data() const noexcept { return words.data(); }

        /// Number of instruction words in the block.
        [[nodiscard]] constexpr std::size_t size() const noexcept { return count; }

        /// Byte size of the instruction region (count * 4).
        /// Use with your preferred copy method:
        ///   std::memcpy(dest, block.data(), block.size_bytes());
        ///   dma::copy(dest, block.data(), block.size_bytes());
        [[nodiscard]] constexpr std::size_t size_bytes() const noexcept { return count * sizeof(arm_word); }

        [[nodiscard]] constexpr arm_word operator[](const std::size_t index) const {
            bits::require(index < count, "compiled_block: index out of bounds");
            return words[index];
        }

        [[nodiscard]] constexpr bool all_patches_imm8() const noexcept {
            return patch_count != 0 && patch_kind_mask == (1u << static_cast<unsigned>(patch_kind::imm8));
        }

        [[nodiscard]] constexpr bool all_patches_word32() const noexcept {
            return patch_count != 0 && patch_kind_mask == (1u << static_cast<unsigned>(patch_kind::instruction));
        }

        [[nodiscard]] constexpr std::size_t patch_word_index(const std::size_t patch_index) const {
            bits::require(patch_index < patch_count, "compiled_block: patch index out of bounds");
            return patches[patch_index].word_index;
        }

        [[nodiscard]] constexpr std::size_t patch_arg_index(const std::size_t patch_index) const {
            bits::require(patch_index < patch_count, "compiled_block: patch index out of bounds");
            return patches[patch_index].arg_index;
        }

        [[nodiscard]] constexpr patch_kind patch_type(const std::size_t patch_index) const {
            bits::require(patch_index < patch_count, "compiled_block: patch index out of bounds");
            return patches[patch_index].kind;
        }
    };

    template<std::size_t Capacity>
    struct arm_macro_builder {
        consteval arm_macro_builder& mov_imm(const arm_reg rd, const std::uint32_t imm) {
            push(bits::mov_imm(rd, imm));
            return *this;
        }

        consteval arm_macro_builder& mov_reg(const arm_reg rd, const arm_reg rm) {
            push(bits::mov_reg(rd, rm));
            return *this;
        }

        consteval arm_macro_builder& lsl_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::lsl_imm(rd, rm, shift));
            return *this;
        }

        consteval arm_macro_builder& lsr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::lsr_imm(rd, rm, shift));
            return *this;
        }

        consteval arm_macro_builder& asr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::asr_imm(rd, rm, shift));
            return *this;
        }

        consteval arm_macro_builder& mov_imm(const arm_reg rd, const imm_arg arg) {
            push_with_patch(bits::mov_imm(rd, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::add_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& add_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::add_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& orr_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::orr_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::add_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::sub_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& sub_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::sub_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::sub_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& ldr_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::ldr_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldrh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::ldrh_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldr_imm(const arm_reg rd, const arm_reg rn, const s12_arg arg) {
            push_with_patch(bits::ldr_imm(rd, rn, 0), patch_kind::signed12, arg.position);
            return *this;
        }

        consteval arm_macro_builder& str_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::str_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& strh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::strh_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldmia(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::ldmia(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& stmia(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::stmia(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& cmp_imm(const arm_reg rn, const std::uint32_t imm) {
            push(bits::cmp_imm(rn, imm));
            return *this;
        }

        consteval arm_macro_builder& cmp_reg(const arm_reg rn, const arm_reg rm) {
            push(bits::cmp_reg(rn, rm));
            return *this;
        }

        consteval arm_macro_builder& str_imm(const arm_reg rd, const arm_reg rn, const s12_arg arg) {
            push_with_patch(bits::str_imm(rd, rn, 0), patch_kind::signed12, arg.position);
            return *this;
        }

         consteval arm_macro_builder& bx(const arm_reg rm) {
             push(bits::bx(rm));
             return *this;
         }

         consteval arm_macro_builder& blx(const arm_reg rm) {
             push(bits::blx(rm));
             return *this;
         }

         consteval arm_macro_builder& b_to(const std::size_t target_index) {
             push(bits::b_to(m_count, target_index));
             return *this;
         }

         consteval arm_macro_builder& bl_to(const std::size_t target_index) {
             push(bits::bl_to(m_count, target_index));
             return *this;
         }

         consteval arm_macro_builder& b_if(const arm_cond cond, const std::size_t target_index) {
             push(bits::b_if_to(cond, m_count, target_index));
             return *this;
         }

         consteval arm_macro_builder& b_to(const b_arg arg) {
             push_with_patch(0xEA000000u, patch_kind::branch_offset, arg.position);
             return *this;
         }

         consteval arm_macro_builder& b_if(const arm_cond cond, const b_arg arg) {
             push_with_patch(bits::cond_bits(cond) | 0x0A000000u, patch_kind::branch_offset, arg.position);
             return *this;
         }

         consteval arm_macro_builder& instruction(const instr_arg arg) {
             push_with_patch(0u, patch_kind::instruction, arg.position);
             return *this;
         }

         consteval arm_macro_builder& word(const word_arg arg) {
             return instruction(instr_arg{arg.position});
         }

         consteval arm_macro_builder& literal_word(const literal_arg arg) {
             return instruction(instr_arg{arg.position});
         }

         // ---- Bitwise ----

         consteval arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::orr_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::and_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& and_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::and_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::eor_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& eor_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::eor_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::bic_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& bic_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::bic_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& mvn_imm(const arm_reg rd, const std::uint32_t imm) {
             push(bits::mvn_imm(rd, imm));
             return *this;
         }

         consteval arm_macro_builder& mvn_reg(const arm_reg rd, const arm_reg rm) {
             push(bits::mvn_reg(rd, rm));
             return *this;
         }

         // ---- Additional arithmetic ----

         consteval arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::rsb_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& rsb_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::rsb_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& adc_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::adc_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& adc_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::adc_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& sbc_imm(const arm_reg rd, const arm_reg rn,
                                              const std::uint32_t imm) {
             push(bits::sbc_imm(rd, rn, imm));
             return *this;
         }

         consteval arm_macro_builder& sbc_reg(const arm_reg rd, const arm_reg rn,
                                              const arm_reg rm) {
             push(bits::sbc_reg(rd, rn, rm));
             return *this;
         }

         // ---- Shifts ----

         consteval arm_macro_builder& ror_imm(const arm_reg rd, const arm_reg rm,
                                              const unsigned shift) {
             push(bits::ror_imm(rd, rm, shift));
             return *this;
         }

         consteval arm_macro_builder& lsl_reg(const arm_reg rd, const arm_reg rm,
                                              const arm_reg rs) {
             push(bits::lsl_reg(rd, rm, rs));
             return *this;
         }

         consteval arm_macro_builder& lsr_reg(const arm_reg rd, const arm_reg rm,
                                              const arm_reg rs) {
             push(bits::lsr_reg(rd, rm, rs));
             return *this;
         }

         consteval arm_macro_builder& asr_reg(const arm_reg rd, const arm_reg rm,
                                              const arm_reg rs) {
             push(bits::asr_reg(rd, rm, rs));
             return *this;
         }

         consteval arm_macro_builder& ror_reg(const arm_reg rd, const arm_reg rm,
                                              const arm_reg rs) {
             push(bits::ror_reg(rd, rm, rs));
             return *this;
         }

         // ---- Halfword / signed-byte memory ----

         consteval arm_macro_builder& ldrh_reg(const arm_reg rd, const arm_reg rn,
                                               const arm_reg rm) {
             push(bits::ldrh_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& strh_reg(const arm_reg rd, const arm_reg rn,
                                               const arm_reg rm) {
             push(bits::strh_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& ldrsb_imm(const arm_reg rd, const arm_reg rn,
                                                const int offset) {
             push(bits::ldrsb_imm(rd, rn, offset));
             return *this;
         }

         consteval arm_macro_builder& ldrsh_imm(const arm_reg rd, const arm_reg rn,
                                                const int offset) {
             push(bits::ldrsh_imm(rd, rn, offset));
             return *this;
         }

         consteval arm_macro_builder& ldrsb_reg(const arm_reg rd, const arm_reg rn,
                                                const arm_reg rm) {
             push(bits::ldrsb_reg(rd, rn, rm));
             return *this;
         }

         consteval arm_macro_builder& ldrsh_reg(const arm_reg rd, const arm_reg rn,
                                                const arm_reg rm) {
             push(bits::ldrsh_reg(rd, rn, rm));
             return *this;
         }

         // ---- Comparison / flag setters ----

         consteval arm_macro_builder& cmn_imm(const arm_reg rn, const std::uint32_t imm) {
             push(bits::cmn_imm(rn, imm));
             return *this;
         }

         consteval arm_macro_builder& cmn_reg(const arm_reg rn, const arm_reg rm) {
             push(bits::cmn_reg(rn, rm));
             return *this;
         }

         consteval arm_macro_builder& tst_imm(const arm_reg rn, const std::uint32_t imm) {
             push(bits::tst_imm(rn, imm));
             return *this;
         }

         consteval arm_macro_builder& tst_reg(const arm_reg rn, const arm_reg rm) {
             push(bits::tst_reg(rn, rm));
             return *this;
         }

         consteval arm_macro_builder& teq_imm(const arm_reg rn, const std::uint32_t imm) {
             push(bits::teq_imm(rn, imm));
             return *this;
         }

         consteval arm_macro_builder& teq_reg(const arm_reg rn, const arm_reg rm) {
             push(bits::teq_reg(rn, rm));
             return *this;
         }

         // ---- Multi-register / stack ----

         // push takes a reg_list bitmask (use reg_list(...) helper)
         consteval arm_macro_builder& push(const std::uint16_t regs) {
             push(bits::push(regs));
             return *this;
         }

         // pop takes a reg_list bitmask (use reg_list(...) helper)
         consteval arm_macro_builder& pop(const std::uint16_t regs) {
             push(bits::pop(regs));
             return *this;
         }

         consteval arm_macro_builder& ldmib(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::ldmib(rn, regs, writeback));
             return *this;
         }

         consteval arm_macro_builder& ldmda(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::ldmda(rn, regs, writeback));
             return *this;
         }

         consteval arm_macro_builder& ldmdb(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::ldmdb(rn, regs, writeback));
             return *this;
         }

         consteval arm_macro_builder& stmib(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::stmib(rn, regs, writeback));
             return *this;
         }

         consteval arm_macro_builder& stmda(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::stmda(rn, regs, writeback));
             return *this;
         }

         consteval arm_macro_builder& stmdb(const arm_reg rn, const std::uint16_t regs,
                                            const bool writeback = false) {
             push(bits::stmdb(rn, regs, writeback));
             return *this;
         }

         // ---- Multiply ----

         consteval arm_macro_builder& mul(const arm_reg rd, const arm_reg rm,
                                          const arm_reg rs) {
             push(bits::mul(rd, rm, rs));
             return *this;
         }

         consteval arm_macro_builder& mla(const arm_reg rd, const arm_reg rm, const arm_reg rs,
                                          const arm_reg rn) {
             push(bits::mla(rd, rm, rs, rn));
             return *this;
         }

         [[nodiscard]] consteval std::size_t mark() const noexcept { return m_count; }

         [[nodiscard]] consteval compiled_block<Capacity> compile() const noexcept {
             return {m_words, m_count, m_patches, m_patchCount, m_argCount, m_patchKindMask};
         }

     private:
        std::array<arm_word, Capacity> m_words{};
        std::size_t m_count{};
        std::array<patch_slot, Capacity> m_patches{};
        std::size_t m_patchCount{};
        std::size_t m_argCount{};
        std::uint8_t m_patchKindMask{};

          [[nodiscard]] static consteval std::uint32_t patch_base(const std::uint32_t word, const patch_kind kind) {
              switch (kind) {
                  case patch_kind::imm8:
                      return word & 0xFFFFFF00u;
                  case patch_kind::signed12:
                      return word & ~(0xFFFu | (1u << 23u));
                  case patch_kind::branch_offset:
                      return word & 0xFF000000u;
                   case patch_kind::instruction:
                       return 0u;
              }
              return word;
          }

        consteval void emit(const std::uint32_t word) {
            bits::require(m_count < Capacity, "arm_macro_builder: instruction capacity exceeded");
            m_words[m_count++] = arm_word{word};
        }

        consteval void push_with_patch(const std::uint32_t word, const patch_kind kind, const std::uint8_t argIndex) {
            bits::require(m_patchCount < Capacity, "arm_macro_builder: patch capacity exceeded");
            bits::require(argIndex <= 31, "arm_macro_builder: patch arg out of range (0-31)");
            bits::require(m_count < Capacity, "arm_macro_builder: instruction capacity exceeded");

            m_patches[m_patchCount++] = {
                static_cast<std::uint8_t>(m_count),
                argIndex,
                kind,
                patch_base(word, kind),
            };
            if (static_cast<std::size_t>(argIndex + 1) > m_argCount) {
                m_argCount = static_cast<std::size_t>(argIndex + 1);
            }
            m_patchKindMask |= static_cast<std::uint8_t>(1u << static_cast<unsigned>(kind));
            m_words[m_count++] = arm_word{word};
        }

        // Helper to emit a raw instruction word (renamed to avoid conflict with push(regs))
        consteval void push(const std::uint32_t word) { emit(word); }
    };

    namespace bits {

        struct counting_arm_macro_builder {
            consteval counting_arm_macro_builder& mov_imm(const arm_reg rd, const std::uint32_t imm) {
                (void)::gba::codegen::bits::mov_imm(rd, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mov_reg(const arm_reg rd, const arm_reg rm) {
                (void)::gba::codegen::bits::mov_reg(rd, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& lsl_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
                (void)::gba::codegen::bits::lsl_imm(rd, rm, shift);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& lsr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
                (void)::gba::codegen::bits::lsr_imm(rd, rm, shift);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& asr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
                (void)::gba::codegen::bits::asr_imm(rd, rm, shift);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mov_imm(const arm_reg rd, const imm_arg arg) {
                (void)::gba::codegen::bits::mov_imm(rd, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::add_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& add_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::add_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& orr_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::orr_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::add_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::sub_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& sub_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::sub_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::sub_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldr_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::ldr_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::ldrh_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldr_imm(const arm_reg rd, const arm_reg rn, const s12_arg arg) {
                (void)::gba::codegen::bits::ldr_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& str_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::str_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& strh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::strh_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldmia(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::ldmia(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& stmia(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::stmia(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& cmp_imm(const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::cmp_imm(rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& cmp_reg(const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::cmp_reg(rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& str_imm(const arm_reg rd, const arm_reg rn, const s12_arg arg) {
                (void)::gba::codegen::bits::str_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& bx(const arm_reg rm) {
                (void)::gba::codegen::bits::bx(rm);
                ++m_count;
                return *this;
            }

             consteval counting_arm_macro_builder& blx(const arm_reg rm) {
                 (void)::gba::codegen::bits::blx(rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& b_to(const std::size_t target_index) {
                 (void)::gba::codegen::bits::b_to(m_count, target_index);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& bl_to(const std::size_t target_index) {
                 (void)::gba::codegen::bits::bl_to(m_count, target_index);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& b_if(const arm_cond cond, const std::size_t target_index) {
                 (void)::gba::codegen::bits::b_if_to(cond, m_count, target_index);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& b_to(const b_arg arg) {
                 track_arg(arg.position);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& b_if(const arm_cond cond, const b_arg arg) {
                 (void)cond;
                 track_arg(arg.position);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& instruction(const instr_arg arg) {
                 track_arg(arg.position);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& word(const word_arg arg) {
                 return instruction(instr_arg{arg.position});
             }

             consteval counting_arm_macro_builder& literal_word(const literal_arg arg) {
                 return instruction(instr_arg{arg.position});
             }

             // ---- Bitwise ----

             consteval counting_arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::orr_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::and_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& and_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::and_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::eor_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& eor_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::eor_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::bic_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& bic_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::bic_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& mvn_imm(const arm_reg rd,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::mvn_imm(rd, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& mvn_reg(const arm_reg rd, const arm_reg rm) {
                 (void)::gba::codegen::bits::mvn_reg(rd, rm);
                 ++m_count;
                 return *this;
             }

             // ---- Additional arithmetic ----

             consteval counting_arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::rsb_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& rsb_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::rsb_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& adc_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::adc_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& adc_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::adc_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& sbc_imm(const arm_reg rd, const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::sbc_imm(rd, rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& sbc_reg(const arm_reg rd, const arm_reg rn,
                                                           const arm_reg rm) {
                 (void)::gba::codegen::bits::sbc_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             // ---- Shifts ----

             consteval counting_arm_macro_builder& ror_imm(const arm_reg rd, const arm_reg rm,
                                                           const unsigned shift) {
                 (void)::gba::codegen::bits::ror_imm(rd, rm, shift);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& lsl_reg(const arm_reg rd, const arm_reg rm,
                                                           const arm_reg rs) {
                 (void)::gba::codegen::bits::lsl_reg(rd, rm, rs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& lsr_reg(const arm_reg rd, const arm_reg rm,
                                                           const arm_reg rs) {
                 (void)::gba::codegen::bits::lsr_reg(rd, rm, rs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& asr_reg(const arm_reg rd, const arm_reg rm,
                                                           const arm_reg rs) {
                 (void)::gba::codegen::bits::asr_reg(rd, rm, rs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ror_reg(const arm_reg rd, const arm_reg rm,
                                                           const arm_reg rs) {
                 (void)::gba::codegen::bits::ror_reg(rd, rm, rs);
                 ++m_count;
                 return *this;
             }

             // ---- Halfword / signed-byte memory ----

             consteval counting_arm_macro_builder& ldrh_reg(const arm_reg rd, const arm_reg rn,
                                                            const arm_reg rm) {
                 (void)::gba::codegen::bits::ldrh_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& strh_reg(const arm_reg rd, const arm_reg rn,
                                                            const arm_reg rm) {
                 (void)::gba::codegen::bits::strh_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldrsb_imm(const arm_reg rd, const arm_reg rn,
                                                             const int offset) {
                 (void)::gba::codegen::bits::ldrsb_imm(rd, rn, offset);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldrsh_imm(const arm_reg rd, const arm_reg rn,
                                                             const int offset) {
                 (void)::gba::codegen::bits::ldrsh_imm(rd, rn, offset);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldrsb_reg(const arm_reg rd, const arm_reg rn,
                                                             const arm_reg rm) {
                 (void)::gba::codegen::bits::ldrsb_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldrsh_reg(const arm_reg rd, const arm_reg rn,
                                                             const arm_reg rm) {
                 (void)::gba::codegen::bits::ldrsh_reg(rd, rn, rm);
                 ++m_count;
                 return *this;
             }

             // ---- Comparison / flag setters ----

             consteval counting_arm_macro_builder& cmn_imm(const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::cmn_imm(rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& cmn_reg(const arm_reg rn, const arm_reg rm) {
                 (void)::gba::codegen::bits::cmn_reg(rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& tst_imm(const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::tst_imm(rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& tst_reg(const arm_reg rn, const arm_reg rm) {
                 (void)::gba::codegen::bits::tst_reg(rn, rm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& teq_imm(const arm_reg rn,
                                                           const std::uint32_t imm) {
                 (void)::gba::codegen::bits::teq_imm(rn, imm);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& teq_reg(const arm_reg rn, const arm_reg rm) {
                 (void)::gba::codegen::bits::teq_reg(rn, rm);
                 ++m_count;
                 return *this;
             }

             // ---- Multi-register / stack ----

             consteval counting_arm_macro_builder& push(const std::uint16_t regs) {
                 (void)::gba::codegen::bits::push(regs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& pop(const std::uint16_t regs) {
                 (void)::gba::codegen::bits::pop(regs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldmib(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::ldmib(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldmda(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::ldmda(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& ldmdb(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::ldmdb(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& stmib(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::stmib(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& stmda(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::stmda(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& stmdb(const arm_reg rn,
                                                         const std::uint16_t regs,
                                                         const bool writeback = false) {
                 (void)::gba::codegen::bits::stmdb(rn, regs, writeback);
                 ++m_count;
                 return *this;
             }

             // ---- Multiply ----

             consteval counting_arm_macro_builder& mul(const arm_reg rd, const arm_reg rm,
                                                       const arm_reg rs) {
                 (void)::gba::codegen::bits::mul(rd, rm, rs);
                 ++m_count;
                 return *this;
             }

             consteval counting_arm_macro_builder& mla(const arm_reg rd, const arm_reg rm,
                                                       const arm_reg rs, const arm_reg rn) {
                 (void)::gba::codegen::bits::mla(rd, rm, rs, rn);
                 ++m_count;
                 return *this;
             }

             [[nodiscard]] consteval std::size_t mark() const noexcept { return m_count; }

            [[nodiscard]] consteval std::size_t arg_count() const noexcept { return m_argCount; }

        private:
            consteval void track_arg(const std::uint8_t arg) {
                if (static_cast<std::size_t>(arg + 1) > m_argCount) {
                    m_argCount = static_cast<std::size_t>(arg + 1);
                }
            }

            std::size_t m_count{};
            std::size_t m_argCount{};
        };

        template<typename Fn>
        consteval std::size_t infer_macro_capacity(Fn&& fn) {
            auto counter = counting_arm_macro_builder{};
            fn(counter);
            return counter.mark();
        }

    } // namespace bits

    template<typename Fn>
    consteval auto arm_macro(Fn&& fn) {
        constexpr std::size_t capacity = bits::infer_macro_capacity(std::forward<Fn>(fn));
        auto builder = arm_macro_builder<capacity>{};
        fn(builder);
        return builder.compile();
    }

} // namespace gba::codegen
