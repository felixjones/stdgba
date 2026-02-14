#include <gba/bios>
#include <gba/video>

#include <mgba_test.hpp>

int main() {
    // Palette memory
    for (auto entry : gba::mem_pal) {
        entry = -1;
    }
    gba::RegisterRamReset({.palette = true});
    EXPECT_ZERO(gba::mem_pal);

    // Video memory
    for (auto entry : gba::mem_vram) {
        entry = -1;
    }
    gba::RegisterRamReset({.vram = true});
    EXPECT_ZERO(gba::mem_vram);

    // Object attribute memory
    for (auto entry : gba::mem_oam) {
        entry = {-1, -1, -1};
    }
    gba::RegisterRamReset({.oam = true});
    EXPECT_ZERO(gba::mem_oam);
}
