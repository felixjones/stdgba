/// @file tests/overlay/overlay_code.cpp
/// @brief IWRAM code overlay functions for test_overlay.

// iwram_code_a lives in .iwram0 overlay section
[[gnu::section(".iwram0"), gnu::noinline, gnu::long_call, gnu::target("arm")]]
int iwram_code_a() {
    return 42;
}

// iwram_code_b lives in .iwram1 overlay section
[[gnu::section(".iwram1"), gnu::noinline, gnu::long_call, gnu::target("arm")]]
int iwram_code_b() {
    return 99;
}
