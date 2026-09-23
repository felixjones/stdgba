/// @file bits/save/flash_backend.hpp
/// @brief Save backends adapting sector/page-erased Flash chips to `gba::bits::backup::Backend`.
#pragma once

#include <gba/bits/flash/operations.hpp>
#include <gba/bits/save/backend.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gba::bits::backup {

    namespace detail {

        inline void ensure_flash_detected() noexcept {
            static bool detected = false;
            if (!detected) {
                static_cast<void>(flash::detect());
                detected = true;
            }
        }

        template<std::size_t Banks>
        void ensure_bank(const int bank) noexcept {
            if constexpr (Banks > 1) {
                if (flash::bits::g_state.current_bank != bank) {
                    flash::bits::switch_bank(bank);
                    flash::bits::g_state.current_bank = bank;
                }
            }
        }

        [[nodiscard]] constexpr std::pair<int, std::size_t> locate_bank(const std::size_t offset) noexcept {
            return {static_cast<int>(offset / flash::bank_size), offset % flash::bank_size};
        }

        template<std::size_t Banks>
        void read_flash(const std::size_t offset, const std::span<std::byte> out) noexcept {
            ensure_flash_detected();
            std::size_t pos = 0;
            while (pos < out.size()) {
                const auto [bank, bankOffset] = locate_bank(offset + pos);
                ensure_bank<Banks>(bank);

                const std::size_t chunk = std::min(out.size() - pos, flash::bank_size - bankOffset);
                flash::bits::read_bytes(out.data() + pos, flash::bits::flash_ptr(bankOffset), chunk);
                pos += chunk;
            }
        }

    } // namespace detail

    /// @brief `Backend` over standard (erase-then-write) Flash chips -- Macronix, Panasonic, Sanyo, SST.
    /// A partial-sector `write()` reads the whole 4KB sector, overlays the new bytes, erases, and
    /// writes the combined result back, since Flash can only clear bits and erasing destroys a sector.
    ///
    /// @tparam Banks Number of 64KB Flash banks (1 for 64KB chips, 2 for 128KB chips).
    template<std::size_t Banks = 1>
        requires(Banks == 1 || Banks == 2)
    struct flash_standard_backend {
        /// @brief Total addressable bytes across all banks.
        static constexpr std::size_t capacity = Banks * flash::bank_size;

        /// @brief The erase domain -- writing any byte in a sector may rewrite the whole sector.
        static constexpr std::size_t granularity = flash::sector_size;

        static void read(const std::size_t offset, const std::span<std::byte> out) noexcept {
            detail::read_flash<Banks>(offset, out);
        }

        void write(std::size_t offset, std::span<const std::byte> in) noexcept {
            detail::ensure_flash_detected();
            m_ok = true;
            std::size_t pos = 0;
            while (pos < in.size()) {
                const auto [bank, bankOffset] = detail::locate_bank(offset + pos);
                const std::size_t sectorOffset = bankOffset % flash::sector_size;
                const std::size_t sectorBase = bankOffset - sectorOffset;
                const std::size_t chunk = std::min(in.size() - pos, flash::sector_size - sectorOffset);

                detail::ensure_bank<Banks>(bank);
                m_ok &= write_sector(sectorBase, sectorOffset, in.subspan(pos, chunk));
                pos += chunk;
            }
        }

        /// @brief Whether every hardware operation in the most recent `write()` reported success.
        [[nodiscard]] bool ok() const noexcept { return m_ok; }

    private:
        bool m_ok = true;

        [[nodiscard]] static bool write_sector(const std::size_t sectorBase, const std::size_t sectorOffset,
                                               const std::span<const std::byte> chunk) noexcept {
            alignas(4) std::array<std::byte, flash::sector_size> staged{};
            flash::bits::read_bytes(staged.data(), flash::bits::flash_ptr(sectorBase), flash::sector_size);
            for (std::size_t i = 0; i < chunk.size(); ++i) {
                staged[sectorOffset + i] = chunk[i];
            }

            const auto base = static_cast<std::uint32_t>(sectorBase);
            bool ok = flash::bits::erase_sector(base) == 0;
            for (std::size_t i = 0; i < flash::sector_size; ++i) {
                ok &= flash::bits::write_byte(base + static_cast<std::uint32_t>(i),
                                              std::to_integer<std::uint8_t>(staged[i])) == 0;
            }
            return ok;
        }
    };

    /// @brief `Backend` over a 64KB standard Flash chip.
    using flash_backend_standard_64k = flash_standard_backend<1>;

    /// @brief `Backend` over a 128KB standard Flash chip.
    using flash_backend_standard_128k = flash_standard_backend<2>;

    /// @brief `Backend` over Atmel Flash chips (128-byte page writes, no separate erase). A partial-page
    /// `write()` still needs a read-modify-write, since a page write implicitly erases the whole page.
    ///
    /// @tparam Banks Number of 64KB Flash banks.
    template<std::size_t Banks = 1>
        requires(Banks == 1 || Banks == 2)
    struct flash_atmel_backend {
        /// @brief Total addressable bytes across all banks.
        static constexpr std::size_t capacity = Banks * flash::bank_size;

        /// @brief Atmel's write unit -- a 128-byte page is always programmed as a whole.
        static constexpr std::size_t granularity = flash::page_size_atmel;

        static void read(const std::size_t offset, const std::span<std::byte> out) noexcept {
            detail::read_flash<Banks>(offset, out);
        }

        void write(std::size_t offset, std::span<const std::byte> in) noexcept {
            detail::ensure_flash_detected();
            m_ok = true;
            std::size_t pos = 0;
            while (pos < in.size()) {
                const auto [bank, bankOffset] = detail::locate_bank(offset + pos);
                const std::size_t pageOffset = bankOffset % flash::page_size_atmel;
                const std::size_t pageBase = bankOffset - pageOffset;
                const std::size_t chunk = std::min(in.size() - pos, flash::page_size_atmel - pageOffset);

                detail::ensure_bank<Banks>(bank);
                m_ok &= write_page(pageBase, pageOffset, in.subspan(pos, chunk));
                pos += chunk;
            }
        }

        /// @brief Whether every hardware operation in the most recent `write()` reported success.
        [[nodiscard]] bool ok() const noexcept { return m_ok; }

    private:
        bool m_ok = true;

        [[nodiscard]] static bool write_page(const std::size_t pageBase, const std::size_t pageOffset,
                                             const std::span<const std::byte> chunk) noexcept {
            alignas(4) std::array<std::uint8_t, flash::page_size_atmel> staged{};
            flash::bits::read_bytes(staged.data(), flash::bits::flash_ptr(pageBase), flash::page_size_atmel);
            for (std::size_t i = 0; i < chunk.size(); ++i) {
                staged[pageOffset + i] = std::to_integer<std::uint8_t>(chunk[i]);
            }
            return flash::bits::write_atmel_page(static_cast<std::uint32_t>(pageBase), staged.data()) == 0;
        }
    };

    /// @brief `Backend` over a 64KB Atmel Flash chip.
    using flash_backend_atmel = flash_atmel_backend<1>;

    static_assert(Backend<flash_backend_standard_64k>);
    static_assert(Backend<flash_backend_standard_128k>);
    static_assert(Backend<flash_backend_atmel>);

} // namespace gba::bits::backup
