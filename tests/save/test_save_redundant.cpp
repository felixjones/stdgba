/// @file test_save_redundant.cpp
/// @brief Tests for gba::backup_redundant()'s power-loss-safe placement, for both
/// fixed-size (ping-pong A/B slots) and variable-size (self-overlap-excluding arena
/// placement) codecs. Uses small in-memory mock backends so the exact byte layout at
/// every step is known and every physical slot can be poked directly to observe its
/// effect (or lack thereof) on get().

#include <gba/backup>
#include <gba/testing>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

    struct mem_backend {
        static constexpr std::size_t capacity = 64;
        static constexpr std::size_t granularity = 4;

        void read(const std::size_t offset, const std::span<std::byte> out) const noexcept {
            for (std::size_t i = 0; i < out.size(); ++i) out[i] = storage[offset + i];
        }

        void write(const std::size_t offset, const std::span<const std::byte> in) noexcept {
            for (std::size_t i = 0; i < in.size() && write_budget != 0; ++i) {
                storage[offset + i] = in[i];
                if (write_budget != std::numeric_limits<std::size_t>::max()) {
                    --write_budget;
                }
            }
        }

        static inline std::array<std::byte, capacity> storage{};
        static inline std::size_t write_budget = std::numeric_limits<std::size_t>::max();
    };

    static_assert(gba::bits::backup::Backend<mem_backend>);

    constexpr auto redundant_codec = gba::backup_redundant(gba::codec::int8_codec);
    using redundant_record = gba::bits::backup::record<redundant_codec, mem_backend>;
    using plain_record = gba::bits::backup::record<gba::codec::int16_codec, mem_backend>;

    static_assert(redundant_record::is_fixed_size);
    static_assert(redundant_record::wire_size == 1);

    using table_type = gba::bits::backup::table<redundant_record, plain_record>;

    // granularity 4: field 0 (redundant, 1-byte payload) reserves two 4-byte-aligned
    // slots at [0, 4) and [4, 8); field 1 (plain, 2 bytes) starts right after at 8.
    constexpr std::size_t slot_a = 0;
    constexpr std::size_t slot_b = 4;
    constexpr std::size_t plain_offset = 8;

    void poke(const std::size_t offset, const std::int8_t value) {
        gba::bits::backup::record<gba::codec::int8_codec, mem_backend> probe(offset);
        probe.store(value);
    }

    // A second, byte-granularity backend/table pair for the variable-size (arena) side of
    // redundant placement, where slots aren't fixed reservations but "wherever fits".
    struct dyn_backend {
        static constexpr std::size_t capacity = 80;
        static constexpr std::size_t granularity = 1;

        void read(const std::size_t offset, const std::span<std::byte> out) const noexcept {
            for (std::size_t i = 0; i < out.size(); ++i) out[i] = storage[offset + i];
        }

        void write(const std::size_t offset, const std::span<const std::byte> in) noexcept {
            for (std::size_t i = 0; i < in.size() && write_budget != 0; ++i) {
                storage[offset + i] = in[i];
                if (write_budget != std::numeric_limits<std::size_t>::max()) {
                    --write_budget;
                }
            }
        }

        static inline std::array<std::byte, capacity> storage{};
        static inline std::size_t write_budget = std::numeric_limits<std::size_t>::max();
    };

    static_assert(gba::bits::backup::Backend<dyn_backend>);

    constexpr auto bytes_codec = gba::codec::vector_of(gba::codec::uint8_codec);
    constexpr auto redundant_bytes_codec = gba::backup_redundant(bytes_codec);

    using redundant_bytes_record = gba::bits::backup::record<redundant_bytes_codec, dyn_backend, 32>;
    using plain_bytes_record = gba::bits::backup::record<bytes_codec, dyn_backend, 32>;

    static_assert(!redundant_bytes_record::is_fixed_size);

    using dyn_table_type = gba::bits::backup::table<redundant_bytes_record, plain_bytes_record>;

    void poke_dyn(const std::size_t offset, const std::uint8_t byte) {
        dyn_backend backend{};
        const std::array<std::byte, 1> raw = {std::byte{byte}};
        backend.write(offset, raw);
    }

    // A third backend/table, with a filler field either side of the redundant one, so
    // erasing the middle filler leaves a hole genuinely sandwiched between two occupied
    // runs -- only defragment() can turn it (plus the tail) into one run large enough
    // to grow the redundant field into.
    struct frag_backend {
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

    static_assert(gba::bits::backup::Backend<frag_backend>);

    constexpr auto redundant_frag_codec = gba::backup_redundant(bytes_codec);
    using redundant_frag_record = gba::bits::backup::record<redundant_frag_codec, frag_backend, 32>;
    using filler_frag_record = gba::bits::backup::record<bytes_codec, frag_backend, 32>;

    using frag_table_type = gba::bits::backup::table<redundant_frag_record, filler_frag_record, filler_frag_record>;

} // namespace

