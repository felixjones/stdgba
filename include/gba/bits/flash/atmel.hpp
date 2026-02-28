/// @file bits/flash/atmel.hpp
/// @brief Layer 1: Compiled command sequences for Atmel Flash chips.
///
/// Provides a consteval command compiler for Atmel Flash chips (AT29LV512).
/// Atmel chips use 128-byte page writes that implicitly erase -- no separate
/// erase step is needed (or available).
///
/// Commands can use compile-time constants or runtime placeholders:
/// @code{.cpp}
/// #include <gba/bits/flash/atmel.hpp>
///
/// namespace af = gba::flash::atmel;
///
/// void fill(af::page_span buf) { buf[0] = 0x42; }
/// void recv(af::const_page_span buf) { g_val = buf[0]; }
///
/// // Fully static
/// constexpr auto cmds = af::compile(
///     af::write_page(0, fill),
///     af::read_page(0, recv)
/// );
/// auto err = cmds.execute();
///
/// // Runtime placeholder -- page chosen at runtime
/// constexpr auto save = af::compile(
///     af::switch_bank(af::arg(0)),
///     af::write_page(af::arg(1), fill)
/// );
/// auto err2 = save.execute(bank, page);
/// @endcode
#pragma once

#include <gba/bits/flash/operations.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <span>

namespace gba::flash::atmel {

    /// @brief Mutable view of a 128-byte page buffer, passed to write functions.
    using page_span = std::span<std::byte, page_size_atmel>;

    /// @brief Const view of a 128-byte page buffer, passed to read functions.
    using const_page_span = std::span<const std::byte, page_size_atmel>;

    /// @brief Function pointer type for write operations (fills the buffer).
    using write_fn = void (*)(page_span);

    /// @brief Function pointer type for read operations (receives the buffer).
    using read_fn = void (*)(const_page_span);

    /// @brief A compile-time placeholder for a runtime argument.
    struct arg_type {
        std::int8_t position;
    };

    /// @brief Create a runtime argument placeholder.
    /// @param n Argument position (0-based). Must be contiguous.
    consteval arg_type arg(int n) {
        if (n < 0 || n > 7) throw "arg: position out of range (0-7)";
        return {static_cast<std::int8_t>(n)};
    }

    /// @brief A single Atmel Flash command descriptor.
    struct cmd {
        enum kind_type : std::uint8_t {
            write_page_k,
            read_page_k,
            switch_bank_k,
        };

        kind_type kind{};
        std::int16_t index{};
        std::int8_t arg_index{-1}; ///< Runtime arg position, or -1 for static.
        write_fn writer{};
        read_fn reader{};
    };

    /// @brief Write a 128-byte page (compile-time constant).
    consteval cmd write_page(int page, write_fn fn) {
        if (page < 0 || page >= pages_per_bank) throw "write_page: page index out of range (0-511)";
        if (fn == nullptr) throw "write_page: write function must not be null";
        return {cmd::write_page_k, static_cast<std::int16_t>(page), -1, fn, nullptr};
    }

    /// @brief Write a 128-byte page (runtime placeholder).
    consteval cmd write_page(arg_type a, write_fn fn) {
        if (fn == nullptr) throw "write_page: write function must not be null";
        return {cmd::write_page_k, 0, a.position, fn, nullptr};
    }

    /// @brief Read a 128-byte page (compile-time constant).
    consteval cmd read_page(int page, read_fn fn) {
        if (page < 0 || page >= pages_per_bank) throw "read_page: page index out of range (0-511)";
        if (fn == nullptr) throw "read_page: read function must not be null";
        return {cmd::read_page_k, static_cast<std::int16_t>(page), -1, nullptr, fn};
    }

    /// @brief Read a 128-byte page (runtime placeholder).
    consteval cmd read_page(arg_type a, read_fn fn) {
        if (fn == nullptr) throw "read_page: read function must not be null";
        return {cmd::read_page_k, 0, a.position, nullptr, fn};
    }

