/// @file demo_dma_pcm_wav.cpp
/// @brief Play an embedded 8-bit PCM WAV once through Direct Sound A.

#include <gba/bios>
#include <gba/dma>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/peripherals>

#include <array>

namespace {

    static constexpr auto sound = gba::embed::wav<16384>([] {
        return std::to_array<unsigned char>({
#embed "Piano.wav"
        });
    });

    static_assert(sound.sample_count <= 65535);
    constexpr unsigned int timer_period = (16777216 + sound.sample_rate / 2) / sound.sample_rate;
    static_assert(timer_period > 0 && timer_period <= 65536);
    constexpr unsigned short sample_timer_reload = 65536 - timer_period;
    constexpr unsigned short stop_timer_reload = 65536 - sound.sample_count;

    void stop_sound() {
        gba::reg_tmcnt_h[0] = {};
        gba::reg_tmcnt_h[1] = {};
        gba::reg_dmacnt_h[1] = {};
        gba::reg_soundcnt_h = {
            .dma_a_volume = true,
            .dma_a_right = true,
            .dma_a_left = true,
            .dma_a_reset = true,
        };
    }

    void play_sound() {
        stop_sound();
        gba::reg_dma[1] = gba::dma::to_fifo_a(sound.samples.data());
        gba::reg_tmcnt_l_reload[1] = stop_timer_reload;
        gba::reg_tmcnt_h[1] = {.cascade = true, .overflow_irq = true, .enabled = true};
        gba::reg_tmcnt_l_reload[0] = sample_timer_reload;
        gba::reg_tmcnt_h[0] = {.cycles = gba::cycles_1, .enabled = true};
    }

} // namespace

int main() {
    gba::irq_handler = {[](gba::irq flags) {
        if (flags.timer1) {
            stop_sound();
        }
    }};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true, .timer1 = true};

    gba::reg_soundcnt_x = {.master_enable = true};
    play_sound();
    gba::reg_ime = true;

    gba::keypad keys;
    while (true) {
        gba::VBlankIntrWait();
        keys = gba::reg_keyinput;
        if (keys.pressed(gba::key_a)) {
            play_sound();
        }
    }
}
