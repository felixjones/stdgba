/// @file demo_dma_pcm_sine.cpp
/// @brief Stream a looping signed 8-bit sine wave through Direct Sound A.

#include <gba/bios>
#include <gba/dma>
#include <gba/interrupt>
#include <gba/peripherals>

#include <array>
#include <cstdint>

namespace {

    constexpr unsigned short samples_per_frame = 96;
    constexpr unsigned short timer_period = 280896 / samples_per_frame;
    constexpr unsigned short timer_reload = 65536 - timer_period;

    constexpr std::array<std::int8_t, 32> sine_period{
        0, 25,  49,  71,  90,  106,  117,  125,  127,  125,  117,  106,  90,  71,  49,  25,
        0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25,
    };

    constexpr auto make_sample_buffer() {
        std::array<std::int8_t, samples_per_frame + sine_period.size()> result{};
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] = sine_period[index % sine_period.size()];
        }
        return result;
    }

    alignas(4) constexpr auto samples = make_sample_buffer();

    void rewind_audio_dma() {
        gba::reg_dmacnt_h[1] = {};
        gba::reg_dma[1] = gba::dma::to_fifo_a(samples.data());
    }

} // namespace

int main() {
    gba::irq_handler = {[](gba::irq flags) {
        if (flags.vblank) {
            rewind_audio_dma();
        }
    }};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};

    gba::reg_soundcnt_x = {.master_enable = true};
    gba::reg_soundcnt_h = {
        .dma_a_volume = true,
        .dma_a_right = true,
        .dma_a_left = true,
        .dma_a_reset = true,
    };

    rewind_audio_dma();
    gba::reg_tmcnt_l_reload[0] = timer_reload;
    gba::reg_tmcnt_h[0] = {.cycles = gba::cycles_1, .enabled = true};
    gba::reg_ime = true;

    while (true) {
        gba::VBlankIntrWait();
    }
}
