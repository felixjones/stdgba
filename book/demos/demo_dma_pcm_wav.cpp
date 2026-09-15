/// @file demo_dma_pcm_wav.cpp
/// @brief Play an embedded 8-bit PCM WAV once through Direct Sound A.

#include <gba/bios>
#include <gba/dma>
#include <gba/embed>
#include <gba/interrupt>
#include <gba/keyinput>
#include <gba/peripherals>

#include <array>
#include <cstddef>

namespace {

    static constexpr auto sound = gba::embed::wav<>([] {
        return std::to_array<unsigned char>({
#embed "Piano.wav"
        });
    });

    volatile std::size_t frames_remaining{};

    void stop_sound() {
        gba::reg_tmcnt_h[0] = {};
        gba::reg_dmacnt_h[1] = {};
        gba::reg_soundcnt_h = {
            .dma_a_volume = true,
            .dma_a_right = true,
            .dma_a_left = true,
            .dma_a_reset = true,
        };
        frames_remaining = 0;
    }

    void play_sound() {
        stop_sound();
        gba::reg_dma[1] = gba::dma::to_fifo_a(sound.samples.data());
        frames_remaining = sound.frame_count;
        gba::reg_tmcnt[0] = sound.timer;
    }

} // namespace

int main() {
    gba::irq_handler = {[](gba::irq flags) {
        if (flags.vblank && frames_remaining != 0) {
            frames_remaining = frames_remaining - 1;
            if (frames_remaining == 0) {
                stop_sound();
            }
        }
    }};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};

    gba::reg_soundcnt_x = {.master_enable = true};
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
