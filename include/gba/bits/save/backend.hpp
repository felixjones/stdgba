/// @file bits/save/backend.hpp
/// @brief Concept describing a byte-addressable save memory backend.
#pragma once

#include <concepts>
#include <cstddef>
#include <span>

namespace gba::bits::backup {

    /// @brief A byte-addressable save memory device.
    ///
    /// Backends abstract over SRAM, EEPROM, and Flash so that
    /// `gba::bits::backup::record` can be written once and reused across every save
    /// memory type, the same way `gba::codec::Codec` lets `gba::link_normal`
    /// and `gba::link_multi` share one framed transport implementation.
    ///
    /// A backend need not be stateless -- it may cache addressing/bank state
    /// -- but `read`/`write` must be able to service any `offset` within
    /// `[0, capacity)` without the caller tracking device-specific geometry
    /// (block size, sector size, ...) itself. `granularity` still surfaces that
    /// geometry for `gba::bits::backup::table`, which places each record on a
    /// `granularity`-aligned boundary so no record's read-modify-write (or,
    /// for Flash, erase) ever touches another record's bytes.
    template<typename B>
    concept Backend =
        requires(B& b, const B& cb, std::size_t offset, std::span<std::byte> out, std::span<const std::byte> in) {
            { B::capacity } -> std::convertible_to<std::size_t>;
            { B::granularity } -> std::convertible_to<std::size_t>;
            { cb.read(offset, out) } -> std::same_as<void>;
            { b.write(offset, in) } -> std::same_as<void>;
        };

} // namespace gba::bits::backup
