# Sound

## Channel 1 (Square with Sweep)

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000060` | `reg_sound1cnt_l` | RW | `sound1_sweep` | `REG_SND1SWEEP` |
| `0x4000062` | `reg_sound1cnt_h` | RW | `sound_duty_envelope` | `REG_SND1CNT` |
| `0x4000064` | `reg_sound1cnt_x` | RW | `sound_frequency` | `REG_SND1FREQ` |

### `sound1_sweep`

```cpp
struct sound1_sweep {
    unsigned short shift : 3;     // Sweep shift (0-7)
    unsigned short direction : 1; // 0 = increase, 1 = decrease
    unsigned short time : 3;      // Sweep time (units of 7.8ms)
};
```

### `sound_duty_envelope`

Shared by channels 1 and 2.

```cpp
struct sound_duty_envelope {
    unsigned short length : 6;        // Sound length (0-63)
    unsigned short duty : 2;          // Duty cycle (0=12.5%, 1=25%, 2=50%, 3=75%)
    unsigned short env_step : 3;      // Envelope step time
    unsigned short env_direction : 1; // 0 = decrease, 1 = increase
    unsigned short env_volume : 4;    // Initial volume (0-15)
};
```

### `sound_frequency`

Shared by channels 1, 2, and 3.

```cpp
struct sound_frequency {
    unsigned short rate : 11; // Frequency rate (131072/(2048-rate) Hz)
    unsigned short : 3;
    bool timed : 1;           // false = continuous, true = use length
    bool trigger : 1;         // Write true to start/restart
};
```

```cpp
gba::reg_sound1cnt_l = { .shift = 2, .time = 3 };
gba::reg_sound1cnt_h = { .duty = 2, .env_volume = 15 };
gba::reg_sound1cnt_x = { .rate = 1750, .trigger = true }; // ~440 Hz
```

## Channel 2 (Square)

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000068` | `reg_sound2cnt_l` | RW | `sound_duty_envelope` | `REG_SND2CNT` |
| `0x400006C` | `reg_sound2cnt_h` | RW | `sound_frequency` | `REG_SND2FREQ` |

Uses the same `sound_duty_envelope` and `sound_frequency` types as channel 1.

## Channel 3 (Wave)

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000070` | `reg_sound3cnt_l` | RW | `sound3_control` | `REG_SND3SEL` |
| `0x4000072` | `reg_sound3cnt_h` | RW | `sound3_length_volume` | `REG_SND3CNT` |
| `0x4000074` | `reg_sound3cnt_x` | RW | `sound_frequency` | `REG_SND3FREQ` |
| `0x4000090` | `reg_wave_ram[4]` | RW | `unsigned int[4]` | `REG_WAVE_RAM` |

### `sound3_control`

```cpp
struct sound3_control {
    unsigned short : 5;
    bool bank_mode : 1;   // false = 2x32 samples, true = 1x64
    bool bank_select : 1; // Select bank (0 or 1) for 2x32
    bool enable : 1;
};
```

### `sound3_length_volume`

```cpp
struct sound3_length_volume {
    unsigned short length : 8; // Sound length (0-255)
    unsigned short : 5;
    unsigned short volume : 2; // 0=mute, 1=100%, 2=50%, 3=25%
    bool force_75 : 1;         // Force 75% volume
};
```

## Channel 4 (Noise)

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000078` | `reg_sound4cnt_l` | RW | `sound4_envelope` | `REG_SND4CNT` |
| `0x400007C` | `reg_sound4cnt_h` | RW | `sound4_frequency` | `REG_SND4FREQ` |

### `sound4_envelope`

```cpp
struct sound4_envelope {
    unsigned short length : 6;
    unsigned short : 2;
    unsigned short env_step : 3;
    unsigned short env_direction : 1; // 0 = decrease, 1 = increase
    unsigned short env_volume : 4;    // Initial volume (0-15)
};
```

### `sound4_frequency`

```cpp
struct sound4_frequency {
    unsigned short div_ratio : 3; // Frequency divider ratio
    bool width : 1;               // Counter width (false=15-bit, true=7-bit)
    unsigned short shift : 4;     // Shift clock frequency
    unsigned short : 6;
    bool timed : 1;
    bool trigger : 1;
};
```

## Master Control

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000080` | `reg_soundcnt_l` | RW | `sound_control_l` | `REG_SNDDMGCNT` |
| `0x4000082` | `reg_soundcnt_h` | RW | `sound_control_h` | `REG_SNDDSCNT` |
| `0x4000084` | `reg_soundcnt_x` | RW | `sound_control_x` | `REG_SNDSTAT` |
| `0x4000088` | `reg_soundbias` | RW | `sound_bias` | `REG_SNDBIAS` |
| `0x40000A0` | `reg_fifo_a` | W | `volatile unsigned int` | `REG_FIFO_A` |
| `0x40000A4` | `reg_fifo_b` | W | `volatile unsigned int` | `REG_FIFO_B` |

### `sound_control_l` - PSG volume and routing

```cpp
struct sound_control_l {
    unsigned short volume_right : 3; // Right master volume (0-7)
    unsigned short : 1;
    unsigned short volume_left : 3;  // Left master volume (0-7)
    unsigned short : 1;
    bool enable_1_right : 1;
    bool enable_2_right : 1;
    bool enable_3_right : 1;
    bool enable_4_right : 1;
    bool enable_1_left : 1;
    bool enable_2_left : 1;
    bool enable_3_left : 1;
    bool enable_4_left : 1;
};
```

### `sound_control_h` - DirectSound/mixer

```cpp
struct sound_control_h {
    unsigned short psg_volume : 2;  // PSG volume (0=25%, 1=50%, 2=100%)
    bool dma_a_volume : 1;         // DMA A volume (0=50%, 1=100%)
    bool dma_b_volume : 1;         // DMA B volume (0=50%, 1=100%)
    unsigned short : 4;
    bool dma_a_right : 1;
    bool dma_a_left : 1;
    bool dma_a_timer : 1;          // 0=timer0, 1=timer1
    bool dma_a_reset : 1;          // Reset FIFO
    bool dma_b_right : 1;
    bool dma_b_left : 1;
    bool dma_b_timer : 1;
    bool dma_b_reset : 1;
};
```

### `sound_control_x` - Master enable

```cpp
struct sound_control_x {
    bool sound1_on : 1; // (read-only)
    bool sound2_on : 1; // (read-only)
    bool sound3_on : 1; // (read-only)
    bool sound4_on : 1; // (read-only)
    unsigned short : 3;
    bool master_enable : 1;
};
```

```cpp
gba::reg_soundcnt_x = { .master_enable = true };
gba::reg_soundcnt_l = {
    .volume_right = 7, .volume_left = 7,
    .enable_1_right = true, .enable_1_left = true
};
```
