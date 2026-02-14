#include <mgba_test.hpp>

#include <gba/peripherals>

// Helper to calculate frequency rate from Hz
// Formula: rate = 2048 - (131072 / freq_hz)
constexpr unsigned short hz_to_rate(unsigned int hz) {
    return static_cast<unsigned short>(2048 - (131072 / hz));
}

// Simple busy-wait delay
void delay(int cycles) {
    for (volatile int i = 0; i < cycles; ++i) {
        asm volatile("");
    }
}

int main() {
    // Enable master sound
    gba::reg_soundcnt_x = { .master_enable = 1 };

    // Verify master sound is enabled
    gba::sound_control_x status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.master_enable, 1u);

    // Configure PSG master volume and routing
    gba::reg_soundcnt_l = {
        .volume_right = 7,
        .volume_left = 7,
        .enable_1_right = 1,
        .enable_2_right = 1,
        .enable_3_right = 1,
        .enable_4_right = 1,
        .enable_1_left = 1,
        .enable_2_left = 1,
        .enable_3_left = 1,
        .enable_4_left = 1
    };

    // Configure DirectSound mixer (full PSG volume)
    gba::reg_soundcnt_h = { .psg_volume = 2 };

    // Play Sound 1 (square wave with sweep) - A4 (440 Hz)
    gba::reg_sound1cnt_l = { .shift = 0, .direction = 0, .time = 0 }; // No sweep
    gba::reg_sound1cnt_h = { .duty = 2, .env_volume = 15 }; // 50% duty, max volume
    gba::reg_sound1cnt_x = { .rate = hz_to_rate(440), .trigger = 1 };

    // Verify sound 1 is playing
    status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.sound1_on, 1u);

    delay(10000);

    // Play Sound 2 (square wave) - E5 (659 Hz)
    gba::reg_sound2cnt_l = { .duty = 2, .env_volume = 15 };
    gba::reg_sound2cnt_h = { .rate = hz_to_rate(659), .trigger = 1 };

    // Verify sound 2 is playing
    status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.sound2_on, 1u);

    delay(10000);

    // Play Sound 3 (wave channel) - C5 (523 Hz)
    // First, set up wave RAM with a sawtooth pattern
    gba::reg_sound3cnt_l = { .enable = 0 }; // Disable to write wave RAM
    gba::reg_wave_ram[0] = 0x01234567;
    gba::reg_wave_ram[1] = 0x89ABCDEF;
    gba::reg_wave_ram[2] = 0xFEDCBA98;
    gba::reg_wave_ram[3] = 0x76543210;

    gba::reg_sound3cnt_l = { .bank_mode = 0, .bank_select = 0, .enable = 1 };
    gba::reg_sound3cnt_h = { .volume = 1 }; // 100% volume
    gba::reg_sound3cnt_x = { .rate = hz_to_rate(523), .trigger = 1 };

    // Verify sound 3 is playing
    status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.sound3_on, 1u);

    delay(10000);

    // Play Sound 4 (noise) - percussion-like
    gba::reg_sound4cnt_l = { .env_step = 3, .env_direction = 0, .env_volume = 15 };
    gba::reg_sound4cnt_h = { .div_ratio = 1, .width = 0, .shift = 2, .trigger = 1 };

    // Verify sound 4 is playing
    status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.sound4_on, 1u);

    delay(10000);

    // Stop all sounds by disabling master
    gba::reg_soundcnt_x = { .master_enable = 0 };

    // Verify all sounds stopped
    status = gba::reg_soundcnt_x;
    ASSERT_EQ(status.master_enable, 0u);

    return 0;
}
