/// @file test_save_defrag.cpp
/// @brief Tests for gba::bits::backup::table's dynamic-arena defragmentation, using a
/// small in-memory mock backend so the exact byte layout at every step is known and
/// deterministic (real backends are either too large to conveniently exhaust, like
/// SRAM's 32KB, or too slow to simulate many times over, like Flash's sector erase).

#include <gba/backup>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

    /// @brief Minimal `gba::bits::backup::Backend`: 96 bytes, byte granularity, backed by a
    /// plain static array so every `mem_backend` instance shares the same "media", exactly
    /// like the real hardware-backed backends do.
    struct mem_backend {
        static constexpr std::size_t capacity = 100;
        static constexpr std::size_t granularity = 1;

        void read(const std::size_t offset, const std::span<std::byte> out) const noexcept {
            for (std::size_t i = 0; i < out.size(); ++i) out[i] = storage[offset + i];
        }

        void write(const std::size_t offset, const std::span<const std::byte> in) noexcept {
            for (std::size_t i = 0; i < in.size(); ++i) storage[offset + i] = in[i];
        }

        static inline std::array<std::byte, capacity> storage{};
    };

    static_assert(gba::bits::backup::Backend<mem_backend>);

    constexpr auto bytes_codec = gba::codec::vector_of(gba::codec::uint8_codec);
    constexpr auto shorts_codec = gba::codec::vector_of(gba::codec::uint16_codec);

    // Both codecs are variable-size (no `encoded_size`), so Capacity must be given
    // explicitly; 64 comfortably bounds every value these tests store.
    using bytes_record = gba::bits::backup::record<bytes_codec, mem_backend, 64>;
    using shorts_record = gba::bits::backup::record<shorts_codec, mem_backend, 64>;

    static_assert(!bytes_record::is_fixed_size);
    static_assert(!shorts_record::is_fixed_size);

    using table_type = gba::bits::backup::table<bytes_record, shorts_record>;

    // Two alternating directory copies: 2 fields * 8 bytes/entry, plus a 2-byte
    // generation and 2-byte CRC per copy, so the arena starts at byte 40.
    constexpr std::size_t arena_start = 40;

    // A record has no framing of its own; only `range_codec`'s own 4-byte length
    // prefix sits on top of the codec's payload.
    [[nodiscard]] constexpr std::size_t needed_granules(const std::size_t count,
                                                        const std::size_t element_size) noexcept {
        return 4 + count * element_size;
    }

} // namespace

int main() {
    gba::test("fresh table has one contiguous free run and is not fragmented", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        gba::test.expect.is_false(saves.is_fragmented());
        gba::test.expect.eq(saves.free_granules(), mem_backend::capacity - arena_start);
        gba::test.expect.eq(saves.largest_free_run(), mem_backend::capacity - arena_start);
    });

    gba::test("erasing the first of two fields fragments the arena", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint16_t> b = {10, 20, 30};
        gba::test.expect.is_true(saves.emplace<0>(a));
        gba::test.expect.is_true(saves.emplace<1>(b));

        saves.erase<0>();

        const std::size_t needed_a = needed_granules(5, 1);
        const std::size_t needed_b = needed_granules(3, 2);
        const std::size_t arena_granules = mem_backend::capacity - arena_start;
        const std::size_t expected_free = needed_a + (arena_granules - needed_a - needed_b);

        gba::test.expect.is_true(saves.is_fragmented());
        gba::test.expect.eq(saves.free_granules(), expected_free);
        gba::test.expect.eq(saves.largest_free_run(), arena_granules - needed_a - needed_b);

        gba::test.expect.is_false(saves.get<0>().has_value());
        const auto loaded_b = saves.get<1>();
        gba::test.expect.is_true(loaded_b.has_value());
        gba::test.expect.eq(loaded_b->size(), std::size_t{3});
        gba::test.expect.eq((*loaded_b)[0], std::uint16_t{10});
    });

    gba::test("defragment() consolidates free space and preserves the surviving field", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint16_t> b = {10, 20, 30};
        gba::test.expect.is_true(saves.emplace<0>(a));
        gba::test.expect.is_true(saves.emplace<1>(b));
        saves.erase<0>();

        saves.defragment();

        gba::test.expect.is_false(saves.is_fragmented());

        const auto loaded_b = saves.get<1>();
        gba::test.expect.is_true(loaded_b.has_value());
        gba::test.expect.eq(*loaded_b, b);

        // b was the only occupied field, so it should have moved to the arena's start.
        gba::test.expect.eq(saves.field<1>().offset(), arena_start);
    });

    gba::test("emplace automatically defragments and retries when nothing else fits", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint16_t> b = {10, 20, 30};
        gba::test.expect.is_true(saves.emplace<0>(a));
        gba::test.expect.is_true(saves.emplace<1>(b));

        // Free A's space, then defragment so occupied B moves to the arena's front,
        // leaving A's old slot and the tail as two disjoint free runs.
        saves.erase<0>();
        saves.defragment();

        // Re-populate A immediately after B.
        gba::test.expect.is_true(saves.emplace<0>(a));

        // Now free B's space and grow it past what either single free run (A's old
        // slot, or the arena's tail) can hold alone -- only their sum, after another
        // defragment, is big enough.
        saves.erase<1>();

        const std::size_t arena_granules = mem_backend::capacity - arena_start;
        const std::size_t needed_a = needed_granules(5, 1);
        const std::size_t before_free = saves.free_granules();
        gba::test.expect.eq(before_free, arena_granules - needed_a);
        gba::test.expect.is_true(saves.is_fragmented());

        std::vector<std::uint16_t> grown_b(22);
        for (std::size_t i = 0; i < grown_b.size(); ++i) grown_b[i] = static_cast<std::uint16_t>(i);
        const std::size_t needed_grown_b = needed_granules(grown_b.size(), 2);
        gba::test.expect.is_true(needed_grown_b > saves.largest_free_run());
        gba::test.expect.is_true(needed_grown_b <= before_free);

        gba::test.expect.is_true(saves.emplace<1>(grown_b));

        gba::test.expect.is_false(saves.is_fragmented());
        const auto loaded_a = saves.get<0>();
        const auto loaded_b = saves.get<1>();
        gba::test.expect.is_true(loaded_a.has_value());
        gba::test.expect.is_true(loaded_b.has_value());
        gba::test.expect.eq(*loaded_a, a);
        gba::test.expect.eq(*loaded_b, grown_b);
    });

    gba::test("emplace fails, without disturbing the existing value, when even a full defragment can't fit it", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        gba::test.expect.is_true(saves.emplace<0>(a));

        // Ask for more elements than the entire arena could ever hold, regardless of
        // fragmentation.
        const std::size_t arena_granules = mem_backend::capacity - arena_start;
        std::vector<std::uint16_t> too_big(arena_granules, std::uint16_t{7});

        gba::test.expect.is_false(saves.emplace<1>(too_big));

        // The failed store must not have disturbed the surviving field.
        const auto loaded_a = saves.get<0>();
        gba::test.expect.is_true(loaded_a.has_value());
        gba::test.expect.eq(*loaded_a, a);
    });

    return gba::test.finish();
}