int main() {
    gba::test("a redundant field reserves two independently aligned slots at the front", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        const auto [a, b] = saves.redundant_offsets<0>();
        gba::test.expect.eq(a, slot_a);
        gba::test.expect.eq(b, slot_b);
        gba::test.expect.eq(saves.offset<1>(), plain_offset);
    });

    gba::test("get() reports no value before the first emplace, despite garbage-filled slots", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        gba::test.expect.is_false(saves.get<0>().has_value());
    });

    gba::test("successive emplaces alternate slots, and corrupting the inactive one never affects get()", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        // First emplace lands on slot B; slot A is still whatever garbage was there.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{1}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{1});

        // Directly corrupt the now-inactive slot A; it must not be observable.
        poke(slot_a, std::int8_t{-99});
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{1});

        // Second emplace flips to slot A, overwriting the corruption we just wrote.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{2}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{2});

        // Now slot B is inactive; corrupting it must not be observable either.
        poke(slot_b, std::int8_t{-88});
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{2});

        // Third emplace flips back to slot B.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{3}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{3});
    });

    gba::test("get_alternate() reports no value before the first emplace", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        gba::test.expect.is_false(saves.get_alternate<0>().has_value());
    });

    gba::test("get_alternate() loads whichever slot get() does not, tracking each emplace", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        mem_backend::write_budget = std::numeric_limits<std::size_t>::max();
        table_type saves;

        // First emplace lands on slot B; slot A is untouched garbage, so its "value"
        // is unspecified but get_alternate() must still surface exactly what's there.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{1}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{1});
        poke(slot_a, std::int8_t{7});
        gba::test.expect.eq(*saves.get_alternate<0>(), std::int8_t{7});

        // Second emplace flips to slot A (now active); slot B (holding the first value)
        // becomes the alternate, recoverable via get_alternate() alone.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{2}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{2});
        gba::test.expect.eq(*saves.get_alternate<0>(), std::int8_t{1});

        // Third emplace flips back to slot B; slot A (holding the second value) is now
        // the alternate.
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{3}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{3});
        gba::test.expect.eq(*saves.get_alternate<0>(), std::int8_t{2});
    });

    gba::test("an interrupted fixed-size redundant save signals corruption and retains the prior value", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        mem_backend::write_budget = std::numeric_limits<std::size_t>::max();
        table_type saves;

        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{1}));

        // One directory generation is 20 bytes: two 8-byte entries, a generation,
        // and a CRC. Let the pending intent finish, then cut power before the payload.
        mem_backend::write_budget = 20;
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{2}));
        mem_backend::write_budget = std::numeric_limits<std::size_t>::max();

        table_type rebooted;
        gba::test.expect.is_false(rebooted.get<0>().has_value());
        gba::test.expect.eq(*rebooted.get_alternate<0>(), std::int8_t{1});
    });

    gba::test("a torn redundant save intent preserves the preceding directory generation", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        mem_backend::write_budget = std::numeric_limits<std::size_t>::max();
        table_type saves;

        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{1}));

        // The attempted intent cannot pass its CRC after five written bytes, so reboot
        // must select the older committed directory rather than invent a pending save.
        mem_backend::write_budget = 5;
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{2}));
        mem_backend::write_budget = std::numeric_limits<std::size_t>::max();

        table_type rebooted;
        gba::test.expect.eq(*rebooted.get<0>(), std::int8_t{1});
    });

    gba::test("a plain fixed field alongside a redundant one is unaffected by its alternation", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        gba::test.expect.is_true(saves.emplace<1>(std::int16_t{4242}));
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{1}));
        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{2}));

        gba::test.expect.eq(*saves.get<1>(), std::int16_t{4242});
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{2});
    });

    gba::test("erase() clears a redundant field, and it can be emplaced again afterwards", [] {
        mem_backend::storage.fill(std::byte{0xFF});
        table_type saves;

        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{7}));
        saves.erase<0>();
        gba::test.expect.is_false(saves.get<0>().has_value());

        gba::test.expect.is_true(saves.emplace<0>(std::int8_t{9}));
        gba::test.expect.eq(*saves.get<0>(), std::int8_t{9});
    });

    gba::test("a redundant variable-size field alternates between two non-overlapping arena runs", [] {
        dyn_backend::storage.fill(std::byte{0xFF});
        dyn_table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint8_t> b = {9, 8, 7, 6, 5};
        const std::vector<std::uint8_t> c = {0, 1, 0, 1, 0};

        // Same-size values (4-byte length prefix + 5 bytes = 9 granules), so successive
        // emplaces ping-pong between [0, 9) and [9, 18) relative to the arena.
        gba::test.expect.is_true(saves.emplace<0>(a));
        const std::size_t run1 = saves.field<0>().offset();
        gba::test.expect.eq(*saves.get<0>(), a);

        gba::test.expect.is_true(saves.emplace<0>(b));
        const std::size_t run2 = saves.field<0>().offset();
        gba::test.expect.eq(*saves.get<0>(), b);
        gba::test.expect.is_true(run2 != run1);

        gba::test.expect.is_true(saves.emplace<0>(c));
        const std::size_t run3 = saves.field<0>().offset();
        gba::test.expect.eq(*saves.get<0>(), c);
        gba::test.expect.eq(run3, run1);
    });

    gba::test("corrupting a redundant variable-size field's currently-unreferenced run never affects get()", [] {
        dyn_backend::storage.fill(std::byte{0xFF});
        dyn_backend::write_budget = std::numeric_limits<std::size_t>::max();
        dyn_table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint8_t> b = {9, 8, 7, 6, 5};

        gba::test.expect.is_true(saves.emplace<0>(a));
        const std::size_t run1 = saves.field<0>().offset();

        // The next emplace will need 9 granules and must not reuse run1, so it lands
        // right after it; corrupt that predicted run before it's ever written for real.
        poke_dyn(run1 + 9, 0xAA);
        gba::test.expect.eq(*saves.get<0>(), a);

        gba::test.expect.is_true(saves.emplace<0>(b));
        gba::test.expect.eq(*saves.get<0>(), b);

        // Now run1 is unreferenced; corrupting it must not affect the current value either.
        poke_dyn(run1, 0xBB);
        gba::test.expect.eq(*saves.get<0>(), b);
    });

    gba::test("an interrupted dynamic redundant save signals corruption and retains the prior value", [] {
        dyn_backend::storage.fill(std::byte{0xFF});
        dyn_backend::write_budget = std::numeric_limits<std::size_t>::max();
        dyn_table_type saves;

        const std::vector<std::uint8_t> a = {1, 2, 3, 4, 5};
        const std::vector<std::uint8_t> b = {9, 8, 7, 6, 5};
        gba::test.expect.is_true(saves.emplace<0>(a));

        // Persist the 20-byte pending directory generation, then allow only three
        // payload bytes before the simulated power loss.
        dyn_backend::write_budget = 23;
        gba::test.expect.is_true(saves.emplace<0>(b));
        dyn_backend::write_budget = std::numeric_limits<std::size_t>::max();

        dyn_table_type rebooted;
        gba::test.expect.is_false(rebooted.get<0>().has_value());
        gba::test.expect.eq(*rebooted.get_alternate<0>(), a);
    });

    gba::test("a redundant variable-size field automatically defragments and retries, like a plain one", [] {
        frag_backend::storage.fill(std::byte{0xFF});
        frag_table_type saves;

        const std::vector<std::uint8_t> r = {1, 2, 3, 4, 5};
        const std::vector<std::uint8_t> filler_a = {6, 7, 8, 9, 10};
        const std::vector<std::uint8_t> filler_b = {11, 12, 13, 14, 15};

        gba::test.expect.is_true(saves.emplace<0>(r));
        gba::test.expect.is_true(saves.emplace<1>(filler_a));
        gba::test.expect.is_true(saves.emplace<2>(filler_b));
        const std::size_t r_run = saves.field<0>().offset();

        // Freeing the sandwiched filler leaves a hole bordered by the redundant field on
        // one side and the surviving filler on the other: no single free run (the hole,
        // or the tail) is enough for the grown value, only their sum after a defragment.
        saves.erase<1>();
        gba::test.expect.is_true(saves.is_fragmented());

        std::vector<std::uint8_t> grown_r(20);
        for (std::size_t i = 0; i < grown_r.size(); ++i) grown_r[i] = static_cast<std::uint8_t>(i);
        gba::test.expect.is_true(20 + 4 > saves.largest_free_run());

        gba::test.expect.is_true(saves.emplace<0>(grown_r));

        gba::test.expect.eq(*saves.get<0>(), grown_r);
        gba::test.expect.eq(*saves.get<2>(), filler_b);
        // The redundant field must have actually relocated, never overwritten in place.
        gba::test.expect.is_true(saves.field<0>().offset() != r_run);
    });

    return gba::test.finish();
}
