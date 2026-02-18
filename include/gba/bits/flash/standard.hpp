/**
 * @file bits/flash/standard.hpp
 * @brief Layer 1: Compiled command sequences for standard Flash chips.
 *
 * Provides a consteval command compiler for standard (non-Atmel) Flash
 * chips (Macronix, Panasonic, SST, Sanyo). Commands are validated at
 * compile time via consteval and produce an execute() method that the
 * compiler can fully unroll and devirtualize at -O2.
 *
 * Standard chips require erase-before-write — the compiler enforces
 * this at compile time and emits a diagnostic on violation.
 *
 * Commands can use compile-time constants or runtime placeholders:
 * @code{.cpp}
 * #include <gba/bits/flash/standard.hpp>
 *
 * namespace sf = gba::flash::standard;
 *
 * void fill(sf::sector_span buf) { buf[0] = 0x42; }
 * void recv(sf::const_sector_span buf) { g_val = buf[0]; }
 *
 * // Fully static — sector 0 hardcoded
 * constexpr auto static_cmds = sf::compile(
 *     sf::erase_sector(0),
 *     sf::write_sector(0, fill),
 *     sf::read_sector(0, recv)
 * );
 * auto err = static_cmds.execute();
 *
 * // Runtime placeholder — sector chosen at runtime
 * constexpr auto save_slot = sf::compile(
 *     sf::switch_bank(sf::arg(0)),
 *     sf::erase_sector(sf::arg(1)),
 *     sf::write_sector(sf::arg(1), fill)
 * );
 * auto err2 = save_slot.execute(bank, sector);
 * @endcode
 */
#pragma once

#include <gba/bits/flash/operations.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <span>

namespace gba::flash::standard {

/// @brief Mutable view of a 4KB sector buffer, passed to write functions.
using sector_span = std::span<std::byte, sector_size>;

/// @brief Const view of a 4KB sector buffer, passed to read functions.
using const_sector_span = std::span<const std::byte, sector_size>;

/// @brief Function pointer type for write operations (fills the buffer).
using write_fn = void(*)(sector_span);

/// @brief Function pointer type for read operations (receives the buffer).
using read_fn = void(*)(const_sector_span);

// Runtime placeholder

/**
 * @brief A compile-time placeholder for a runtime argument.
 *
 * Used in command constructors to indicate that a value (sector index,
 * bank number) will be provided at runtime via execute().
 *
 * @code{.cpp}
 * // arg(0) = first runtime argument, arg(1) = second, etc.
 * sf::erase_sector(sf::arg(0))
 * sf::switch_bank(sf::arg(1))
 * @endcode
 */
struct arg_type {
    std::int8_t position;
};

/// @brief Create a runtime argument placeholder.
/// @param n Argument position (0-based). Must be unique and contiguous.
consteval arg_type arg(int n) {
    if (n < 0 || n > 7)
        throw "arg: position out of range (0-7)";
    return {static_cast<std::int8_t>(n)};
}

// Command descriptor

/**
 * @brief A single Flash command descriptor.
 *
 * Plain data — no templates. Constructed via the consteval helper
 * functions (erase_sector(), write_sector(), etc.) and validated
 * by compile().
 *
 * When arg_index >= 0, the index field is ignored at runtime and
 * replaced by the corresponding execute() argument.
 */
struct cmd {
    enum kind_type : std::uint8_t {
        erase_sector_k,
        erase_chip_k,
        write_sector_k,
        read_sector_k,
        switch_bank_k,
    };

