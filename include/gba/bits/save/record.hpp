/// @file bits/save/record.hpp
/// @brief Codec-driven typed save record.
#pragma once

#include <gba/bits/save/backend.hpp>
#include <gba/codec>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <variant>

namespace gba::bits::backup {

    namespace detail {

        template<typename T>
        concept Monostate = std::same_as<T, std::monostate>;

        template<typename C>
        struct is_optional_like_variant : std::false_type {
            using alt_codec = C;
        };

        template<codec::Codec A0, codec::Codec A1>
            requires(Monostate<typename A0::value_type> != Monostate<typename A1::value_type>)
        struct is_optional_like_variant<codec::variant_codec<A0, A1>> : std::true_type {
            static constexpr bool monostate_is_first = Monostate<typename A0::value_type>;
            using alt_codec = std::conditional_t<monostate_is_first, A1, A0>;
        };

        template<typename C>
        concept OptionalLike = is_optional_like_variant<std::remove_cvref_t<C>>::value;

        template<const auto& Codec>
        using record_codec_t =
            std::conditional_t<OptionalLike<decltype(Codec)>,
                               typename is_optional_like_variant<std::remove_cvref_t<decltype(Codec)>>::alt_codec,
                               std::remove_cvref_t<decltype(Codec)>>;

        template<const auto& Codec>
        using record_value_t = record_codec_t<Codec>::value_type;

        template<const auto& Codec>
        consteval std::size_t default_capacity() noexcept {
            using c = record_codec_t<Codec>;
            static_assert(gba::codec::FixedSizeCodec<c>,
                          "Capacity must be specified explicitly for a variable-size codec");
            return c::encoded_size;
        }

        template<typename C>
        consteval bool capacity_fits(const std::size_t capacity) noexcept {
            if constexpr (gba::codec::FixedSizeCodec<C>) {
                return C::encoded_size <= capacity;
            } else {
                return true;
            }
        }

    } // namespace detail

    /// @brief One typed value backed by save memory, with no framing of its own.
    ///
    /// A record is exactly `Capacity` bytes of whatever `Codec` encodes -- no header,
    /// length, or checksum. Presence and formatting are `gba::bits::backup::table`'s job;
    /// used directly, `load()` just decodes whatever bytes are there, valid or not.
    /// `Capacity` may be omitted for a `gba::codec::FixedSizeCodec` (defaults to its
    /// `encoded_size`); a variable-size codec requires it explicitly as an upper bound.
    ///
    /// A top-level `Codec` shaped like `std::variant<std::monostate, T>` is treated as
    /// `T` with its own presence erased (`table` already tracks that per field), but is
    /// not fixed-size: `table` still places it in the dynamic arena.
    ///
    /// @code{.cpp}
    /// struct player_save { std::uint32_t score; std::uint16_t level; };
    ///
    /// constexpr auto save_codec = gba::codec::make_tuple_codec(
    ///     gba::codec::member<&player_save::score>(gba::codec::uint32_codec),
    ///     gba::codec::member<&player_save::level>(gba::codec::uint16_codec)
    /// ).apply<player_save>();
    ///
    /// using record_type = gba::bits::backup::record<save_codec, gba::bits::backup::sram_backend>;
    ///
    /// record_type save;
    /// if (auto loaded = save.load()) {
    ///     use(*loaded);
    /// }
    /// save.store(player_save{.score = 100, .level = 2});
    /// @endcode
    template<const auto& Codec, Backend B, std::size_t Capacity = detail::default_capacity<Codec>()>
        requires gba::codec::Codec<std::remove_cvref_t<decltype(Codec)>>
    struct record {
        using backend_type = B;

        /// @brief The raw codec named by `Codec`, before any `std::monostate`-variant erasure.
        using codec_type = std::remove_cvref_t<decltype(Codec)>;

        /// @brief The value `store`/`load` deal in.
        using value_type = detail::record_value_t<Codec>;

        /// @brief Whether this record's wire footprint is a compile-time constant.
        static constexpr bool is_fixed_size = gba::codec::FixedSizeCodec<codec_type>;

        /// @brief Total wire footprint -- exactly `Capacity`.
        static constexpr std::size_t wire_size = Capacity;

        static_assert(detail::capacity_fits<detail::record_codec_t<Codec>>(Capacity),
                      "Capacity is smaller than the codec's fixed encoded_size");

        /// @brief Construct a record at a caller-chosen byte offset in the backend.
        ///
        /// Prefer `gba::bits::backup::table` unless this is the only record sharing the backend.
        constexpr explicit record(const std::size_t offset = 0) noexcept : m_offset(offset) {}

        /// @brief The byte offset this record occupies in its backend.
        [[nodiscard]] constexpr std::size_t offset() const noexcept { return m_offset; }

        /// @brief How many bytes `value` would occupy if `store`d, without writing anything.
        /// @return `std::nullopt` if `value` does not fit within `Capacity` encoded bytes.
        [[nodiscard]] std::optional<std::size_t> encoded_length(const value_type& value) const {
            std::array<std::byte, Capacity> payload{};
            const auto written = codec::encode(effective_codec(), value, payload);
            if (!written) {
                return std::nullopt;
            }
            return *written;
        }

        /// @brief Encode `value` and write it, but only if it fits within `maxBytes`
        /// (itself clamped to `Capacity`); nothing is written otherwise.
        /// @return The number of bytes written, or `std::nullopt` if `value` does not fit.
        std::optional<std::size_t> try_store(const value_type& value, const std::size_t maxBytes) {
            std::array<std::byte, Capacity> payload{};
            const std::size_t bound = std::min({maxBytes, Capacity, backend_room()});
            const auto written = codec::encode(effective_codec(), value, std::span{payload.data(), bound});
            if (!written) {
                return std::nullopt;
            }
            m_backend.write(m_offset, std::span<const std::byte>{payload.data(), *written});
            return *written;
        }

        /// @brief Encode `value` and write it to the backend, with no framing of any kind.
        /// @return `false` if `value` does not fit within `Capacity` encoded bytes.
        bool store(const value_type& value) { return try_store(value, Capacity).has_value(); }

        /// @brief Read up to `Capacity` bytes and decode them via `Codec`.
        /// @return The decoded value, or `std::nullopt` if the bytes read do not decode to a
        ///         valid `value_type`. Not a presence check -- a blank backend may decode to
        ///         *some* value.
        [[nodiscard]] std::optional<value_type> load() const {
            std::array<std::byte, Capacity> payload{};
            m_backend.read(m_offset, std::span{payload.data(), std::min(Capacity, backend_room())});

            const auto outcome = codec::decode(effective_codec(), payload);
            if (!outcome.value) {
                return std::nullopt;
            }
            return *outcome.value;
        }

        [[nodiscard]] backend_type& backend() noexcept { return m_backend; }
        [[nodiscard]] const backend_type& backend() const noexcept { return m_backend; }

    private:
        [[nodiscard]] std::size_t backend_room() const noexcept {
            return m_offset < backend_type::capacity ? backend_type::capacity - m_offset : 0;
        }

        static constexpr decltype(auto) effective_codec() noexcept {
            if constexpr (detail::OptionalLike<decltype(Codec)>) {
                using info = detail::is_optional_like_variant<codec_type>;
                if constexpr (info::monostate_is_first) {
                    return std::get<1>(Codec.alternatives);
                } else {
                    return std::get<0>(Codec.alternatives);
                }
            } else {
                return Codec;
            }
        }

        std::size_t m_offset;
        backend_type m_backend{};
    };

} // namespace gba::bits::backup
