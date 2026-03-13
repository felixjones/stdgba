#include <gba/bios>
#include <gba/testing>
#include <gba/video>

int main() {
    // Palette memory
    for (auto entry : gba::mem_pal) {
        entry = -1;
    }
    gba::RegisterRamReset({.palette = true});
    gba::test.expect.zero(gba::mem_pal);

    // Video memory
    for (auto entry : gba::mem_vram) {
        entry = -1;
    }
    gba::RegisterRamReset({.vram = true});
    gba::test.expect.zero(gba::mem_vram);

    // Object attribute memory
    for (auto entry : gba::mem_oam) {
        entry = {-1, -1, -1};
    }
    gba::RegisterRamReset({.oam = true});
    gba::test.expect.zero(gba::mem_oam);
    return gba::test.finish();
}
