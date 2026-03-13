/// @file tests/overlay/test_overlay.cpp
/// @brief Tests for EWRAM/IWRAM overlay metadata and LMA conflict detection.
///
/// This test places data in all 10 EWRAM overlays, all 10 IWRAM overlays,
/// AND in the regular .ewram/.iwram data sections. Overlays have varying
/// sizes (4 to 32 words) to stress-test LMA packing. If the linker script
/// LMA calculations are wrong, the non-overlay section data will be corrupted
/// by overlapping overlay LMAs, causing assertion failures.

#include <gba/bios>
#include <gba/overlay>
#include <gba/testing>

#include <cstdint>

// Regular .ewram data (non-overlay, initialized by CRT0 at startup)
// If any EWRAM overlay LMA overlaps this section's LMA, these will be corrupt.
[[gnu::section(".ewram")]]
volatile unsigned int ewram_data[8] = {0xE0E0E0E0, 0xE1E1E1E1, 0xE2E2E2E2, 0xE3E3E3E3,
                                       0xE4E4E4E4, 0xE5E5E5E5, 0xE6E6E6E6, 0xE7E7E7E7};

// Regular .iwram data (non-overlay, initialized by CRT0 at startup)
// If any IWRAM overlay LMA overlaps this section's LMA, these will be corrupt.
[[gnu::section(".iwram")]]
volatile unsigned int iwram_data[8] = {0x10101010, 0x11111111, 0x12121212, 0x13131313,
                                       0x14141414, 0x15151515, 0x16161616, 0x17171717};

// EWRAM overlays 0-9: varying sizes (4, 8, 12, 16, 20, 24, 28, 32, 4, 8 words)
// Pattern: value = (overlay_index << 24) | word_index
[[gnu::section(".ewram0")]] unsigned int ew0[4] = {0xE0000000, 0xE0000001, 0xE0000002, 0xE0000003};
[[gnu::section(".ewram1")]] unsigned int ew1[8] = {0xE1000000, 0xE1000001, 0xE1000002, 0xE1000003,
                                                   0xE1000004, 0xE1000005, 0xE1000006, 0xE1000007};
[[gnu::section(".ewram2")]] unsigned int ew2[12] = {0xE2000000, 0xE2000001, 0xE2000002, 0xE2000003,
                                                    0xE2000004, 0xE2000005, 0xE2000006, 0xE2000007,
                                                    0xE2000008, 0xE2000009, 0xE200000A, 0xE200000B};
[[gnu::section(".ewram3")]] unsigned int ew3[16] = {0xE3000000, 0xE3000001, 0xE3000002, 0xE3000003,
                                                    0xE3000004, 0xE3000005, 0xE3000006, 0xE3000007,
                                                    0xE3000008, 0xE3000009, 0xE300000A, 0xE300000B,
                                                    0xE300000C, 0xE300000D, 0xE300000E, 0xE300000F};
[[gnu::section(".ewram4")]] unsigned int ew4[20] = {0xE4000000, 0xE4000001, 0xE4000002, 0xE4000003, 0xE4000004,
                                                    0xE4000005, 0xE4000006, 0xE4000007, 0xE4000008, 0xE4000009,
                                                    0xE400000A, 0xE400000B, 0xE400000C, 0xE400000D, 0xE400000E,
                                                    0xE400000F, 0xE4000010, 0xE4000011, 0xE4000012, 0xE4000013};
[[gnu::section(".ewram5")]] unsigned int ew5[24] = {0xE5000000, 0xE5000001, 0xE5000002, 0xE5000003, 0xE5000004,
                                                    0xE5000005, 0xE5000006, 0xE5000007, 0xE5000008, 0xE5000009,
                                                    0xE500000A, 0xE500000B, 0xE500000C, 0xE500000D, 0xE500000E,
                                                    0xE500000F, 0xE5000010, 0xE5000011, 0xE5000012, 0xE5000013,
                                                    0xE5000014, 0xE5000015, 0xE5000016, 0xE5000017};