    /// @brief Switch the active Flash bank (compile-time constant, 0 or 1).
    consteval cmd switch_bank(int bank) {
        if (bank < 0 || bank > 1) throw "switch_bank: bank index out of range (0-1)";
        return {cmd::switch_bank_k, static_cast<std::int16_t>(bank), -1, nullptr, nullptr};
    }

    /// @brief Switch the active Flash bank (runtime placeholder).
    consteval cmd switch_bank(arg_type a) {
        return {cmd::switch_bank_k, 0, a.position, nullptr, nullptr};
    }

    namespace bits {

        /// @brief Count distinct runtime arguments and validate contiguity.
        consteval std::size_t count_args(const cmd* commands, std::size_t count) {
            bool used[8] = {};
            std::int8_t max_arg = -1;

            for (std::size_t i = 0; i < count; ++i) {
                if (commands[i].arg_index >= 0) {
                    used[commands[i].arg_index] = true;
                    if (commands[i].arg_index > max_arg) max_arg = commands[i].arg_index;
                }
            }

            if (max_arg < 0) return 0;

            for (std::int8_t j = 0; j <= max_arg; ++j) {
                if (!used[j]) throw "arg indices must be contiguous starting from 0";
            }

            return static_cast<std::size_t>(max_arg + 1);
        }

    } // namespace bits

    /// @brief A compiled sequence of Atmel Flash commands.
    struct compiled {
        static constexpr std::size_t max_commands = 32;
        cmd commands[max_commands]{};
        std::size_t count{};
        std::size_t num_args{};

        /// @brief Execute with runtime arguments (or none if fully static).
        template<std::convertible_to<int>... Args>
        error execute(Args... args) const noexcept {
            static_assert(sizeof...(args) <= 8, "Too many runtime arguments (max 8)");
            const int rt[] = {static_cast<int>(args)..., 0};
            return execute_impl(rt);
        }

        /// @brief Alias for execute().
        template<std::convertible_to<int>... Args>
        error operator()(Args... args) const noexcept {
            return execute(args...);
        }

    private:
        error execute_impl(const int* rt) const noexcept {
            alignas(4) std::array<std::uint8_t, page_size_atmel> buffer{};
            const auto as_byte = [&]() noexcept {
                return reinterpret_cast<std::byte*>(buffer.data());
            };
            const auto as_cbyte = [&]() noexcept {
                return reinterpret_cast<const std::byte*>(buffer.data());
            };

            for (std::size_t i = 0; i < count; ++i) {
                const auto& c = commands[i];
                const auto idx = (c.arg_index >= 0) ? rt[c.arg_index] : static_cast<int>(c.index);
                const auto addr = static_cast<std::uint32_t>(idx) * page_size_atmel;

                switch (c.kind) {
                    case cmd::write_page_k:
                        c.writer(page_span{as_byte(), page_size_atmel});
                        if (flash::bits::write_atmel_page(addr, buffer.data()) != 0) return error::timeout;
                        break;

                    case cmd::read_page_k:
                        flash::bits::read_bytes(buffer.data(), flash::bits::flash_ptr(addr), page_size_atmel);
                        c.reader(const_page_span{as_cbyte(), page_size_atmel});
                        break;

                    case cmd::switch_bank_k:
                        flash::bits::switch_bank(idx);
                        flash::bits::g_state.current_bank = idx;
                        break;
                }
            }
            return error::success;
        }
    };

    /// @brief Compile a sequence of Atmel Flash commands.
    ///
    /// Validates at compile time:
    /// - Page indices are 0-511 (checked by constructors)
    /// - Bank indices are 0-1 (checked by constructors)
    /// - Runtime arg indices are contiguous starting from 0
    ///
    /// @return A compiled command object with execute() / operator()().
    consteval compiled compile(std::same_as<cmd> auto... cmds) {
        static_assert(sizeof...(cmds) <= compiled::max_commands, "Too many commands (max 32)");

        compiled result{};
        result.count = sizeof...(cmds);
        std::size_t i = 0;
        ((result.commands[i++] = cmds), ...);

        result.num_args = bits::count_args(result.commands, result.count);

        return result;
    }

} // namespace gba::flash::atmel