    kind_type kind{};
    std::int8_t index{};       ///< Static index, or ignored if arg_index >= 0.
    std::int8_t arg_index{-1}; ///< Runtime arg position, or -1 for static.
    write_fn writer{};
    read_fn reader{};
};

// Command constructors — consteval, validated at construction

/// @brief Erase a 4KB sector (compile-time constant).
consteval cmd erase_sector(int sector) {
    if (sector < 0 || sector >= sectors_per_bank)
        throw "erase_sector: sector index out of range (0-15)";
    return {cmd::erase_sector_k, static_cast<std::int8_t>(sector), -1, nullptr, nullptr};
}

/// @brief Erase a 4KB sector (runtime placeholder).
consteval cmd erase_sector(arg_type a) {
    return {cmd::erase_sector_k, 0, a.position, nullptr, nullptr};
}

/// @brief Erase the entire Flash chip (both banks on 128KB chips).
consteval cmd erase_chip() {
    return {cmd::erase_chip_k, 0, -1, nullptr, nullptr};
}

/// @brief Write a 4KB sector (compile-time constant).
consteval cmd write_sector(int sector, write_fn fn) {
    if (sector < 0 || sector >= sectors_per_bank)
        throw "write_sector: sector index out of range (0-15)";
    if (fn == nullptr)
        throw "write_sector: write function must not be null";
    return {cmd::write_sector_k, static_cast<std::int8_t>(sector), -1, fn, nullptr};
}

/// @brief Write a 4KB sector (runtime placeholder).
consteval cmd write_sector(arg_type a, write_fn fn) {
    if (fn == nullptr)
        throw "write_sector: write function must not be null";
    return {cmd::write_sector_k, 0, a.position, fn, nullptr};
}

/// @brief Read a 4KB sector (compile-time constant).
consteval cmd read_sector(int sector, read_fn fn) {
    if (sector < 0 || sector >= sectors_per_bank)
        throw "read_sector: sector index out of range (0-15)";
    if (fn == nullptr)
        throw "read_sector: read function must not be null";
    return {cmd::read_sector_k, static_cast<std::int8_t>(sector), -1, nullptr, fn};
}

/// @brief Read a 4KB sector (runtime placeholder).
consteval cmd read_sector(arg_type a, read_fn fn) {
    if (fn == nullptr)
        throw "read_sector: read function must not be null";
    return {cmd::read_sector_k, 0, a.position, nullptr, fn};
}

/// @brief Switch the active Flash bank (compile-time constant, 0 or 1).
consteval cmd switch_bank(int bank) {
    if (bank < 0 || bank > 1)
        throw "switch_bank: bank index out of range (0-1)";
    return {cmd::switch_bank_k, static_cast<std::int8_t>(bank), -1, nullptr, nullptr};
}

/// @brief Switch the active Flash bank (runtime placeholder).
consteval cmd switch_bank(arg_type a) {
    return {cmd::switch_bank_k, 0, a.position, nullptr, nullptr};
}

// Validation

namespace detail {

    /**
     * @brief Validate a command sequence at compile time.
     *
     * For fully static commands, checks that every write_sector is
     * preceded by erase_sector (same bank + sector) or erase_chip.
     *
     * For commands using runtime placeholders:
     * - A write with arg(N) is satisfied by an erase with the same arg(N)
     *   (they'll resolve to the same sector at runtime).
     * - A write with arg(N) is also satisfied by erase_chip.
     * - A static erase does NOT satisfy a placeholder write (or vice versa),
     *   since the runtime value is unknown.
     */
    consteval bool validate(const cmd* commands, std::size_t count) {
        int bank = 0;
        bool bank_is_arg = false;
        std::int8_t bank_arg = -1;

        // Track erased state for static sectors
        bool erased[2][sectors_per_bank] = {};
        // Track erased state for arg-based sectors (by arg index, max 8)
        bool arg_erased[8] = {};
        bool chip_erased = false;

        for (std::size_t i = 0; i < count; ++i) {
            const auto& c = commands[i];
            const bool is_arg = c.arg_index >= 0;

            switch (c.kind) {
                case cmd::switch_bank_k:
                    if (is_arg) {
                        bank_is_arg = true;
                        bank_arg = c.arg_index;
                    } else {
                        bank = c.index;
                        bank_is_arg = false;
                        bank_arg = -1;
                    }
                    break;

                case cmd::erase_sector_k:
                    if (is_arg) {
                        arg_erased[c.arg_index] = true;
                    } else if (!bank_is_arg) {
                        erased[bank][c.index] = true;
                    }
                    // If bank is an arg but sector is static, we can't
                    // track it — conservative: skip (don't mark erased)
                    break;

                case cmd::erase_chip_k:
                    chip_erased = true;
                    break;

                case cmd::write_sector_k:
                    if (chip_erased) break; // always OK after chip erase
                    if (is_arg) {
                        // Write uses a placeholder — need erase with same arg
                        if (!arg_erased[c.arg_index])
                            return false;
                    } else if (!bank_is_arg) {
                        // Fully static — need static erase on same bank+sector
                        if (!erased[bank][c.index])
                            return false;
                    } else {
                        // Bank is a placeholder, sector is static — can't verify
                        // Conservative: require chip erase (already handled above)
                        return false;
                    }
                    break;

                default:
                    break;
            }
        }
        return true;
    }