[[gnu::section(".ewram6")]] unsigned int ew6[28] = {0xE6000000, 0xE6000001, 0xE6000002, 0xE6000003, 0xE6000004,
                                                    0xE6000005, 0xE6000006, 0xE6000007, 0xE6000008, 0xE6000009,
                                                    0xE600000A, 0xE600000B, 0xE600000C, 0xE600000D, 0xE600000E,
                                                    0xE600000F, 0xE6000010, 0xE6000011, 0xE6000012, 0xE6000013,
                                                    0xE6000014, 0xE6000015, 0xE6000016, 0xE6000017, 0xE6000018,
                                                    0xE6000019, 0xE600001A, 0xE600001B};
[[gnu::section(".ewram7")]] unsigned int ew7[32] = {
    0xE7000000, 0xE7000001, 0xE7000002, 0xE7000003, 0xE7000004, 0xE7000005, 0xE7000006, 0xE7000007,
    0xE7000008, 0xE7000009, 0xE700000A, 0xE700000B, 0xE700000C, 0xE700000D, 0xE700000E, 0xE700000F,
    0xE7000010, 0xE7000011, 0xE7000012, 0xE7000013, 0xE7000014, 0xE7000015, 0xE7000016, 0xE7000017,
    0xE7000018, 0xE7000019, 0xE700001A, 0xE700001B, 0xE700001C, 0xE700001D, 0xE700001E, 0xE700001F};
[[gnu::section(".ewram8")]] unsigned int ew8[4] = {0xE8000000, 0xE8000001, 0xE8000002, 0xE8000003};
[[gnu::section(".ewram9")]] unsigned int ew9[8] = {0xE9000000, 0xE9000001, 0xE9000002, 0xE9000003,
                                                   0xE9000004, 0xE9000005, 0xE9000006, 0xE9000007};

// IWRAM overlays 0-9: varying sizes (8, 4, 16, 12, 32, 28, 20, 24, 8, 4 words)
[[gnu::section(".iwram0")]] unsigned int iw0[8] = {0x10000000, 0x10000001, 0x10000002, 0x10000003,
                                                   0x10000004, 0x10000005, 0x10000006, 0x10000007};
[[gnu::section(".iwram1")]] unsigned int iw1[4] = {0x11000000, 0x11000001, 0x11000002, 0x11000003};
[[gnu::section(".iwram2")]] unsigned int iw2[16] = {0x12000000, 0x12000001, 0x12000002, 0x12000003,
                                                    0x12000004, 0x12000005, 0x12000006, 0x12000007,
                                                    0x12000008, 0x12000009, 0x1200000A, 0x1200000B,
                                                    0x1200000C, 0x1200000D, 0x1200000E, 0x1200000F};
[[gnu::section(".iwram3")]] unsigned int iw3[12] = {0x13000000, 0x13000001, 0x13000002, 0x13000003,
                                                    0x13000004, 0x13000005, 0x13000006, 0x13000007,
                                                    0x13000008, 0x13000009, 0x1300000A, 0x1300000B};
[[gnu::section(".iwram4")]] unsigned int iw4[32] = {
    0x14000000, 0x14000001, 0x14000002, 0x14000003, 0x14000004, 0x14000005, 0x14000006, 0x14000007,
    0x14000008, 0x14000009, 0x1400000A, 0x1400000B, 0x1400000C, 0x1400000D, 0x1400000E, 0x1400000F,
    0x14000010, 0x14000011, 0x14000012, 0x14000013, 0x14000014, 0x14000015, 0x14000016, 0x14000017,
    0x14000018, 0x14000019, 0x1400001A, 0x1400001B, 0x1400001C, 0x1400001D, 0x1400001E, 0x1400001F};
[[gnu::section(".iwram5")]] unsigned int iw5[28] = {0x15000000, 0x15000001, 0x15000002, 0x15000003, 0x15000004,
                                                    0x15000005, 0x15000006, 0x15000007, 0x15000008, 0x15000009,
                                                    0x1500000A, 0x1500000B, 0x1500000C, 0x1500000D, 0x1500000E,
                                                    0x1500000F, 0x15000010, 0x15000011, 0x15000012, 0x15000013,
                                                    0x15000014, 0x15000015, 0x15000016, 0x15000017, 0x15000018,
                                                    0x15000019, 0x1500001A, 0x1500001B};
