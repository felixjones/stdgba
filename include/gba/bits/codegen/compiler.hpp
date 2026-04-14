/// @file bits/codegen/compiler.hpp
/// @brief Builder and compiled-block types for gba::codegen.
#pragma once

#include <gba/bits/codegen/encoder.hpp>
#include <gba/bits/codegen/patch_args.hpp>

// Shared named-argument binder/value helpers ("name"_arg) live in the format layer.
// We depend only on binder/value types (no format parsing).
#include <gba/bits/format/args.hpp>

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

    template<typename... Regs>
    consteval std::uint16_t reg_list(Regs... regs) {
        static_assert((std::is_same_v<Regs, arm_reg> && ...), "reg_list: all arguments must be arm_reg");
        const auto mask = (0u | ... | (1u << bits::reg_bits(regs)));
        bits::require(mask != 0u, "reg_list: register list must not be empty");
        bits::require(mask <= 0xFFFFu, "reg_list: register list out of range");
        return static_cast<std::uint16_t>(mask);
    }

    // Checked instruction helpers (for whole-instruction patch slots)
    [[nodiscard]] constexpr arm_word nop_instr() noexcept {
        return arm_word{bits::nop()};
    }

    [[nodiscard]] constexpr arm_word add_reg_instr(const arm_reg rd, const arm_reg rn, const arm_reg rm) noexcept {
        return arm_word{bits::add_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word sub_reg_instr(const arm_reg rd, const arm_reg rn, const arm_reg rm) noexcept {
        return arm_word{bits::sub_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word orr_reg_instr(const arm_reg rd, const arm_reg rn, const arm_reg rm) noexcept {
        return arm_word{bits::orr_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word and_reg_instr(const arm_reg rd, const arm_reg rn, const arm_reg rm) noexcept {
        return arm_word{bits::and_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word eor_reg_instr(const arm_reg rd, const arm_reg rn, const arm_reg rm) noexcept {
        return arm_word{bits::eor_reg(rd, rn, rm)};
    }

    [[nodiscard]] constexpr arm_word lsl_imm_instr(const arm_reg rd, const arm_reg rm, const unsigned shift) noexcept {
        return arm_word{bits::lsl_imm(rd, rm, shift)};
    }

    [[nodiscard]] constexpr arm_word lsr_imm_instr(const arm_reg rd, const arm_reg rm, const unsigned shift) noexcept {
        return arm_word{bits::lsr_imm(rd, rm, shift)};
    }

    [[nodiscard]] constexpr arm_word mul_instr(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
        return arm_word{bits::mul(rd, rm, rs)};
    }

    // Expose raw encoders directly for constexpr usage
    using bits::adc_imm;
    using bits::adc_reg;
    using bits::and_imm;
    using bits::and_reg;
    using bits::asr_reg;
    using bits::bic_imm;
    using bits::bic_reg;
    using bits::blx;
    using bits::cmn_imm;
    using bits::cmn_reg;
    using bits::eor_imm;
    using bits::eor_reg;
    using bits::ldmda;
    using bits::ldmdb;
    using bits::ldmib;
    using bits::ldrb_imm;
    using bits::ldrb_reg;
    using bits::ldrh_reg;
    using bits::ldrsb_imm;
    using bits::ldrsb_reg;
    using bits::ldrsh_imm;
    using bits::ldrsh_reg;
    using bits::lsl_reg;
    using bits::lsr_reg;
    using bits::mla;
    using bits::mul;
    using bits::mvn_imm;
    using bits::mvn_reg;
    using bits::orr_imm;
    using bits::orr_reg;
    using bits::pop;
    using bits::push;
    using bits::ror_imm;
    using bits::ror_reg;
    using bits::rsb_imm;
    using bits::rsb_reg;
    using bits::sbc_imm;
    using bits::sbc_reg;
    using bits::stmda;
    using bits::stmdb;
    using bits::stmib;
    using bits::strb_imm;
    using bits::strb_reg;
    using bits::strh_reg;
    using bits::teq_imm;
    using bits::teq_reg;
    using bits::tst_imm;
    using bits::tst_reg;

    template<std::size_t Capacity>
    struct compiled_block;

    namespace bits {
        template<std::size_t PatchCount, std::size_t ArgCount>
        struct block_patcher;

        template<std::size_t PatchCount, std::size_t ArgCount, std::size_t Capacity>
        consteval block_patcher<PatchCount, ArgCount> make_block_patcher(const compiled_block<Capacity>& block);
    } // namespace bits

    template<std::size_t Capacity>
    struct compiled_block {
        std::array<arm_word, Capacity> words{};
        std::size_t count{};
        std::array<patch_slot, Capacity> patches{};
        std::size_t patch_count{};
        std::size_t arg_count{};
        std::uint8_t patch_kind_mask{};
        std::array<std::uint32_t, 32> arg_hashes{}; // 0 for positional, hash for named

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

        /// @brief Create an untyped patcher object for this block.
        ///
        /// The returned type is opaque (store as `auto`). It supports both:
        /// - positional patching: `patcher(dest, 1u, 2u, ...)`
        /// - named patching using the shared format literal: `patcher(dest, "x"_arg = 7u)`
        ///
        /// Use `patcher<Signature>()` to get a typed patcher whose `operator()`
        /// returns a function pointer of type `Signature*` directly.
        consteval auto patcher() const noexcept { return bits::make_block_patcher<Capacity, Capacity>(*this); }

        /// @brief Create a typed patcher object for this block.
        ///
        /// The call operator of the returned patcher applies all patches and then
        /// returns a raw function pointer of type `std::add_pointer_t<Signature>`:
        /// @code{.cpp}
        /// static constexpr auto tpl = arm_macro([](auto& b) {
        ///     b.mov_imm(arm_reg::r0, imm_slot(0)).bx(arm_reg::lr);
        /// });
        /// constexpr auto patch = tpl.patcher<int()>();
        ///
        /// std::memcpy(code, tpl.data(), tpl.size_bytes());
        /// auto fn = patch(code, 42u);  // returns int(*)(), applies patches
        /// @endcode
        template<typename Signature>
        consteval auto patcher() const noexcept {
            return bits::make_block_patcher<Capacity, Capacity>(*this).template typed<Signature>();
        }
    };

    /// @brief Compile-time builder for ARM instruction blocks with runtime patching support.
    ///
    /// Accumulates instructions into a compiled_block<Capacity> that can be materialized
    /// at runtime and patched with dynamic values. All builder methods are consteval,
    /// ensuring build-time validation of instruction encodings and patch metadata.
    ///
    /// Usage:
    /// @code {.cpp}
    /// static constexpr auto block = arm_macro_builder<4>{}.mov_imm(r0, 42).bx(lr).compile();
    /// @endcode
    ///
    /// Or via the consteval arm_macro() helper for automatic capacity inference:
    /// @code {.cpp}
    /// static constexpr auto block = arm_macro([](auto& b) {
    ///     b.mov_imm(r0, imm_slot(0)).bx(lr);
    /// });
    /// @endcode
    template<std::size_t Capacity>
    struct arm_macro_builder {
        /// @brief Load immediate into register.
        /// @param rd Destination register
        /// @param imm Immediate value (0-255 for ARM, rotated by even amounts)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& mov_imm(const arm_reg rd, const std::uint32_t imm) {
            push(bits::mov_imm(rd, imm));
            return *this;
        }

        /// @brief Move register to register.
        /// @param rd Destination register
        /// @param rm Source register
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& mov_reg(const arm_reg rd, const arm_reg rm) {
            push(bits::mov_reg(rd, rm));
            return *this;
        }

        /// @brief Logical shift left by immediate.
        /// @param rd Destination register
        /// @param rm Source register
        /// @param shift Shift amount (0-31 bits)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& lsl_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::lsl_imm(rd, rm, shift));
            return *this;
        }

        /// @brief Logical shift right by immediate.
        /// @param rd Destination register
        /// @param rm Source register
        /// @param shift Shift amount (1-32 bits)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& lsr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::lsr_imm(rd, rm, shift));
            return *this;
        }

        /// @brief Arithmetic shift right by immediate.
        /// @param rd Destination register
        /// @param rm Source register
        /// @param shift Shift amount (1-32 bits)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& asr_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::asr_imm(rd, rm, shift));
            return *this;
        }

        /// @brief Load patchable immediate into register.
        ///
        /// Creates a patch slot for runtime value assignment. The immediate value
        /// will be filled in during apply_patches() call.
        ///
        /// @param rd Destination register
        /// @param arg Patch slot token from imm_slot(n)
        /// @return Reference to builder for chaining
        /// @see imm_slot() - Create a patchable immediate slot
        consteval arm_macro_builder& mov_imm(const arm_reg rd, const imm_arg arg) {
            push_with_patch(bits::mov_imm(rd, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        /// @brief Load named patchable immediate into register.
        ///
        /// Creates a named patch slot for runtime value assignment via named arguments
        /// (e.g., `"value"_arg = 42`). The name becomes part of the patch metadata.
        ///
        /// @tparam Name Compile-time fixed string (from "name"_arg syntax)
        /// @param rd Destination register
        /// @return Reference to builder for chaining
        /// @see gba::literals::operator""_arg - Create named patch arguments
        template<::gba::format::fixed_string Name>
        [[deprecated("Use positional imm_slot(n) instead; named args reduce specialization efficiency")]]
        consteval arm_macro_builder& mov_imm(const arm_reg rd, const ::gba::format::arg_binder<Name>) {
            const auto idx = named_arg_index(static_cast<std::uint32_t>(::gba::format::arg_binder<Name>::hash));
            push_with_patch(bits::mov_imm(rd, 0), patch_kind::imm8, idx);
            return *this;
        }

        /// @brief Add immediate to register.
        /// @param rd Destination register (modified by operation)
        /// @param rn Source/left operand register
        /// @param imm Immediate value (0-255 for ARM, rotated by even amounts)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::add_imm(rd, rn, imm));
            return *this;
        }

        /// @brief Add register to register.
        /// @param rd Destination register (modified by operation)
        /// @param rn Source/left operand register
        /// @param rm Source/right operand register
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& add_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::add_reg(rd, rn, rm));
            return *this;
        }

        /// @brief Bitwise OR register into register.
        /// @param rd Destination register
        /// @param rn Source/left operand register
        /// @param rm Source/right operand register
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& orr_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::orr_reg(rd, rn, rm));
            return *this;
        }

        /// @brief Add patchable immediate to register.
        ///
        /// Creates a patch slot for runtime value assignment. The immediate will be
        /// filled in during apply_patches() call.
        ///
        /// @param rd Destination register
        /// @param rn Source/left operand register
        /// @param arg Patch slot token from imm_slot(n)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::add_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        template<::gba::format::fixed_string Name>
        consteval arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn,
                                             const ::gba::format::arg_binder<Name>) {
            const auto idx = named_arg_index(static_cast<std::uint32_t>(::gba::format::arg_binder<Name>::hash));
            push_with_patch(bits::add_imm(rd, rn, 0), patch_kind::imm8, idx);
            return *this;
        }

        /// @brief Subtract immediate from register.
        /// @param rd Destination register (modified by operation)
        /// @param rn Source/left operand register
        /// @param imm Immediate value (0-255 for ARM, rotated by even amounts)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::sub_imm(rd, rn, imm));
            return *this;
        }

        /// @brief Subtract register from register.
        /// @param rd Destination register (modified by operation)
        /// @param rn Source/left operand register
        /// @param rm Source/right operand register (subtracted from rn)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& sub_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::sub_reg(rd, rn, rm));
            return *this;
        }

        /// @brief Subtract patchable immediate from register.
        ///
        /// Creates a patch slot for runtime value assignment.
        ///
        /// @param rd Destination register
        /// @param rn Source/left operand register
        /// @param arg Patch slot token from imm_slot(n)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::sub_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        template<::gba::format::fixed_string Name>
        consteval arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn,
                                             const ::gba::format::arg_binder<Name>) {
            const auto idx = named_arg_index(static_cast<std::uint32_t>(::gba::format::arg_binder<Name>::hash));
            push_with_patch(bits::sub_imm(rd, rn, 0), patch_kind::imm8, idx);
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

        /// @brief Compare register with immediate (set flags).
        ///
        /// Performs subtraction and updates the CPSR flags without storing the result.
        /// Commonly used before conditional branches to test values.
        ///
        /// @param rn Register to compare
        /// @param imm Immediate value to compare against
        /// @return Reference to builder for chaining
        /// @see b_if() - Branch based on comparison result
        consteval arm_macro_builder& cmp_imm(const arm_reg rn, const std::uint32_t imm) {
            push(bits::cmp_imm(rn, imm));
            return *this;
        }

        /// @brief Compare two registers (set flags).
        ///
        /// Performs subtraction and updates the CPSR flags without storing the result.
        ///
        /// @param rn First register to compare
        /// @param rm Second register to compare
        /// @return Reference to builder for chaining
        /// @see b_if() - Branch based on comparison result
        consteval arm_macro_builder& cmp_reg(const arm_reg rn, const arm_reg rm) {
            push(bits::cmp_reg(rn, rm));
            return *this;
        }

        /// @brief Compare register with patchable immediate (set flags).
        ///
        /// Creates a patch slot for runtime comparison value assignment.
        ///
        /// @param rn Register to compare
        /// @param arg Patch slot token from imm_slot(n)
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& cmp_imm(const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::cmp_imm(rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& str_imm(const arm_reg rd, const arm_reg rn, const s12_arg arg) {
            push_with_patch(bits::str_imm(rd, rn, 0), patch_kind::signed12, arg.position);
            return *this;
        }

        /// @brief Branch exchange (jump to register).
        ///
        /// Jumps to the address in the specified register. Commonly used to return
        /// from a function (via `bx(lr)`) or to implement indirect function calls.
        ///
        /// @param rm Register containing target address
        /// @return Reference to builder for chaining
        /// @note On ARM7TDMI, this also switches execution mode (ARM/Thumb)
        consteval arm_macro_builder& bx(const arm_reg rm) {
            push(bits::bx(rm));
            return *this;
        }

        /// @brief Branch exchange with link (call via register).
        ///
        /// Jumps to the address in the specified register while saving the return
        /// address in LR. Implements indirect function calls.
        ///
        /// @param rm Register containing target address
        /// @return Reference to builder for chaining
        /// @note On ARM7TDMI, this also switches execution mode (ARM/Thumb)
        consteval arm_macro_builder& blx(const arm_reg rm) {
            push(bits::blx(rm));
            return *this;
        }

        /// @brief Branch to immediate target (compile-time).
        ///
        /// Branches to a fixed instruction index known at compile time.
        /// For runtime-determined targets, use b_to(b_arg) instead.
        ///
        /// @param target_index Destination instruction index (from mark())
        /// @return Reference to builder for chaining
        /// @see mark() - Record an instruction index for later branching
        consteval arm_macro_builder& b_to(const std::size_t target_index) {
            push(bits::b_to(m_count, target_index));
            return *this;
        }

        /// @brief Branch with link to immediate target (compile-time).
        ///
        /// Branches to a fixed instruction index while saving the return address in LR.
        /// @param target_index Destination instruction index (from mark())
        /// @return Reference to builder for chaining
        consteval arm_macro_builder& bl_to(const std::size_t target_index) {
            push(bits::bl_to(m_count, target_index));
            return *this;
        }

        /// @brief Conditional branch to immediate target (compile-time).
        ///
        /// Branches only if the condition code is satisfied.
        /// Most commonly used after a cmp, tst, or arithmetic instruction.
        ///
        /// @param cond Condition code (e.g., arm_cond::eq, arm_cond::ne)
        /// @param target_index Destination instruction index
        /// @return Reference to builder for chaining
        /// @see cmp_imm(), cmp_reg(), tst_imm(), tst_reg() - Set condition flags
        consteval arm_macro_builder& b_if(const arm_cond cond, const std::size_t target_index) {
            push(bits::b_if_to(cond, m_count, target_index));
            return *this;
        }

        /// @brief Branch with patchable offset (runtime).
        ///
        /// Creates a patch slot for runtime branch target assignment. Commonly used
        /// when the target address is not known at compile time but determined during
        /// code generation or execution.
        ///
        /// @param arg Patch slot token from b_slot(n)
        /// @return Reference to builder for chaining
        /// @see b_slot() - Create a branch offset patch slot
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

        consteval arm_macro_builder& word(const word_arg arg) { return instruction(instr_arg{arg.position}); }

        consteval arm_macro_builder& literal_word(const literal_arg arg) {
            return instruction(instr_arg{arg.position});
        }

        // ---- Bitwise ----

        consteval arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::orr_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::orr_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::and_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::and_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& and_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::and_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::eor_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::eor_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& eor_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::eor_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::bic_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::bic_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& bic_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::bic_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& mvn_imm(const arm_reg rd, const std::uint32_t imm) {
            push(bits::mvn_imm(rd, imm));
            return *this;
        }

        consteval arm_macro_builder& mvn_imm(const arm_reg rd, const imm_arg arg) {
            push_with_patch(bits::mvn_imm(rd, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& mvn_reg(const arm_reg rd, const arm_reg rm) {
            push(bits::mvn_reg(rd, rm));
            return *this;
        }

        // ---- Additional arithmetic ----

        consteval arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::rsb_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::rsb_imm(rd, rn, 0), patch_kind::imm8, arg.position);
            return *this;
        }

        consteval arm_macro_builder& rsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::rsb_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& adc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::adc_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& adc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::adc_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& sbc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
            push(bits::sbc_imm(rd, rn, imm));
            return *this;
        }

        consteval arm_macro_builder& sbc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::sbc_reg(rd, rn, rm));
            return *this;
        }

        // ---- Shifts ----

        consteval arm_macro_builder& ror_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
            push(bits::ror_imm(rd, rm, shift));
            return *this;
        }

        consteval arm_macro_builder& lsl_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
            push(bits::lsl_reg(rd, rm, rs));
            return *this;
        }

        consteval arm_macro_builder& lsr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
            push(bits::lsr_reg(rd, rm, rs));
            return *this;
        }

        consteval arm_macro_builder& asr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
            push(bits::asr_reg(rd, rm, rs));
            return *this;
        }

        consteval arm_macro_builder& ror_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
            push(bits::ror_reg(rd, rm, rs));
            return *this;
        }

        // ---- Byte memory ----

        consteval arm_macro_builder& ldrb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::ldrb_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& strb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::strb_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldrb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::ldrb_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& strb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::strb_reg(rd, rn, rm));
            return *this;
        }

        // ---- Halfword / signed-byte memory ----

        consteval arm_macro_builder& ldrh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::ldrh_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& strh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::strh_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& ldrsb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::ldrsb_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldrsh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
            push(bits::ldrsh_imm(rd, rn, offset));
            return *this;
        }

        consteval arm_macro_builder& ldrsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
            push(bits::ldrsb_reg(rd, rn, rm));
            return *this;
        }

        consteval arm_macro_builder& ldrsh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
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

        consteval arm_macro_builder& tst_imm(const arm_reg rn, const imm_arg arg) {
            push_with_patch(bits::tst_imm(rn, 0), patch_kind::imm8, arg.position);
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

        consteval arm_macro_builder& ldmib(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::ldmib(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& ldmda(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::ldmda(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& ldmdb(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::ldmdb(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& stmib(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::stmib(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& stmda(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::stmda(rn, regs, writeback));
            return *this;
        }

        consteval arm_macro_builder& stmdb(const arm_reg rn, const std::uint16_t regs, const bool writeback = false) {
            push(bits::stmdb(rn, regs, writeback));
            return *this;
        }

        // ---- Multiply ----

        consteval arm_macro_builder& mul(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
            push(bits::mul(rd, rm, rs));
            return *this;
        }

        consteval arm_macro_builder& mla(const arm_reg rd, const arm_reg rm, const arm_reg rs, const arm_reg rn) {
            push(bits::mla(rd, rm, rs, rn));
            return *this;
        }

        /// @brief Record current instruction count as a branch target.
        ///
        /// Useful for constructing conditional branches and loops. The returned value
        /// can be passed to b_if_to() for runtime-conditional jumps.
        ///
        /// @return Current number of instructions in the block
        [[nodiscard]] consteval std::size_t mark() const noexcept { return m_count; }

        /// @brief Finalize and compile the instruction block.
        ///
        /// Generates a compiled_block<Capacity> containing all accumulated instructions,
        /// patch metadata, and argument information. This is the final step before
        /// materializing code to memory.
        ///
        /// @return compiled_block<Capacity> ready for materialization
        [[nodiscard]] consteval compiled_block<Capacity> compile() const noexcept {
            return {m_words, m_count, m_patches, m_patchCount, m_argCount, m_patchKindMask, m_argHashes};
        }

    private:
        std::array<arm_word, Capacity> m_words{};
        std::size_t m_count{};
        std::array<patch_slot, Capacity> m_patches{};
        std::size_t m_patchCount{};
        std::size_t m_argCount{};
        std::uint8_t m_patchKindMask{};
        std::array<std::uint32_t, 32> m_argHashes{};
        std::array<std::uint32_t, 32> m_namedHashes{};
        std::uint8_t m_namedBase{0xFFu};
        std::uint8_t m_namedCount{};

        [[nodiscard]] static consteval std::uint32_t patch_base(const std::uint32_t word, const patch_kind kind) {
            switch (kind) {
                case patch_kind::imm8: return word & 0xFFFFFF00u;
                case patch_kind::signed12: return word & ~(0xFFFu | (1u << 23u));
                case patch_kind::branch_offset: return word & 0xFF000000u;
                case patch_kind::instruction: return 0u;
            }
            return word;
        }

        consteval void emit(const std::uint32_t word) {
            bits::require(m_count < Capacity, "arm_macro_builder: instruction capacity exceeded");
            m_words[m_count++] = arm_word{word};
        }

        consteval std::uint8_t named_arg_index(const std::uint32_t hash) {
            bits::require(hash != 0u, "arm_macro_builder: named arg hash must be nonzero");

            if (m_namedBase == 0xFFu) {
                // Start named args after the highest positional arg used so far.
                bits::require(m_argCount <= 32u, "arm_macro_builder: too many patch args");
                m_namedBase = static_cast<std::uint8_t>(m_argCount);
            }

            for (std::uint8_t i = 0; i < m_namedCount; ++i) {
                if (m_namedHashes[i] == hash) {
                    return static_cast<std::uint8_t>(m_namedBase + i);
                }
            }

            bits::require(m_namedCount < 32u, "arm_macro_builder: too many named args");
            const auto idx = static_cast<std::uint8_t>(m_namedBase + m_namedCount);
            bits::require(idx <= 31u, "arm_macro_builder: too many patch args (max 32)");

            m_namedHashes[m_namedCount++] = hash;
            m_argHashes[idx] = hash;
            if (static_cast<std::size_t>(idx + 1u) > m_argCount) {
                m_argCount = static_cast<std::size_t>(idx + 1u);
            }
            return idx;
        }

        consteval void push_with_patch(const std::uint32_t word, const patch_kind kind, const std::uint8_t argIndex) {
            bits::require(m_patchCount < Capacity, "arm_macro_builder: patch capacity exceeded");
            bits::require(argIndex <= 31, "arm_macro_builder: patch arg out of range (0-31)");
            bits::require(m_count < Capacity, "arm_macro_builder: instruction capacity exceeded");

            // Positional args have arg_hash = 0.
            if (m_argHashes[argIndex] == 0u) {
                m_argHashes[argIndex] = 0u;
            }

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

            template<::gba::format::fixed_string Name>
            consteval counting_arm_macro_builder& mov_imm(const arm_reg rd, const ::gba::format::arg_binder<Name>) {
                (void)::gba::codegen::bits::mov_imm(rd, 0);
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

            template<::gba::format::fixed_string Name>
            consteval counting_arm_macro_builder& add_imm(const arm_reg rd, const arm_reg rn,
                                                          const ::gba::format::arg_binder<Name>) {
                (void)::gba::codegen::bits::add_imm(rd, rn, 0);
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

            template<::gba::format::fixed_string Name>
            consteval counting_arm_macro_builder& sub_imm(const arm_reg rd, const arm_reg rn,
                                                          const ::gba::format::arg_binder<Name>) {
                (void)::gba::codegen::bits::sub_imm(rd, rn, 0);
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

            consteval counting_arm_macro_builder& cmp_imm(const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::cmp_imm(rn, 0);
                track_arg(arg.position);
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

            consteval counting_arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::orr_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& orr_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::orr_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::and_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& and_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::and_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& and_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::and_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::eor_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& eor_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::eor_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& eor_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::eor_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::bic_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& bic_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::bic_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& bic_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::bic_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mvn_imm(const arm_reg rd, const std::uint32_t imm) {
                (void)::gba::codegen::bits::mvn_imm(rd, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mvn_imm(const arm_reg rd, const imm_arg arg) {
                (void)::gba::codegen::bits::mvn_imm(rd, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mvn_reg(const arm_reg rd, const arm_reg rm) {
                (void)::gba::codegen::bits::mvn_reg(rd, rm);
                ++m_count;
                return *this;
            }

            // ---- Additional arithmetic ----

            consteval counting_arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::rsb_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& rsb_imm(const arm_reg rd, const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::rsb_imm(rd, rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& rsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::rsb_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& adc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::adc_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& adc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::adc_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& sbc_imm(const arm_reg rd, const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::sbc_imm(rd, rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& sbc_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::sbc_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            // ---- Shifts ----

            consteval counting_arm_macro_builder& ror_imm(const arm_reg rd, const arm_reg rm, const unsigned shift) {
                (void)::gba::codegen::bits::ror_imm(rd, rm, shift);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& lsl_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
                (void)::gba::codegen::bits::lsl_reg(rd, rm, rs);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& lsr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
                (void)::gba::codegen::bits::lsr_reg(rd, rm, rs);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& asr_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
                (void)::gba::codegen::bits::asr_reg(rd, rm, rs);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ror_reg(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
                (void)::gba::codegen::bits::ror_reg(rd, rm, rs);
                ++m_count;
                return *this;
            }

            // ---- Byte memory ----

            consteval counting_arm_macro_builder& ldrb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::ldrb_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& strb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::strb_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::ldrb_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& strb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::strb_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            // ---- Halfword / signed-byte memory ----

            consteval counting_arm_macro_builder& ldrh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::ldrh_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& strh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::strh_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrsb_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::ldrsb_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrsh_imm(const arm_reg rd, const arm_reg rn, const int offset) {
                (void)::gba::codegen::bits::ldrsh_imm(rd, rn, offset);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrsb_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::ldrsb_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldrsh_reg(const arm_reg rd, const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::ldrsh_reg(rd, rn, rm);
                ++m_count;
                return *this;
            }

            // ---- Comparison / flag setters ----

            consteval counting_arm_macro_builder& cmn_imm(const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::cmn_imm(rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& cmn_reg(const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::cmn_reg(rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& tst_imm(const arm_reg rn, const std::uint32_t imm) {
                (void)::gba::codegen::bits::tst_imm(rn, imm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& tst_imm(const arm_reg rn, const imm_arg arg) {
                (void)::gba::codegen::bits::tst_imm(rn, 0);
                track_arg(arg.position);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& tst_reg(const arm_reg rn, const arm_reg rm) {
                (void)::gba::codegen::bits::tst_reg(rn, rm);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& teq_imm(const arm_reg rn, const std::uint32_t imm) {
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

            consteval counting_arm_macro_builder& ldmib(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::ldmib(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldmda(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::ldmda(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& ldmdb(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::ldmdb(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& stmib(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::stmib(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& stmda(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::stmda(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& stmdb(const arm_reg rn, const std::uint16_t regs,
                                                        const bool writeback = false) {
                (void)::gba::codegen::bits::stmdb(rn, regs, writeback);
                ++m_count;
                return *this;
            }

            // ---- Multiply ----

            consteval counting_arm_macro_builder& mul(const arm_reg rd, const arm_reg rm, const arm_reg rs) {
                (void)::gba::codegen::bits::mul(rd, rm, rs);
                ++m_count;
                return *this;
            }

            consteval counting_arm_macro_builder& mla(const arm_reg rd, const arm_reg rm, const arm_reg rs,
                                                      const arm_reg rn) {
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

    /// @brief Compile an ARM instruction macro with automatic capacity deduction.
    ///
    /// Main entry point for building ARM instruction blocks. Automatically determines
    /// the required capacity by running the builder in a counting pass, then compiles
    /// a block with that exact size.
    ///
    /// Usage:
    /// @code{.cpp}
    /// static constexpr auto add_immediate = arm_macro([](auto& b) {
    ///     b.add_imm(arm_reg::r0, arm_reg::r0, imm_slot(0))
    ///     .bx(arm_reg::lr);
    /// });
    ///
    /// std::memcpy(dest_code,add_immediate.data(), add_immediate.size_bytes());
    /// auto fn = apply_patches<int(int)>(add_immediate, dest_code, add_immediate.size(), 10u);
    /// int result = fn(5);  // returns 15 (5 + 10)
    /// @endcode
    ///
    /// @tparam Fn Callable type (usually a lambda) that accepts `arm_macro_builder<N>&`
    /// @param fn Callable that builds instructions via the builder
    /// @return `compiled_block<Capacity>` ready for materialization and patching
    /// @see compiled_block - The returned instruction block type
    /// @see apply_patches() - Apply runtime patch values
    /// @see block_patcher - Zero-overhead compile-time patcher
    template<typename Fn>
    consteval auto arm_macro(Fn&& fn) {
        constexpr std::size_t capacity = bits::infer_macro_capacity(std::forward<Fn>(fn));
        auto builder = arm_macro_builder<capacity>{};
        fn(builder);
        return builder.compile();
    }

    namespace bits {
        template<std::size_t PatchCount, std::size_t ArgCount>
        struct block_patcher {
            std::array<patch_slot, PatchCount> patches{};
            std::array<std::uint32_t, ArgCount> arg_hashes{};
            /// Actual number of patch slots in use (≤ PatchCount).
            std::size_t actual_patches{PatchCount};
            /// Actual number of arg slots in use (≤ ArgCount).
            std::size_t actual_args{ArgCount};

        private:
            template<typename T>
            [[nodiscard]] static constexpr bool is_named_arg() noexcept {
                return requires { std::remove_reference_t<T>::hash; }&&
                    requires(const std::remove_reference_t<T>& v)
                {
                    v.get();
                };
            }

            template<typename T>
            [[nodiscard]] static constexpr bool is_patch_arg_convertible() noexcept {
                if constexpr (requires(const std::remove_reference_t<T>& v) { v.get(); }) {
                    using got_t = decltype(std::declval<const std::remove_reference_t<T>&>().get());
                    return std::is_convertible_v<got_t, std::uint32_t>;
                } else {
                    return std::is_convertible_v<T, std::uint32_t>;
                }
            }

            template<typename T>
            [[nodiscard]] static constexpr std::uint32_t normalize_arg(T&& arg) noexcept {
                if constexpr (requires(const std::remove_reference_t<T>& v) { v.get(); }) {
                    return static_cast<std::uint32_t>(arg.get());
                } else {
                    return static_cast<std::uint32_t>(std::forward<T>(arg));
                }
            }

            [[nodiscard]] constexpr std::size_t find_named_index(const std::uint32_t hash) const noexcept {
                for (std::size_t i = 0; i < actual_args; ++i) {
                    if (arg_hashes[i] == hash) return i;
                }
                return static_cast<std::size_t>(-1);
            }

            [[nodiscard]] constexpr std::size_t positional_needed() const noexcept {
                std::size_t n = 0;
                for (std::size_t i = 0; i < actual_args; ++i) {
                    if (arg_hashes[i] == 0u) ++n;
                }
                return n;
            }

            template<typename... Ts>
            [[nodiscard]] static consteval bool no_duplicate_named() {
                // Compile-time duplicate detection: for each named arg type, count how many
                // other arg types share the same hash.  Uses if constexpr so that ::hash is
                // only accessed when the type actually satisfies is_named_arg<T>.
                bool ok = true;
                auto check_one = [&]<typename T>() {
                    if constexpr (is_named_arg<T>()) {
                        constexpr auto h = static_cast<std::uint32_t>(std::remove_reference_t<T>::hash);
                        std::size_t n = 0;
                        auto count_h = [&]<typename U>() {
                            if constexpr (is_named_arg<U>()) {
                                if constexpr (static_cast<std::uint32_t>(std::remove_reference_t<U>::hash) == h) {
                                    ++n;
                                }
                            }
                        };
                        (count_h.template operator()<Ts>(), ...);
                        if (n > 1) ok = false;
                    }
                };
                (check_one.template operator()<Ts>(), ...);
                return ok;
            }

        public:
            /// @brief A typed variant whose `operator()` returns a function pointer of type
            ///        `std::add_pointer_t<Signature>` after applying all patches.
            template<typename Signature>
            struct typed_result {
                block_patcher<PatchCount, ArgCount> inner;

                template<typename... InnerArgs>
                [[gnu::always_inline]] std::add_pointer_t<Signature> operator()(std::uint32_t* dest,
                                                                                InnerArgs&&... args) const noexcept {
                    return inner.template entry<Signature>(dest, std::forward<InnerArgs>(args)...);
                }
            };

            /// @brief Return a typed patcher whose `operator()` returns
            ///        `std::add_pointer_t<Signature>` after applying patches.
            template<typename Signature>
            [[nodiscard]] constexpr typed_result<Signature> typed() const noexcept {
                return {*this};
            }

            template<typename... Args>
            [[gnu::always_inline]] void operator()(std::uint32_t* dest, Args&&... args) const noexcept {
                static_assert((is_patch_arg_convertible<Args>() && ...),
                              "patcher: args must be convertible to uint32_t or provide get() -> uint32_t");
                static_assert(no_duplicate_named<Args...>(), "patcher: duplicate named arg provided");

                std::array<std::uint32_t, ArgCount> vals{};
                std::array<bool, ArgCount> filled{};
                std::size_t pos_cursor = 0;

                auto bind_positional = [&](const std::uint32_t value) {
                    while (pos_cursor < actual_args && arg_hashes[pos_cursor] != 0u) {
                        ++pos_cursor;
                    }
                    bits::require(pos_cursor < actual_args, "patcher: too many positional args");
                    vals[pos_cursor] = value;
                    filled[pos_cursor] = true;
                    ++pos_cursor;
                };

                auto bind_one = [&](auto&& a) {
                    using arg_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (is_named_arg<arg_t>()) {
                        constexpr auto h = static_cast<std::uint32_t>(arg_t::hash);
                        const auto idx = find_named_index(h);
                        bits::require(idx != static_cast<std::size_t>(-1), "patcher: unknown named arg provided");
                        vals[idx] = normalize_arg(std::forward<decltype(a)>(a));
                        filled[idx] = true;
                    } else {
                        bind_positional(normalize_arg(std::forward<decltype(a)>(a)));
                    }
                };

                (bind_one(std::forward<Args>(args)), ...);

                // Validate all args were provided.
                for (std::size_t i = 0; i < actual_args; ++i) {
                    bits::require(filled[i], "patcher: missing patch argument");
                }

                // Apply patches (fully unrolled).
                for (std::size_t i = 0; i < actual_patches; ++i) {
                    const auto& p = patches[i];
                    const auto value = vals[p.arg_index];
                    switch (p.kind) {
                        case patch_kind::imm8:
                            bits::require(value <= 0xFFu, "patcher: imm8 value out of range");
                            dest[p.word_index] = p.base_word | value;
                            break;
                        case patch_kind::signed12: {
                            const auto sv = static_cast<std::int32_t>(value);
                            if (sv < 0) {
                                bits::require(sv >= -4095, "patcher: signed12 value out of range");
                                dest[p.word_index] = p.base_word | static_cast<std::uint32_t>(-sv);
                            } else {
                                bits::require(sv <= 4095, "patcher: signed12 value out of range");
                                dest[p.word_index] = p.base_word | 0x00800000u | static_cast<std::uint32_t>(sv);
                            }
                            break;
                        }
                        case patch_kind::branch_offset: {
                            const auto sv = static_cast<std::int32_t>(value);
                            bits::require(sv >= -0x00800000 && sv <= 0x007FFFFF, "patcher: branch offset out of range");
                            dest[p.word_index] = p.base_word | (value & 0x00FFFFFFu);
                            break;
                        }
                        case patch_kind::instruction: dest[p.word_index] = value; break;
                    }
                }
            }

            template<typename Signature, typename... Args>
            [[gnu::always_inline]] std::add_pointer_t<Signature> entry(std::uint32_t* dest,
                                                                       Args&&... args) const noexcept {
                (*this)(dest, std::forward<Args>(args)...);
                return reinterpret_cast<std::add_pointer_t<Signature>>(dest);
            }
        };

        template<std::size_t PatchCount, std::size_t ArgCount, std::size_t Capacity>
        consteval block_patcher<PatchCount, ArgCount> make_block_patcher(const compiled_block<Capacity>& block) {
            block_patcher<PatchCount, ArgCount> p{};
            p.actual_patches = block.patch_count;
            p.actual_args = block.arg_count;
            const auto n_patches = block.patch_count < PatchCount ? block.patch_count : PatchCount;
            for (std::size_t i = 0; i < n_patches; ++i) {
                p.patches[i] = block.patches[i];
            }
            const auto n_args = block.arg_count < ArgCount ? block.arg_count : ArgCount;
            for (std::size_t i = 0; i < n_args; ++i) {
                p.arg_hashes[i] = block.arg_hashes[i];
            }
            return p;
        }
    } // namespace bits

    /// @brief Zero-overhead compile-time patcher for a `compiled_block`.
    ///
    /// Parameterised on the block value as a non-type template argument, so every
    /// piece of patch metadata (word index, base word, patch kind, arg index) is a
    /// compile-time constant.  `operator()` expands into a straight-line sequence of
    /// stores — no loop, no dispatch table, no ROM metadata loads.
    ///
    /// Usage:
    /// @code{.cpp}
    /// static constexpr auto tpl = arm_macro([](auto& b) {
    ///     b.mov_imm(arm_reg::r0, imm_slot(0)).bx(arm_reg::lr);
    /// });
    ///
    /// constexpr block_patcher<tpl> patch{};
    ///
    /// // Apply patches (in-place, zero overhead):
    /// patch(code, 42u);
    ///
    /// // Apply and get a typed function pointer:
    /// auto fn = patch.entry<int()>(code, 42u);
    /// @endcode
    template<auto Block>
    struct block_patcher {
    private:
        // Apply a single patch slot I, with all metadata as compile-time constants.
        template<std::size_t I>
        [[gnu::always_inline]] static void apply_slot(std::uint32_t* dest, const std::uint32_t* vals) noexcept {
            constexpr std::size_t wi = Block.patches[I].word_index;
            constexpr std::uint32_t bw = Block.patches[I].base_word;
            constexpr patch_kind kind = Block.patches[I].kind;
            constexpr std::size_t ai = Block.patches[I].arg_index;
            const auto value = vals[ai];

            if constexpr (kind == patch_kind::imm8) {
                bits::require(value <= 0xFFu, "block_patcher: imm8 value out of range");
                dest[wi] = bw | value;
            } else if constexpr (kind == patch_kind::signed12) {
                const auto sv = static_cast<std::int32_t>(value);
                if (sv < 0) {
                    bits::require(sv >= -4095, "block_patcher: signed12 value out of range");
                    dest[wi] = bw | static_cast<std::uint32_t>(-sv);
                } else {
                    bits::require(sv <= 4095, "block_patcher: signed12 value out of range");
                    dest[wi] = bw | 0x00800000u | static_cast<std::uint32_t>(sv);
                }
            } else if constexpr (kind == patch_kind::branch_offset) {
                const auto sv = static_cast<std::int32_t>(value);
                bits::require(sv >= -0x00800000 && sv <= 0x007FFFFF, "block_patcher: branch offset out of range");
                dest[wi] = bw | (value & 0x00FFFFFFu);
            } else {
                // instruction / word32: write full word directly
                dest[wi] = value;
            }
        }

        // Fold-expand over all patch indices.
        template<std::size_t... Is>
        [[gnu::always_inline]] static void apply_all(std::uint32_t* dest, const std::uint32_t* vals,
                                                     std::index_sequence<Is...>) noexcept {
            (apply_slot<Is>(dest, vals), ...);
        }

        template<typename T>
        static constexpr bool is_patch_arg_convertible = [] static {
            if constexpr (requires(const std::remove_reference_t<T>& v) { v.get(); }) {
                using got_t = decltype(std::declval<const std::remove_reference_t<T>&>().get());
                return std::is_convertible_v<got_t, std::uint32_t>;
            } else {
                return std::is_convertible_v<T, std::uint32_t>;
            }
        }();

        template<typename T>
        [[nodiscard]] static constexpr std::uint32_t normalize_patch_arg(T&& arg) {
            if constexpr (requires(const std::remove_reference_t<T>& v) { v.get(); }) {
                return static_cast<std::uint32_t>(arg.get());
            } else {
                return static_cast<std::uint32_t>(std::forward<T>(arg));
            }
        }

    public:
        /// @brief A typed variant whose `operator()` returns a function pointer of type
        ///        `std::add_pointer_t<Signature>` after applying all patches.
        ///
        /// Obtain via `block_patcher<Block>{}.typed<Signature>()` or, for zero-overhead
        /// NTT usage, store as `constexpr auto patch = block_patcher<tpl>{}.typed<int()>()`.
        template<typename Signature>
        struct typed_result {
            template<typename... InnerArgs>
            [[gnu::always_inline]] std::add_pointer_t<Signature> operator()(std::uint32_t* dest,
                                                                            InnerArgs&&... args) const noexcept {
                return block_patcher<Block>{}.template entry<Signature>(dest, std::forward<InnerArgs>(args)...);
            }
        };

        /// @brief Return a typed patcher whose `operator()` returns
        ///        `std::add_pointer_t<Signature>` after applying all patches.
        ///
        /// Example:
        /// @code{.cpp}
        /// static constexpr auto tpl = arm_macro([](auto& b) { ... });
        /// constexpr auto patch = block_patcher<tpl>{}.typed<int()>();
        /// auto fn = patch(code, 42u);   // returns int(*)()
        /// @endcode
        template<typename Signature>
        [[nodiscard]] static constexpr typed_result<Signature> typed() noexcept {
            return {};
        }

        /// @brief Apply all patches to `dest`. Args are runtime; all patch
        ///        metadata is resolved at compile time.
        template<typename... Args>
        [[gnu::always_inline]] void operator()(std::uint32_t* dest, Args&&... args) const noexcept {
            static_assert((is_patch_arg_convertible<Args> && ...),
                          "block_patcher: args must be convertible to uint32_t or provide get() -> uint32_t");
            static_assert(sizeof...(Args) >= Block.arg_count, "block_patcher: too few patch arguments");
            const std::array<std::uint32_t, sizeof...(Args)> vals{normalize_patch_arg(std::forward<Args>(args))...};
            apply_all(dest, vals.data(), std::make_index_sequence<Block.patch_count>{});
        }

        /// @brief Apply patches and return a typed function pointer to `dest`.
        ///        Suitable for use as a tail-call target after patching.
        template<typename Signature, typename... Args>
        [[gnu::always_inline]] std::add_pointer_t<Signature> entry(std::uint32_t* dest, Args&&... args) const noexcept {
            (*this)(dest, std::forward<Args>(args)...);
            return reinterpret_cast<std::add_pointer_t<Signature>>(dest);
        }
    };

} // namespace gba::codegen
