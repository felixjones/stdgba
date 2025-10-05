#include <gba/bios>
#include <gba/interrupt>
#include <gba/peripherals>

// ReSharper disable once CppUnusedIncludeDirective
#include <mgba_test.hpp>

int main() {
    gba::irq_handler = {};

    gba::reg_dispcnt = {};
    gba::reg_dispstat = { .enable_irq_vblank = true };
    gba::reg_ie = { .vblank = true };
    gba::reg_ime = true;

    gba::VBlankIntrWait();
}