[[gnu::section(".iwram6")]] unsigned int iw6[20] = {0x16000000, 0x16000001, 0x16000002, 0x16000003, 0x16000004,
                                                    0x16000005, 0x16000006, 0x16000007, 0x16000008, 0x16000009,
                                                    0x1600000A, 0x1600000B, 0x1600000C, 0x1600000D, 0x1600000E,
                                                    0x1600000F, 0x16000010, 0x16000011, 0x16000012, 0x16000013};
[[gnu::section(".iwram7")]] unsigned int iw7[24] = {0x17000000, 0x17000001, 0x17000002, 0x17000003, 0x17000004,
                                                    0x17000005, 0x17000006, 0x17000007, 0x17000008, 0x17000009,
                                                    0x1700000A, 0x1700000B, 0x1700000C, 0x1700000D, 0x1700000E,
                                                    0x1700000F, 0x17000010, 0x17000011, 0x17000012, 0x17000013,
                                                    0x17000014, 0x17000015, 0x17000016, 0x17000017};
[[gnu::section(".iwram8")]] unsigned int iw8[8] = {0x18000000, 0x18000001, 0x18000002, 0x18000003,
                                                   0x18000004, 0x18000005, 0x18000006, 0x18000007};
[[gnu::section(".iwram9")]] unsigned int iw9[4] = {0x19000000, 0x19000001, 0x19000002, 0x19000003};

// Expected sizes in bytes for each overlay index
static constexpr unsigned int ew_bytes[10] = {16, 32, 48, 64, 80, 96, 112, 128, 16, 32};
static constexpr unsigned int iw_bytes[10] = {32, 16, 64, 48, 128, 112, 80, 96, 32, 16};

// IWRAM code overlay functions (defined in overlay_code.cpp).
// Two functions sharing the same VMA region - proves overlays work for
// executable code, not just data.
extern "C++" int iwram_code_a();
extern "C++" int iwram_code_b();

// Helper: load an overlay section using CpuSet
static void load_overlay(const gba::overlay::section& ov) {
    if (ov.bytes) {
        gba::CpuSet(ov.rom, ov.wram, {.count = ov.bytes / 4, .set_32bit = true});
    }
}

// Helper: verify overlay data matches expected pattern at runtime
static void verify_overlay(volatile unsigned int* ptr, unsigned int base, unsigned int words) {
    for (unsigned int i = 0; i < words; ++i) {
        gba::test.eq(ptr[i], base + i);
    }
}

// Helper: cast an address to a uintptr_t for range checks
static uintptr_t addr(const void* p) {
    return reinterpret_cast<uintptr_t>(p);
}

// Helper: get EWRAM overlay by runtime index
static gba::overlay::section get_ewram(int n) {
    switch (n) {
        case 0: return gba::overlay::ewram<0>;
        case 1: return gba::overlay::ewram<1>;
        case 2: return gba::overlay::ewram<2>;
        case 3: return gba::overlay::ewram<3>;
        case 4: return gba::overlay::ewram<4>;
        case 5: return gba::overlay::ewram<5>;
        case 6: return gba::overlay::ewram<6>;
        case 7: return gba::overlay::ewram<7>;
        case 8: return gba::overlay::ewram<8>;
        case 9: return gba::overlay::ewram<9>;
        default: return {};
    }
}

// Helper: get IWRAM overlay by runtime index
static gba::overlay::section get_iwram(int n) {
    switch (n) {
        case 0: return gba::overlay::iwram<0>;
        case 1: return gba::overlay::iwram<1>;
        case 2: return gba::overlay::iwram<2>;
        case 3: return gba::overlay::iwram<3>;
        case 4: return gba::overlay::iwram<4>;
        case 5: return gba::overlay::iwram<5>;
        case 6: return gba::overlay::iwram<6>;
        case 7: return gba::overlay::iwram<7>;
        case 8: return gba::overlay::iwram<8>;
        case 9: return gba::overlay::iwram<9>;
        default: return {};
    }
}

