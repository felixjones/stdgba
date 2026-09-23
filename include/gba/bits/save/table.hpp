/// @file bits/save/table.hpp
/// @brief Auto-placement of multiple `gba::bits::backup::record` fields on one backend.
#pragma once

#include <gba/bits/constexpr_assert.hpp>
#include <gba/bits/save/crc16.hpp>
#include <gba/bits/save/redundant.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace gba::bits::backup {

    namespace detail {

        [[nodiscard]] constexpr std::size_t align_up(const std::size_t bytes, const std::size_t granularity) noexcept {
            return ((bytes + granularity - 1) / granularity) * granularity;
        }

        template<std::size_t N>
        [[nodiscard]] constexpr std::array<std::size_t, N> pack_offsets(const std::array<std::size_t, N>& wireSizes,
                                                                        const std::size_t granularity) noexcept {
            std::array<std::size_t, N> offsets{};
            std::size_t cursor = 0;
            for (std::size_t i = 0; i < N; ++i) {
                offsets[i] = cursor;
                cursor += align_up(wireSizes[i], granularity);
            }
            return offsets;
        }

        template<std::size_t N>
        [[nodiscard]] constexpr std::size_t total_packed_size(const std::array<std::size_t, N>& wireSizes,
                                                              const std::size_t granularity) noexcept {
            std::size_t total = 0;
            for (const auto w : wireSizes) {
                total += align_up(w, granularity);
            }
            return total;
        }

        template<std::size_t N, Backend B>
        struct field_directory {
            struct entry {
                std::uint16_t start = 0;
                std::uint16_t count = 0;
                std::uint16_t pending_start = 0;
                std::uint16_t pending_count = 0;
            };

            static constexpr std::size_t generation_size = 2;
            static constexpr std::size_t entry_size = 8;
            static constexpr std::size_t footer_size = 2;
            static constexpr std::size_t wire_size = generation_size + (N * entry_size) + footer_size;

            constexpr field_directory() noexcept = default;
            constexpr field_directory(const std::size_t offset, const std::size_t copyStride) noexcept
                : m_offset(offset), m_copyStride(copyStride) {}

            [[nodiscard]] std::array<entry, N> load() {
                const auto first = load_copy(0);
                const auto second = load_copy(1);

                if (!first && !second) {
                    m_hasActive = false;
                    m_generation = 0;
                    return {};
                }
                if (!second || (first && is_newer(first->generation, second->generation))) {
                    m_activeCopy = 0;
                    m_generation = first->generation;
                    m_hasActive = true;
                    return first->entries;
                }
                m_activeCopy = 1;
                m_generation = second->generation;
                m_hasActive = true;
                return second->entries;
            }

            void store(const std::array<entry, N>& entries) {
                const std::size_t copy = m_hasActive ? m_activeCopy ^ 1u : 0;
                const auto generation = static_cast<std::uint16_t>(m_generation + 1);
                std::array<std::byte, wire_size> raw{};
                const auto g = pack_u16(generation);
                raw[0] = g[0];
                raw[1] = g[1];
                for (std::size_t i = 0; i < N; ++i) {
                    const std::size_t base = generation_size + (i * entry_size);
                    const auto s = pack_u16(entries[i].start);
                    const auto c = pack_u16(entries[i].count);
                    const auto pendingS = pack_u16(entries[i].pending_start);
                    const auto pendingC = pack_u16(entries[i].pending_count);
                    raw[base] = s[0];
                    raw[base + 1] = s[1];
                    raw[base + 2] = c[0];
                    raw[base + 3] = c[1];
                    raw[base + 4] = pendingS[0];
                    raw[base + 5] = pendingS[1];
                    raw[base + 6] = pendingC[0];
                    raw[base + 7] = pendingC[1];
                }
                const std::uint16_t crc = crc16(std::span{raw.data(), wire_size - footer_size});
                const auto ck = pack_u16(crc);
                raw[wire_size - 2] = ck[0];
                raw[wire_size - 1] = ck[1];
                m_backend.write(m_offset + (copy * m_copyStride), raw);
                m_activeCopy = copy;
                m_generation = generation;
                m_hasActive = true;
            }

        private:
            struct copy {
                std::uint16_t generation;
                std::array<entry, N> entries;
            };

            [[nodiscard]] std::optional<copy> load_copy(const std::size_t index) const {
                std::array<std::byte, wire_size> raw{};
                m_backend.read(m_offset + (index * m_copyStride), raw);

                const std::uint16_t stored = unpack_u16(raw[wire_size - 2], raw[wire_size - 1]);
                if (crc16(std::span{raw.data(), wire_size - footer_size}) != stored) {
                    return std::nullopt;
                }
                copy result{.generation = unpack_u16(raw[0], raw[1])};
                for (std::size_t i = 0; i < N; ++i) {
                    const std::size_t base = generation_size + (i * entry_size);
                    result.entries[i].start = unpack_u16(raw[base], raw[base + 1]);
                    result.entries[i].count = unpack_u16(raw[base + 2], raw[base + 3]);
                    result.entries[i].pending_start = unpack_u16(raw[base + 4], raw[base + 5]);
                    result.entries[i].pending_count = unpack_u16(raw[base + 6], raw[base + 7]);
                }
                return result;
            }

            [[nodiscard]] static constexpr bool is_newer(const std::uint16_t lhs, const std::uint16_t rhs) noexcept {
                return lhs != rhs && static_cast<std::int16_t>(lhs - rhs) > 0;
            }

            [[nodiscard]] static constexpr std::array<std::byte, 2> pack_u16(const std::uint16_t value) noexcept {
                return {
                    std::byte{static_cast<unsigned char>(value)},
                    std::byte{static_cast<unsigned char>(value >> 8u)},
                };
            }

            [[nodiscard]] static constexpr std::uint16_t unpack_u16(const std::byte lo, const std::byte hi) noexcept {
                return static_cast<std::uint16_t>(
                    std::to_integer<unsigned char>(lo) |
                    (static_cast<std::uint16_t>(std::to_integer<unsigned char>(hi)) << 8u));
            }

            std::size_t m_offset = 0;
            std::size_t m_copyStride = wire_size;
            B m_backend{};
            std::size_t m_activeCopy = 0;
            std::uint16_t m_generation = 0;
            bool m_hasActive = false;
        };

    } // namespace detail

    /// @brief Places several `gba::bits::backup::record` fields on one backend, one `Backend::granularity`
    /// unit (sector/block/byte) at a time.
    ///
    /// Fixed-size fields (`Records::is_fixed_size`) get a permanent reservation packed
    /// back to back at the front of the backend, so they can never fragment. Variable-size
    /// fields share the remaining capacity (the arena), tracked by a small CRC-checked
    /// directory placed right after the fixed region; `emplace<I>()` first tries to place
    /// the newly-encoded value in a free run of the arena, and compacts the arena
    /// (`defragment()`) and retries once if none is big enough. A failed store never
    /// disturbs a field's previous value.
    ///
    /// A field declared with `gba::bits::backup::redundant()` never has a new value written
    /// over its previous one: a fixed-size redundant field alternates between two reserved
    /// slots, a variable-size redundant field is placed in a free arena run that excludes
    /// its own current allocation. Before writing the new copy, a durable directory
    /// generation records its intended location. An interrupted write therefore makes
    /// `get<I>()` report no value, while `get_alternate<I>()` retains the previous committed
    /// value for recovery.
    ///
    /// Every field must share the same `Backend` type. Fields can be reached either by
    /// declaration index or by the field's own (table-unique) value type:
    /// - `get<I>()`/`get<T>()` load the field, `std::nullopt` if never stored or erased.
    /// - `emplace<I>(args...)`/`emplace<T>(args...)` construct a `value_type` from `args`
    ///   (default: none, value-initializing it), store it, and return whether it fit.
    /// - `erase<I>()`/`erase<T>()` invalidate a field.
    /// - `field<I>()`/`field<T>()` reach the underlying `gba::bits::backup::record` directly.
    ///
    /// @code{.cpp}
    /// using player_record = gba::bits::backup::record<player_codec, gba::bits::backup::flash_backend_standard_64k>;
    /// using settings_record = gba::bits::backup::record<settings_codec,
    /// gba::bits::backup::flash_backend_standard_64k>;
    ///
    /// gba::bits::backup::table<player_record, settings_record> saves;
    /// saves.emplace<0>(player_save{...});
    /// saves.emplace<1>(settings_save{...});
    /// @endcode
    template<typename... Records>
        requires(sizeof...(Records) > 0)
    class table {
        using first_backend = std::tuple_element_t<0, std::tuple<Records...>>::backend_type;
        using all_records = std::tuple<Records...>;

        static_assert((std::same_as<typename Records::backend_type, first_backend> && ...),
                      "every record in a table must share the same Backend");

        static constexpr std::size_t record_count = sizeof...(Records);
        static constexpr std::array<bool, record_count> is_fixed_flags = {Records::is_fixed_size...};
        static constexpr std::array<bool, record_count> is_redundant_flags = {
            detail::is_redundant_codec<typename Records::codec_type>::value...};
        static constexpr std::size_t fixed_count = (std::size_t{Records::is_fixed_size} + ...);
        static constexpr std::size_t dynamic_count = record_count - fixed_count;

        template<std::size_t K>
        static consteval std::size_t fixed_slot_of() noexcept {
            std::size_t pos = 0;
            for (std::size_t i = 0; i < K; ++i) {
                if (is_fixed_flags[i]) {
                    ++pos;
                }
            }
            return pos;
        }

        template<typename R>
        using fixed_only = std::conditional_t<R::is_fixed_size, std::tuple<R>, std::tuple<>>;

        using fixed_tuple = decltype(std::tuple_cat(std::declval<fixed_only<Records>>()...));

        using directory_type = detail::field_directory<record_count, first_backend>;
        using directory_entry = directory_type::entry;
        static constexpr std::size_t directory_copy_stride = detail::align_up(directory_type::wire_size,
                                                                              first_backend::granularity);
        static constexpr std::size_t directory_wire_size = directory_copy_stride * 2;

        template<std::size_t K>
        static consteval std::size_t redundant_slot_size() noexcept {
            return detail::align_up(std::tuple_element_t<K, all_records>::wire_size, first_backend::granularity);
        }

        template<std::size_t K>
        static consteval std::size_t reserved_wire_size() noexcept {
            using r = std::tuple_element_t<K, all_records>;
            return is_redundant_flags[K] ? redundant_slot_size<K>() * 2 : r::wire_size;
        }

        template<std::size_t... K>
        static consteval std::array<std::size_t, fixed_count + 1> make_front_wire_sizes(
            std::index_sequence<K...>) noexcept {
            std::array<std::size_t, fixed_count + 1> sizes{};
            ((is_fixed_flags[K] ? (sizes[fixed_slot_of<K>()] = reserved_wire_size<K>(), void()) : void()), ...);
            sizes[fixed_count] = directory_wire_size;
            return sizes;
        }

        static constexpr std::array<std::size_t, fixed_count + 1> front_wire_sizes =
            make_front_wire_sizes(std::index_sequence_for<Records...>{});

        template<std::size_t... K>
        fixed_tuple make_fixed_records(const std::array<std::size_t, fixed_count + 1>& offsets,
                                       std::index_sequence<K...>) {
            return std::tuple_cat(make_fixed_one<K>(offsets)...);
        }

        template<std::size_t K>
        auto make_fixed_one(const std::array<std::size_t, fixed_count + 1>& offsets) {
            using r = std::tuple_element_t<K, all_records>;
            if constexpr (r::is_fixed_size) {
                return std::tuple<r>{r(offsets[fixed_slot_of<K>()])};
            } else {
                return std::tuple<>{};
            }
        }

        template<std::size_t I>
        [[nodiscard]] std::size_t redundant_offset() const noexcept {
            const std::size_t base = m_offsets[fixed_slot_of<I>()];
            return m_entries[I].count == 2 ? base + redundant_slot_size<I>() : base;
        }

        template<std::size_t I>
        [[nodiscard]] auto field_redundant() const noexcept {
            return std::tuple_element_t<I, all_records>(redundant_offset<I>());
        }

        template<std::size_t I>
        [[nodiscard]] std::size_t alternate_offset() const noexcept {
            const std::size_t base = m_offsets[fixed_slot_of<I>()];
            return m_entries[I].count == 2 ? base : base + redundant_slot_size<I>();
        }

        template<std::size_t I>
        [[nodiscard]] auto field_alternate() const noexcept {
            return std::tuple_element_t<I, all_records>(alternate_offset<I>());
        }

    public:
        /// @brief Number of fields, e.g. for `gba::backup`'s field-entry table.
        static constexpr std::size_t size = record_count;

        /// @brief The declared `gba::bits::backup::record` type at declaration index `I`.
        template<std::size_t I>
        using field_type = std::tuple_element_t<I, all_records>;

        table()
            : m_offsets(detail::pack_offsets(front_wire_sizes, first_backend::granularity)),
              m_records(make_fixed_records(m_offsets, std::index_sequence_for<Records...>{})),
              m_arenaStart(detail::total_packed_size(front_wire_sizes, first_backend::granularity)),
              m_directory(directory_type(m_offsets[fixed_count], directory_copy_stride)),
              m_entries(m_directory.load()) {
            static_assert(detail::total_packed_size(front_wire_sizes, first_backend::granularity) <=
                              first_backend::capacity,
                          "table's fixed fields (and directory) do not fit in the backend");
        }

        /// @brief The record at declaration index `I`.
        ///
        /// For a fixed-size field this is a persistent reference; for a variable-size
        /// field it is a fresh snapshot returned by value, reflecting this field's
        /// current allocation at the time of the call.
        template<std::size_t I>
        [[nodiscard]] decltype(auto) field() noexcept {
            if constexpr (is_redundant_flags[I] && is_fixed_flags[I]) {
                return field_redundant<I>();
            } else if constexpr (is_fixed_flags[I]) {
                return std::get<fixed_slot_of<I>()>(m_records);
            } else {
                return field_dynamic<I>();
            }
        }

        /// @copydoc field()
        template<std::size_t I>
        [[nodiscard]] decltype(auto) field() const noexcept {
            if constexpr (is_redundant_flags[I] && is_fixed_flags[I]) {
                return field_redundant<I>();
            } else if constexpr (is_fixed_flags[I]) {
                return std::get<fixed_slot_of<I>()>(m_records);
            } else {
                return field_dynamic<I>();
            }
        }

        /// @brief The one record whose value type is `T`; ill-formed unless exactly one field decodes to `T`.
        template<typename T>
        [[nodiscard]] decltype(auto) field() noexcept {
            return field<index_of<T>()>();
        }

        /// @copydoc field()
        template<typename T>
        [[nodiscard]] decltype(auto) field() const noexcept {
            return field<index_of<T>()>();
        }

        /// @brief Load the field at declaration index `I`; `std::nullopt` if never stored or erased.
        template<std::size_t I>
        [[nodiscard]] std::optional<typename field_type<I>::value_type> get() const {
            if (m_entries[I].count == 0 || m_entries[I].pending_count != 0) {
                return std::nullopt;
            }
            return field<I>().load();
        }

        /// @brief Load the one field whose value type is `T`; ill-formed unless exactly one field decodes to `T`.
        template<typename T>
        [[nodiscard]] std::optional<T> get() const {
            return get<index_of<T>()>();
        }

        /// @brief Load redundant field `I`'s last committed copy when its next save is pending.
        ///
        /// Use this to recover a previous value when the active copy fails a check of the
        /// caller's own devising (e.g. a CRC embedded in the record itself), or to recover
        /// after `get<I>()` reports a pending interrupted save. A fixed-size field also
        /// exposes its inactive slot after a completed save. `std::nullopt` if no previous
        /// committed copy exists. Ill-formed for a non-redundant field.
        template<std::size_t I>
            requires(is_redundant_flags[I])
        [[nodiscard]] std::optional<typename field_type<I>::value_type> get_alternate() const {
            if (m_entries[I].count == 0) {
                return std::nullopt;
            }
            if (m_entries[I].pending_count != 0) {
                return field<I>().load();
            }
            if constexpr (is_fixed_flags[I]) {
                return field_alternate<I>().load();
            } else {
                return std::nullopt;
            }
        }

        /// @copydoc get_alternate()
        template<typename T>
        [[nodiscard]] std::optional<T> get_alternate() const {
            return get_alternate<index_of<T>()>();
        }

        /// @brief Construct a `value_type` from `args` (default: none, value-initializing it)
        /// and store it at declaration index `I`.
        template<std::size_t I, typename... Args>
            requires std::constructible_from<typename field_type<I>::value_type, Args...>
        bool emplace(Args&&... args) {
            using value_type = field_type<I>::value_type;
            const value_type value(std::forward<Args>(args)...);
            if constexpr (is_redundant_flags[I] && is_fixed_flags[I]) {
                if (!field_type<I>{}.encoded_length(value)) {
                    return false;
                }
                const bool wasSlotB = m_entries[I].count == 2;
                const std::size_t base = m_offsets[fixed_slot_of<I>()];
                const std::size_t target = wasSlotB ? base : base + redundant_slot_size<I>();
                m_entries[I].pending_count = static_cast<std::uint16_t>(wasSlotB ? 1 : 2);
                m_directory.store(m_entries);
                if (!field_type<I>(target).store(value)) {
                    m_entries[I].pending_count = 0;
                    m_directory.store(m_entries);
                    return false;
                }
                m_entries[I].count = m_entries[I].pending_count;
                m_entries[I].pending_count = 0;
                m_directory.store(m_entries);
                return true;
            } else if constexpr (is_fixed_flags[I]) {
                if (!field<I>().store(value)) {
                    return false;
                }
                if (m_entries[I].count == 0) {
                    m_entries[I].count = 1;
                    m_directory.store(m_entries);
                }
                return true;
            } else {
                return emplace_dynamic<I>(value);
            }
        }

        /// @brief Construct a `T` from `args` (default: none, value-initializing it) and store it
        /// at the one field whose value type is `T`; ill-formed unless exactly one field decodes to `T`.
        template<typename T, typename... Args>
            requires std::constructible_from<T, Args...>
        bool emplace(Args&&... args) {
            return emplace<index_of<T>()>(std::forward<Args>(args)...);
        }

        /// @brief Erase the field at declaration index `I`; `get<I>()` then reports no value.
        template<std::size_t I>
        void erase() {
            if (m_entries[I].count != 0) {
                m_entries[I] = directory_entry{};
                m_directory.store(m_entries);
            }
        }

        /// @brief Erase the one field whose value type is `T`; ill-formed unless exactly one field decodes to `T`.
        template<typename T>
        void erase() {
            erase<index_of<T>()>();
        }

        /// @brief The granularity-aligned byte offset assigned to fixed-size field `I`.
        ///
        /// Ill-formed for a variable-size field, whose offset can move -- use
        /// `field<I>().offset()` for a snapshot of its current one. Ill-formed for a
        /// redundant fixed-size field, which reserves two offsets -- use `redundant_offsets<I>()`.
        template<std::size_t I>
            requires(is_fixed_flags[I] && !is_redundant_flags[I])
        [[nodiscard]] constexpr std::size_t offset() const noexcept {
            return m_offsets[fixed_slot_of<I>()];
        }

        /// @brief The two granularity-aligned offsets reserved for redundant fixed-size field
        /// `I`: `.first` is where the field's storage begins, `.second` its alternate slot.
        /// `field<I>()` resolves whichever of the two is currently active.
        ///
        /// Ill-formed for a redundant variable-size field -- it has no fixed pair of
        /// offsets, since each new value is placed wherever the arena currently has room
        /// (excluding the field's own current allocation); use `field<I>().offset()` for a
        /// snapshot of whichever copy is currently active.
        template<std::size_t I>
            requires(is_redundant_flags[I] && is_fixed_flags[I])
        [[nodiscard]] constexpr std::pair<std::size_t, std::size_t> redundant_offsets() const noexcept {
            const std::size_t base = m_offsets[fixed_slot_of<I>()];
            return {base, base + redundant_slot_size<I>()};
        }

        /// @brief Whether the dynamic arena's free space is split across more than one run.
        [[nodiscard]] bool is_fragmented() const noexcept {
            if constexpr (dynamic_count == 0) {
                return false;
            } else {
                const std::size_t free = free_granules();
                return free != 0 && free != largest_free_run();
            }
        }

        /// @brief Total unallocated space in the dynamic arena, in `Backend::granularity` units.
        [[nodiscard]] std::size_t free_granules() const noexcept {
            if constexpr (dynamic_count == 0) {
                return 0;
            } else {
                std::size_t used = 0;
                for (std::size_t i = 0; i < record_count; ++i) {
                    if (!is_fixed_flags[i]) {
                        used += m_entries[i].count;
                        used += m_entries[i].pending_count;
                    }
                }
                return arena_granules() - used;
            }
        }

        /// @brief The size of the largest contiguous free run in the dynamic arena, in
        /// `Backend::granularity` units; equal to `free_granules()` unless fragmented.
        [[nodiscard]] std::size_t largest_free_run() const noexcept {
            if constexpr (dynamic_count == 0) {
                return 0;
            } else {
                const auto order = occupied_order();
                std::size_t largest = 0;
                std::size_t cursor = 0;
                for (std::size_t k = 0; k < order.second; ++k) {
                    const auto& e = order.first[k];
                    largest = std::max<std::size_t>(largest, std::size_t{e.start} - cursor);
                    cursor = std::size_t{e.start} + e.count;
                }
                largest = std::max<std::size_t>(largest, arena_granules() - cursor);
                return largest;
            }
        }

        /// @brief Compact every allocated dynamic field to the arena's front, in ascending
        /// order of current position, consolidating free space into one trailing run.
        ///
        /// `emplace<I>()` already calls this automatically (once) when a variable-size
        /// field doesn't fit as-is. A no-op when every field is fixed-size or unfragmented.
        void defragment() {
            if constexpr (dynamic_count > 0) {
                if (has_pending_save()) {
                    return;
                }
                const auto order = occupied_order();
                first_backend backend{};
                std::array<std::byte, granule_buffer_bytes> buffer{};
                std::size_t cursor = 0;
                bool changed = false;
                for (std::size_t k = 0; k < order.second; ++k) {
                    const auto& e = order.first[k];
                    if (e.start != cursor) {
                        move_granules(backend, buffer, e.start, cursor, e.count);
                        for (auto& entry : m_entries) {
                            if (entry.start == e.start && entry.count == e.count) {
                                entry.start = static_cast<std::uint16_t>(cursor);
                                break;
                            }
                        }
                        changed = true;
                    }
                    cursor += e.count;
                }
                if (changed) {
                    m_directory.store(m_entries);
                }
            }
        }

    private:
        template<typename T>
        static consteval std::size_t index_of() {
            const std::array<bool, record_count> matches{std::same_as<typename Records::value_type, T>...};
            std::size_t found = record_count;
            std::size_t count = 0;
            for (std::size_t i = 0; i < matches.size(); ++i) {
                if (matches[i]) {
                    found = i;
                    ++count;
                }
            }
            if (count != 1) {
                constexpr_fail("T must be exactly one record's value type in this table");
            }
            return found;
        }

        [[nodiscard]] std::size_t arena_granules() const noexcept {
            return (first_backend::capacity - m_arenaStart) / first_backend::granularity;
        }

        template<std::size_t I>
        [[nodiscard]] auto field_dynamic() const {
            using r = field_type<I>;
            const std::size_t off = m_arenaStart + std::size_t{m_entries[I].start} * first_backend::granularity;
            return r(off);
        }

        template<std::size_t I>
        bool emplace_dynamic(const field_type<I>::value_type& value) {
            using r = field_type<I>;
            const r probe(m_arenaStart);
            const auto length = probe.encoded_length(value);
            if (!length) {
                return false;
            }
            const std::size_t exact = std::max<std::size_t>(1, granules_for(*length));

            if (place_exact<I>(value, exact)) {
                return true;
            }
            defragment();
            return place_exact<I>(value, exact);
        }

        template<std::size_t I>
        bool place_exact(const field_type<I>::value_type& value, const std::size_t needed) {
            const auto found = find_free_run(I, needed, !is_redundant_flags[I]);
            if (!found) {
                return false;
            }
            field_type<I> rec(m_arenaStart + *found * first_backend::granularity);
            if constexpr (is_redundant_flags[I]) {
                m_entries[I].pending_start = static_cast<std::uint16_t>(*found);
                m_entries[I].pending_count = static_cast<std::uint16_t>(needed);
                m_directory.store(m_entries);
            }
            if (!rec.store(value)) {
                if constexpr (is_redundant_flags[I]) {
                    m_entries[I].pending_start = 0;
                    m_entries[I].pending_count = 0;
                    m_directory.store(m_entries);
                }
                return false;
            }
            commit_dynamic<I>(*found, needed);
            return true;
        }

        template<std::size_t I>
        void commit_dynamic(const std::size_t start, const std::size_t count) {
            m_entries[I].start = static_cast<std::uint16_t>(start);
            m_entries[I].count = static_cast<std::uint16_t>(count);
            m_entries[I].pending_start = 0;
            m_entries[I].pending_count = 0;
            m_directory.store(m_entries);
        }

        [[nodiscard]] static std::size_t granules_for(const std::size_t bytes) noexcept {
            return (bytes + first_backend::granularity - 1) / first_backend::granularity;
        }

        static constexpr std::size_t granule_buffer_bytes = first_backend::granularity;

        void move_granules(first_backend& backend, std::span<std::byte> buffer, const std::size_t fromGranule,
                           const std::size_t toGranule, const std::size_t granuleCount) const {
            const std::size_t granuleBytes = first_backend::granularity;
            const std::span<std::byte> buf = buffer.subspan(0, granuleBytes);
            for (std::size_t g = 0; g < granuleCount; ++g) {
                const std::size_t src = m_arenaStart + ((fromGranule + g) * granuleBytes);
                const std::size_t dst = m_arenaStart + ((toGranule + g) * granuleBytes);
                backend.read(src, buf);
                backend.write(dst, buf);
            }
        }

        [[nodiscard]] bool overlaps(const std::size_t skip, const std::size_t start, const std::size_t count,
                                    const bool allowSelfOverlap) const noexcept {
            for (std::size_t i = 0; i < record_count; ++i) {
                if ((allowSelfOverlap && i == skip) || is_fixed_flags[i]) {
                    continue;
                }
                if (overlaps_range(start, count, m_entries[i].start, m_entries[i].count) ||
                    overlaps_range(start, count, m_entries[i].pending_start, m_entries[i].pending_count)) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::optional<std::size_t> find_free_run(const std::size_t skip, const std::size_t needed,
                                                               const bool allowSelfOverlap = true) const noexcept {
            const std::size_t total = arena_granules();
            std::size_t candidate = 0;
            while (candidate + needed <= total) {
                if (!overlaps(skip, candidate, needed, allowSelfOverlap)) {
                    return candidate;
                }
                std::size_t advance = candidate + 1;
                for (std::size_t i = 0; i < record_count; ++i) {
                    if ((allowSelfOverlap && i == skip) || is_fixed_flags[i]) {
                        continue;
                    }
                    advance = advance_past_overlap(candidate, needed, m_entries[i].start, m_entries[i].count, advance);
                    advance = advance_past_overlap(candidate, needed, m_entries[i].pending_start,
                                                   m_entries[i].pending_count, advance);
                }
                candidate = advance;
            }
            return std::nullopt;
        }

        [[nodiscard]] static bool overlaps_range(const std::size_t start, const std::size_t count,
                                                 const std::uint16_t occupiedStart,
                                                 const std::uint16_t occupiedCount) noexcept {
            return occupiedCount != 0 && start < std::size_t{occupiedStart} + occupiedCount &&
                   occupiedStart < start + count;
        }

        [[nodiscard]] static std::size_t advance_past_overlap(const std::size_t candidate, const std::size_t needed,
                                                              const std::uint16_t occupiedStart,
                                                              const std::uint16_t occupiedCount,
                                                              const std::size_t advance) noexcept {
            if (overlaps_range(candidate, needed, occupiedStart, occupiedCount)) {
                return std::max(advance, std::size_t{occupiedStart} + occupiedCount);
            }
            return advance;
        }

        [[nodiscard]] bool has_pending_save() const noexcept {
            for (const auto& entry : m_entries) {
                if (entry.pending_count != 0) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::pair<std::array<directory_entry, dynamic_count * 2>, std::size_t> occupied_order()
            const noexcept {
            std::array<directory_entry, dynamic_count * 2> order{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < record_count; ++i) {
                if (!is_fixed_flags[i] && m_entries[i].count != 0) {
                    order[n++] = m_entries[i];
                }
                if (!is_fixed_flags[i] && m_entries[i].pending_count != 0) {
                    order[n++] = {.start = m_entries[i].pending_start, .count = m_entries[i].pending_count};
                }
            }
            for (std::size_t a = 1; a < n; ++a) {
                std::size_t b = a;
                while (b > 0 && order[b - 1].start > order[b].start) {
                    std::swap(order[b - 1], order[b]);
                    --b;
                }
            }
            return {order, n};
        }

        std::array<std::size_t, fixed_count + 1> m_offsets;
        fixed_tuple m_records;
        std::size_t m_arenaStart = 0;
        directory_type m_directory{};
        std::array<directory_entry, record_count> m_entries{};
    };

} // namespace gba::bits::backup
