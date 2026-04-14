/// @file bits/codegen/patch_args.hpp
/// @brief Patch argument types and slot creation helpers for runtime-patchable codegen.
#pragma once

#include <gba/bits/codegen/encoder.hpp>

#include <cstddef>
#include <cstdint>

namespace gba::codegen {

    /// @brief Immediate (8-bit) argument type for patchable immediates.
    struct imm_arg {
        std::uint8_t position{};
    };

    /// @brief Signed 12-bit argument type for patchable load/store offsets.
    struct s12_arg {
        std::uint8_t position{};
    };

    /// @brief Branch offset argument type for patchable branch displacements.
    struct b_arg {
        std::uint8_t position{};
    };

    /// @brief Whole-instruction argument type for patchable full instruction words.
    struct instr_arg {
        std::uint8_t position{};
    };

    /// @brief Create a patchable immediate (8-bit) argument slot.
    /// @param n Slot position (0-31)
    /// @return imm_arg token for use in builder methods.
    consteval imm_arg imm_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "imm_slot: position out of range (0-31)");
        return imm_arg{static_cast<std::uint8_t>(n)};
    }

    /// @brief Create a patchable signed 12-bit argument slot.
    /// @param n Slot position (0-31)
    /// @return s12_arg token for use in builder methods.
    consteval s12_arg s12_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "s12_slot: position out of range (0-31)");
        return s12_arg{static_cast<std::uint8_t>(n)};
    }

    /// @brief Create a patchable branch offset argument slot.
    /// @param n Slot position (0-31)
    /// @return b_arg token for use in builder methods.
    consteval b_arg b_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "b_slot: position out of range (0-31)");
        return b_arg{static_cast<std::uint8_t>(n)};
    }

    /// @brief Create a patchable whole-instruction argument slot.
    /// @param n Slot position (0-31)
    /// @return instr_arg token for use in builder methods.
    consteval instr_arg instr_slot(const int n) {
        bits::require(n >= 0 && n <= 31, "instr_slot: position out of range (0-31)");
        return instr_arg{static_cast<std::uint8_t>(n)};
    }

    /// @brief Alias for whole-instruction patch slot.
    using word_arg = instr_arg;

    /// @brief Alias for whole-instruction literal patch slot.
    using literal_arg = instr_arg;

    /// @brief Create a patchable word (32-bit instruction) argument slot.
    /// @param n Slot position (0-31)
    /// @return word_arg token for use in builder methods.
    consteval word_arg word_slot(const int n) {
        return instr_slot(n);
    }

    /// @brief Create a patchable literal (32-bit) argument slot.
    /// @param n Slot position (0-31)
    /// @return literal_arg token for use in builder methods.
    consteval literal_arg literal_slot(const int n) {
        return instr_slot(n);
    }

    /// @brief Enumeration of patch kinds supported by the codegen system.
    enum class patch_kind : std::uint8_t {
        imm8,          ///< 8-bit immediate within instruction
        signed12,      ///< Signed 12-bit offset (load/store)
        branch_offset, ///< 24-bit signed branch displacement
        instruction,   ///< Full 32-bit instruction word
    };

    /// @brief Metadata for a single patchable slot within a compiled block.
    struct patch_slot {
        std::uint8_t word_index{}; ///< Word offset within the compiled block
        std::uint8_t arg_index{};  ///< Argument index for this patch
        patch_kind kind{};         ///< Type of patch (imm8, signed12, etc.)
        std::uint32_t base_word{}; ///< Base instruction word with patch site cleared
    };

} // namespace gba::codegen