    /**
     * @brief Count distinct runtime arguments and validate contiguity.
     * @return Number of required runtime arguments (0 if none).
     */
    consteval std::size_t count_args(const cmd* commands, std::size_t count) {
        bool used[8] = {};
        std::int8_t max_arg = -1;

        for (std::size_t i = 0; i < count; ++i) {
            if (commands[i].arg_index >= 0) {
                used[commands[i].arg_index] = true;
                if (commands[i].arg_index > max_arg)
                    max_arg = commands[i].arg_index;
            }
        }

        if (max_arg < 0) return 0;

        // Check contiguity: arg(0) through arg(max_arg) must all be used
        for (std::int8_t j = 0; j <= max_arg; j++) {
            if (!used[j])
                throw "arg indices must be contiguous starting from 0";
        }

        return static_cast<std::size_t>(max_arg + 1);
    }

} // namespace detail

// Compiled command sequence

/**
 * @brief A compiled sequence of Flash commands.
 *
 * Constructed by compile(). The commands array and count are constexpr,
 * so the compiler can fully unroll execute() and devirtualize function
 * pointer calls at -O2.
 *
 * If the sequence uses runtime placeholders (arg(N)), execute() must
 * be called with exactly num_args integer arguments.
 *
 * @note execute() allocates a 4KB sector buffer on the stack.
 */
struct compiled {
    static constexpr std::size_t max_commands = 32;
    cmd commands[max_commands]{};
    std::size_t count{};
    std::size_t num_args{};

    /// @brief Execute with runtime arguments (or none if fully static).
    template<std::convertible_to<int>... Args>
    error execute(Args... args) const noexcept {
        static_assert(sizeof...(args) <= 8, "Too many runtime arguments (max 8)");
        const int rt[] = {static_cast<int>(args)..., 0}; // trailing 0 for empty pack
        return execute_impl(rt);
    }

    /// @brief Alias for execute().
    template<std::convertible_to<int>... Args>
    error operator()(Args... args) const noexcept {
        return execute(args...);
    }

private:
    error execute_impl(const int* rt) const noexcept {
        alignas(4) std::array<std::uint8_t, sector_size> buffer{};
        const auto as_byte = [&]() noexcept { return reinterpret_cast<std::byte*>(buffer.data()); };
        const auto as_cbyte = [&]() noexcept { return reinterpret_cast<const std::byte*>(buffer.data()); };

        for (std::size_t i = 0; i < count; ++i) {
            const auto& c = commands[i];
            const auto idx = (c.arg_index >= 0) ? rt[c.arg_index] : c.index;
            const auto base = static_cast<std::uint32_t>(idx) * sector_size;

            switch (c.kind) {
                case cmd::erase_sector_k:
                    if (flash::detail::erase_sector(base) != 0)
                        return error::timeout;
                    break;

                case cmd::erase_chip_k:
                    if (flash::detail::erase_chip() != 0)
                        return error::timeout;
                    break;

                case cmd::write_sector_k:
                    c.writer(sector_span{as_byte(), sector_size});
                    for (std::size_t j = 0; j < sector_size; ++j) {
                        if (flash::detail::write_byte(base + j, buffer[j]) != 0)
                            return error::timeout;
                    }
                    break;

                case cmd::read_sector_k:
                    flash::detail::read_bytes(buffer.data(), flash::detail::flash_ptr(base), sector_size);
                    c.reader(const_sector_span{as_cbyte(), sector_size});
                    break;

                case cmd::switch_bank_k:
                    flash::detail::switch_bank(idx);
                    flash::detail::g_state.current_bank = idx;
                    break;
            }
        }
        return error::success;
    }
};

// compile() — consteval command sequence compiler

/**
 * @brief Compile a sequence of standard Flash commands.
 *
 * Validates at compile time:
 * - Sector indices are 0-15 (checked by constructors)
 * - Bank indices are 0-1 (checked by constructors)
 * - Every write_sector is preceded by erase_sector or erase_chip
 * - Runtime arg indices are contiguous starting from 0
 *
 * @return A compiled command object with an execute() method.
 */
consteval compiled compile(std::same_as<cmd> auto... cmds) {
    static_assert(sizeof...(cmds) <= compiled::max_commands,
        "Too many commands (max 32)");

    compiled result{};
    result.count = sizeof...(cmds);
    std::size_t i = 0;
    ((result.commands[i++] = cmds), ...);

    result.num_args = detail::count_args(result.commands, result.count);

    if (!detail::validate(result.commands, result.count))
        throw "write_sector must be preceded by erase_sector or erase_chip";

    return result;
}

} // namespace gba::flash::standard
