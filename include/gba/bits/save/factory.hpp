/// @file bits/save/factory.hpp
/// @brief Backend-named factories building a `gba::bits::backup::table`, deferred until assignment.
#pragma once

#include <gba/bits/save/eeprom_backend.hpp>
#include <gba/bits/save/flash_backend.hpp>
#include <gba/bits/save/record.hpp>
#include <gba/bits/save/sram_backend.hpp>
#include <gba/bits/save/table.hpp>

namespace gba::bits::backup {

    /// @brief One `gba::bits::backup::record<Codec, B>` per `Codecs`, auto-placed by
    /// `gba::bits::backup::table`.
    template<Backend B, const auto&... Codecs>
        requires(sizeof...(Codecs) > 0)
    using backup_table = table<record<Codecs, B>...>;

    /// @brief Stateless tag naming a physical `Backend` and root codecs, building the table
    /// only when `operator()` runs -- i.e. when `gba::backup` is actually assigned, not when
    /// the `backup_*` variable is defined (its constructor reads the backend's directory).
    template<Backend B, const auto&... Codecs>
        requires(sizeof...(Codecs) > 0)
    struct backup_factory {
        using table_type = backup_table<B, Codecs...>;

        table_type operator()() const { return table_type{}; }
    };

    /// @brief `record<Codecs, flash_backend_standard_64k>...`, one per 4KB sector.
    template<const auto&... Codecs>
    inline constexpr backup_factory<flash_backend_standard_64k, Codecs...> backup_flash_64k{};

    /// @brief `record<Codecs, flash_backend_standard_128k>...`, one per 4KB sector.
    template<const auto&... Codecs>
    inline constexpr backup_factory<flash_backend_standard_128k, Codecs...> backup_flash_128k{};

    /// @brief `record<Codecs, flash_backend_atmel>...`, one per 128-byte page.
    template<const auto&... Codecs>
    inline constexpr backup_factory<flash_backend_atmel, Codecs...> backup_flash_atmel{};

    /// @brief `record<Codecs, eeprom_backend_512b>...`, one per 8-byte block.
    template<const auto&... Codecs>
    inline constexpr backup_factory<eeprom_backend_512b, Codecs...> backup_eeprom_512b{};

    /// @brief `record<Codecs, eeprom_backend_8k>...`, one per 8-byte block.
    template<const auto&... Codecs>
    inline constexpr backup_factory<eeprom_backend_8k, Codecs...> backup_eeprom_8k{};

    /// @brief `record<Codecs, sram_backend>...`, packed byte-for-byte.
    template<const auto&... Codecs>
    inline constexpr backup_factory<sram_backend, Codecs...> backup_sram{};

} // namespace gba::bits::backup