int main() {
    // Test: Regular .ewram data was initialized correctly by CRT0.
    // If any EWRAM overlay LMA conflicted with .ewram LMA, this data
    // would be corrupt.
    {
        gba::test.eq(ewram_data[0], 0xE0E0E0E0u);
        gba::test.eq(ewram_data[1], 0xE1E1E1E1u);
        gba::test.eq(ewram_data[2], 0xE2E2E2E2u);
        gba::test.eq(ewram_data[3], 0xE3E3E3E3u);
        gba::test.eq(ewram_data[4], 0xE4E4E4E4u);
        gba::test.eq(ewram_data[5], 0xE5E5E5E5u);
        gba::test.eq(ewram_data[6], 0xE6E6E6E6u);
        gba::test.eq(ewram_data[7], 0xE7E7E7E7u);
    }

    // Test: Regular .iwram data was initialized correctly by CRT0.
    // If any IWRAM overlay LMA conflicted with .iwram LMA, this data
    // would be corrupt.
    {
        gba::test.eq(iwram_data[0], 0x10101010u);
        gba::test.eq(iwram_data[1], 0x11111111u);
        gba::test.eq(iwram_data[2], 0x12121212u);
        gba::test.eq(iwram_data[3], 0x13131313u);
        gba::test.eq(iwram_data[4], 0x14141414u);
        gba::test.eq(iwram_data[5], 0x15151515u);
        gba::test.eq(iwram_data[6], 0x16161616u);
        gba::test.eq(iwram_data[7], 0x17171717u);
    }

    // Test: All 10 EWRAM overlay descriptors report correct sizes and share WRAM
    {
        for (int n = 0; n < 10; ++n) {
            auto ov = get_ewram(n);
            gba::test.is_true(ov.wram == gba::overlay::ewram<0>.wram);
            gba::test.eq(ov.bytes, ew_bytes[n]);
        }
    }

    // Test: All 10 IWRAM overlay descriptors report correct sizes and share WRAM
    {
        for (int n = 0; n < 10; ++n) {
            auto ov = get_iwram(n);
            gba::test.is_true(ov.wram == gba::overlay::iwram<0>.wram);
            // IWRAM overlay sizes account for both data and code
            gba::test.is_true(ov.bytes >= iw_bytes[n]);
        }
    }

    // Test: EWRAM and IWRAM overlays target different WRAM regions
    { gba::test.is_true(gba::overlay::ewram<0>.wram != gba::overlay::iwram<0>.wram); }

    // Test: All EWRAM overlay ROM addresses are distinct
    {
        const void* addrs[10];
        for (int n = 0; n < 10; ++n) addrs[n] = get_ewram(n).rom;
        for (int i = 0; i < 10; ++i) {
            for (int j = i + 1; j < 10; ++j) {
                gba::test.is_true(addrs[i] != addrs[j]);
            }
        }
    }

    // Test: All IWRAM overlay ROM addresses are distinct
    {
        const void* addrs[10];
        for (int n = 0; n < 10; ++n) addrs[n] = get_iwram(n).rom;
        for (int i = 0; i < 10; ++i) {
            for (int j = i + 1; j < 10; ++j) {
                gba::test.is_true(addrs[i] != addrs[j]);
            }
        }
    }

    // Test: EWRAM overlay addresses are in the correct memory regions
    {
        for (int n = 0; n < 10; ++n) {
            auto ov = get_ewram(n);
            // ROM source must be in ROM address space (0x08000000-0x09FFFFFF)
            gba::test.is_true(addr(ov.rom) >= 0x08000000u);
            gba::test.is_true(addr(ov.rom) < 0x0A000000u);
            // WRAM destination must be in EWRAM (0x02000000-0x0203FFFF)
            gba::test.is_true(addr(ov.wram) >= 0x02000000u);
            gba::test.is_true(addr(ov.wram) < 0x02040000u);
        }
    }

    // Test: IWRAM overlay addresses are in the correct memory regions
    {
        for (int n = 0; n < 10; ++n) {
            auto ov = get_iwram(n);
            // ROM source must be in ROM address space
            gba::test.is_true(addr(ov.rom) >= 0x08000000u);
            gba::test.is_true(addr(ov.rom) < 0x0A000000u);
            // WRAM destination must be in IWRAM (0x03000000-0x03007FFF)
            gba::test.is_true(addr(ov.wram) >= 0x03000000u);
            gba::test.is_true(addr(ov.wram) < 0x03008000u);
        }
    }

    // Test: EWRAM overlay ROM addresses are sequentially ordered (no LMA overlap)
    {
        for (int i = 0; i < 9; ++i) {
            auto a = get_ewram(i);
            auto b = get_ewram(i + 1);
            gba::test.is_true(addr(a.rom) < addr(b.rom));
            gba::test.is_true(addr(b.rom) - addr(a.rom) >= a.bytes);
        }
    }

    // Test: IWRAM overlay ROM addresses are sequentially ordered (no LMA overlap)
    {
        for (int i = 0; i < 9; ++i) {
            gba::test.is_true(addr(get_iwram(i).rom) < addr(get_iwram(i + 1).rom));
        }
    }

    // Test: EWRAM overlay LMAs do not overlap with IWRAM overlay LMAs
    {
        auto ew_last = gba::overlay::ewram<9>;
        auto iw_first = gba::overlay::iwram<0>;
        gba::test.is_true(addr(ew_last.rom) + ew_last.bytes <= addr(iw_first.rom));
    }

    // Test: Load and verify each EWRAM overlay
    {
        auto* ptr = static_cast<volatile unsigned int*>(gba::overlay::ewram<0>.wram);
        const unsigned int bases[10] = {0xE0000000, 0xE1000000, 0xE2000000, 0xE3000000, 0xE4000000,
                                        0xE5000000, 0xE6000000, 0xE7000000, 0xE8000000, 0xE9000000};

        for (int n = 0; n < 10; ++n) {
            load_overlay(get_ewram(n));
            verify_overlay(ptr, bases[n], ew_bytes[n] / 4);
        }
    }

    // Test: Load and verify each IWRAM overlay
    {
        auto* ptr = static_cast<volatile unsigned int*>(gba::overlay::iwram<0>.wram);
        const unsigned int bases[10] = {0x10000000, 0x11000000, 0x12000000, 0x13000000, 0x14000000,
                                        0x15000000, 0x16000000, 0x17000000, 0x18000000, 0x19000000};

        for (int n = 0; n < 10; ++n) {
            load_overlay(get_iwram(n));
            verify_overlay(ptr, bases[n], iw_bytes[n] / 4);
        }
    }

    // Test: Switching between small and large overlays works correctly
    {
        auto* ptr = static_cast<volatile unsigned int*>(gba::overlay::ewram<0>.wram);

        load_overlay(gba::overlay::ewram<7>); // 128 bytes (largest)
        verify_overlay(ptr, 0xE7000000, 32);

        load_overlay(gba::overlay::ewram<0>); // 16 bytes (smallest)
        verify_overlay(ptr, 0xE0000000, 4);

        load_overlay(gba::overlay::ewram<7>);
        verify_overlay(ptr, 0xE7000000, 32);
    }

    // Test: Switching between small and large IWRAM overlays
    {
        auto* ptr = static_cast<volatile unsigned int*>(gba::overlay::iwram<0>.wram);

        load_overlay(gba::overlay::iwram<4>); // 128 bytes (largest)
        verify_overlay(ptr, 0x14000000, 32);

        load_overlay(gba::overlay::iwram<1>); // 16 bytes (smallest)
        verify_overlay(ptr, 0x11000000, 4);

        load_overlay(gba::overlay::iwram<4>);
        verify_overlay(ptr, 0x14000000, 32);
    }

    // Test: Cross-type independence - loading EWRAM does not affect IWRAM
    {
        load_overlay(gba::overlay::iwram<0>);
        auto iw_ptr = static_cast<volatile unsigned int*>(gba::overlay::iwram<0>.wram);
        gba::test.eq(iw_ptr[0], 0x10000000u);

        // Load all 10 EWRAM overlays - IWRAM region must not change
        for (int n = 0; n < 10; ++n) load_overlay(get_ewram(n));
        gba::test.eq(iw_ptr[0], 0x10000000u);
        gba::test.eq(iw_ptr[7], 0x10000007u);

        // Now load known data into EWRAM
        load_overlay(gba::overlay::ewram<3>);
        auto ew_ptr = static_cast<volatile unsigned int*>(gba::overlay::ewram<0>.wram);
        gba::test.eq(ew_ptr[0], 0xE3000000u);

        // Load all 10 IWRAM overlays - EWRAM region must not change
        for (int n = 0; n < 10; ++n) load_overlay(get_iwram(n));
        gba::test.eq(ew_ptr[0], 0xE3000000u);
        gba::test.eq(ew_ptr[15], 0xE300000Fu);
    }

    // Test: Rapid cycling stress test - 3 full forward/reverse passes
    {
        auto* ew_ptr = static_cast<volatile unsigned int*>(gba::overlay::ewram<0>.wram);
        auto* iw_ptr = static_cast<volatile unsigned int*>(gba::overlay::iwram<0>.wram);

        const unsigned int ew_bases[10] = {0xE0000000, 0xE1000000, 0xE2000000, 0xE3000000, 0xE4000000,
                                           0xE5000000, 0xE6000000, 0xE7000000, 0xE8000000, 0xE9000000};
        const unsigned int iw_bases[10] = {0x10000000, 0x11000000, 0x12000000, 0x13000000, 0x14000000,
                                           0x15000000, 0x16000000, 0x17000000, 0x18000000, 0x19000000};

        for (int cycle = 0; cycle < 3; ++cycle) {
            for (int n = 0; n < 10; ++n) {
                load_overlay(get_ewram(n));
                verify_overlay(ew_ptr, ew_bases[n], ew_bytes[n] / 4);
            }
            for (int n = 9; n >= 0; --n) {
                load_overlay(get_iwram(n));
                verify_overlay(iw_ptr, iw_bases[n], iw_bytes[n] / 4);
            }
        }

        // After all cycling, regular data sections must still be intact
        gba::test.eq(ewram_data[0], 0xE0E0E0E0u);
        gba::test.eq(ewram_data[7], 0xE7E7E7E7u);
        gba::test.eq(iwram_data[0], 0x10101010u);
        gba::test.eq(iwram_data[7], 0x17171717u);
    }

    // Test: Code execution from IWRAM overlays
    {
        using fn_type = int (*)();
        auto fn = reinterpret_cast<fn_type>(reinterpret_cast<uintptr_t>(&iwram_code_a));

        load_overlay(gba::overlay::iwram<0>);
        gba::test.eq(fn(), 42);

        load_overlay(gba::overlay::iwram<1>);
        auto fn_b = reinterpret_cast<fn_type>(reinterpret_cast<uintptr_t>(&iwram_code_b));
        gba::test.eq(fn_b(), 99);

        // Switch back and verify code_a works again
        load_overlay(gba::overlay::iwram<0>);
        gba::test.eq(fn(), 42);
    }

    // Test: Loading overlays does NOT corrupt regular .ewram data
    {
        for (int n = 0; n < 10; ++n) load_overlay(get_ewram(n));

        gba::test.eq(ewram_data[0], 0xE0E0E0E0u);
        gba::test.eq(ewram_data[7], 0xE7E7E7E7u);
    }

    // Test: Loading overlays does NOT corrupt regular .iwram data
    {
        for (int n = 0; n < 10; ++n) load_overlay(get_iwram(n));

        gba::test.eq(iwram_data[0], 0x10101010u);
        gba::test.eq(iwram_data[7], 0x17171717u);
    }
    return gba::test.finish();
}
