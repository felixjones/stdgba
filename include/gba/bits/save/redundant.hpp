/// @file bits/save/redundant.hpp
/// @brief Tags a codec for power-loss-safe placement by gba::bits::backup::table: the
/// previously stored value is never touched until a new one has fully landed elsewhere.
#pragma once

#include <gba/codec>

#include <type_traits>

namespace gba::bits::backup {

    /// @brief Wraps a codec, unchanged, tagging its field so a new value is never written
    /// over the currently stored one -- it always lands somewhere else first.
    ///
    /// For a fixed-size codec, `gba::bits::backup::table` reserves two granularity-aligned
    /// slots up front and alternates between them. For a variable-size codec, each
    /// `emplace<I>()` is placed in a free run of the dynamic arena that excludes the
    /// field's own current allocation (defragmenting first if nothing else is big enough),
    /// so the previous copy's bytes are left alone until the new one is in place.
    ///
    /// Either way, the field's directory entry -- already CRC-checked, and already only
    /// ever updated after the corresponding write succeeds -- is what flips to the new
    /// copy; a write interrupted midway, e.g. by power loss, leaves the entry (and the
    /// previous copy it still points at) untouched. No extra checksum or generation
    /// counter is needed per copy.
    ///
    /// @code{.cpp}
    /// constexpr auto settings_codec = gba::bits::backup::redundant(
    ///     gba::codec::make_tuple_codec(gba::codec::member<&settings_save::sound>(gba::codec::bool_codec))
    ///         .apply<settings_save>());
    /// @endcode
    template<codec::Codec C>
    struct redundant_codec
        : codec::fixed_size_base<codec::detail::forwarded_encoded_size<C>(), codec::FixedSizeCodec<C>> {
        using value_type = C::value_type;

        C inner{};

        constexpr auto make_decoder() const noexcept(noexcept(inner.make_decoder())) { return inner.make_decoder(); }

        constexpr auto make_encoder(const value_type& value) const noexcept(noexcept(inner.make_encoder(value))) {
            return inner.make_encoder(value);
        }
    };

    /// @brief Tag `codec` for power-loss-safe (never-overwrite-in-place) placement by
    /// `gba::bits::backup::table`; works for both fixed- and variable-size codecs.
    template<codec::Codec C>
    consteval redundant_codec<C> redundant(const C& codec) noexcept {
        return redundant_codec<C>{.inner = codec};
    }

    namespace detail {

        template<typename C>
        struct is_redundant_codec : std::false_type {};

        template<typename C>
        struct is_redundant_codec<redundant_codec<C>> : std::true_type {};

    } // namespace detail

} // namespace gba::bits::backup
