/// @file tests/memory/test_bitpool.cpp
/// @brief Unit tests for bitpool optimizations.

#include <gba/memory>
#include <gba/testing>

int main() {
    using namespace gba;

    // Test basic allocation
    {
        char buffer[1024];
        bitpool pool{buffer, 32}; // 32-byte chunks, 32 chunks = 1024 bytes

        void* p1 = pool.allocate(32);
        gba::test.nz(p1 != nullptr);
        gba::test.is_true(p1 == static_cast<void*>(buffer));

        void* p2 = pool.allocate(32);
        gba::test.nz(p2 != nullptr);
        gba::test.is_true(p2 == static_cast<void*>(buffer + 32));
    }

    // Test deallocation
    {
        char buffer[1024];
        bitpool pool{buffer, 32};

        void* p1 = pool.allocate(32);
        void* p2 = pool.allocate(32);

        pool.deallocate(p1, 32);

        // Should reuse freed slot
        void* p3 = pool.allocate(32);
        gba::test.is_true(p1 == p3);
    }

    // Test power-of-two chunk size (shift optimization)
    {
        char buffer[256];
        bitpool pool{buffer, 8}; // Power of 2 chunk size

        void* p1 = pool.allocate(8);
        gba::test.nz(p1 != nullptr);

        void* p2 = pool.allocate(16); // 2 chunks
        gba::test.nz(p2 != nullptr);

        pool.deallocate(p1, 8);
        pool.deallocate(p2, 16);

        // Should be able to reallocate
        void* p3 = pool.allocate(24); // 3 chunks
        gba::test.nz(p3 != nullptr);
    }

    // Test capacity
    {
        char buffer[128];
        bitpool pool{buffer, 4}; // 4-byte chunks

        gba::test.eq(pool.capacity(), 32u);
        gba::test.eq(pool.chunk_size(), 4u);
        gba::test.eq(pool.size(), 128u);
    }

    // Test aligned allocation with power-of-two
    {
        char buffer[256];
        bitpool pool{buffer, 16}; // 16-byte chunks

        void* p1 = pool.allocate(16, 32); // Align to 32 bytes
        auto addr = reinterpret_cast<std::uintptr_t>(p1);
        gba::test.eq(addr % 32, 0u);
    }

    // Test exhaustion
    {
        char buffer[64];
        bitpool pool{buffer, 2}; // 2-byte chunks, 32 total

        // Allocate all
        for (int i = 0; i < 32; ++i) {
            void* p = pool.allocate(2);
            gba::test.nz(p != nullptr);
        }

        // Should fail now
        void* p = pool.allocate(2);
        gba::test.is_true(p == nullptr);
    }

    return gba::test.finish();
}
