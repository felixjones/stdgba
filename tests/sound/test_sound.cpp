#include <gba/bios>
#include <gba/interrupt>
#include <gba/peripherals>
#include <gba/testing>

// Helper to calculate frequency rate from Hz
// Formula: rate = 2048 - (131072 / freq_hz)
constexpr unsigned short hz_to_rate(unsigned int hz) {
    return static_cast<unsigned short>(2048 - (131072 / hz));
}

// Wait for N frames using VBlank interrupt
void wait_frames(int frames) {
    for (int i = 0; i < frames; ++i) {
        gba::VBlankIntrWait();
    }
}

int main() {
    // Setup VBlank interrupt
    gba::irq_handler = {};
    gba::reg_dispstat = {.enable_irq_vblank = true};
    gba::reg_ie = {.vblank = true};
    gba::reg_ime = true;

    // Enable master sound
    gba::reg_soundcnt_x = {.master_enable = true};

    // Verify master sound is enabled
    gba::sound_control_x status = gba::reg_soundcnt_x;
    gba::test.is_true(status.master_enable);

    // Configure PSG master volume and routing
    gba::reg_soundcnt_l = {.volume_right = 7,
                           .volume_left = 7,
                           .enable_1_right = true,
                           .enable_2_right = true,
                           .enable_3_right = true,
                           .enable_4_right = true,
                           .enable_1_left = true,
                           .enable_2_left = true,
                           .enable_3_left = true,
                           .enable_4_left = true};

    // Configure DirectSound mixer (full PSG volume)
    gba::reg_soundcnt_h = {.psg_volume = 2};

    // Play Sound 1 (square wave with sweep) - A4 (440 Hz)
    gba::reg_sound1cnt_l = {.shift = 0, .direction = 0, .time = 0}; // No sweep
    gba::reg_sound1cnt_h = {.duty = 2, .env_volume = 15};           // 50% duty, max volume
    gba::reg_sound1cnt_x = {.rate = hz_to_rate(440), .trigger = true};

    // Verify sound 1 is playing
    status = gba::reg_soundcnt_x;
    gba::test.is_true(status.sound1_on);

    wait_frames(60); // ~1 second

    // Play Sound 2 (square wave) - E5 (659 Hz)
    gba::reg_sound2cnt_l = {.duty = 2, .env_volume = 15};
    gba::reg_sound2cnt_h = {.rate = hz_to_rate(659), .trigger = true};

    // Verify sound 2 is playing
    status = gba::reg_soundcnt_x;
    gba::test.is_true(status.sound2_on);

    wait_frames(60);

    // Play Sound 3 (wave channel) - C5 (523 Hz)
    // Select bank 1 so writes go to bank 0, disable channel
    gba::reg_sound3cnt_l = {.bank_select = true, .enable = false};

    // Write wave RAM (goes to bank 0 because bank 1 is selected)
    gba::reg_wave_ram[0] = 0x01234567;
    gba::reg_wave_ram[1] = 0x89ABCDEF;
    gba::reg_wave_ram[2] = 0xFEDCBA98;
    gba::reg_wave_ram[3] = 0x76543210;

    // Set volume 100%
    gba::reg_sound3cnt_h = {.volume = 1};
    // Select bank 0 for playback and enable
    gba::reg_sound3cnt_l = {.bank_select = false, .enable = true};
    // Trigger
    gba::reg_sound3cnt_x = {.rate = hz_to_rate(523), .trigger = true};

    // Verify sound 3 is playing
    status = gba::reg_soundcnt_x;
    gba::test.expect.is_true(status.sound3_on);

    wait_frames(60);

    // Play Sound 4 (noise) - percussion-like
    gba::reg_sound4cnt_l = {.env_step = 3, .env_direction = 0, .env_volume = 15};
    gba::reg_sound4cnt_h = {.div_ratio = 1, .width = false, .shift = 2, .trigger = true};

    // Verify sound 4 is playing
    status = gba::reg_soundcnt_x;
    gba::test.is_true(status.sound4_on);

    wait_frames(60);

    // Stop all sounds by disabling master
    gba::reg_soundcnt_x = {.master_enable = false};

    // Verify all sounds stopped
    status = gba::reg_soundcnt_x;
    gba::test.is_false(status.master_enable);

    return gba::test.finish();
}
