/// @file bits/music/types.hpp
/// @brief Core types for PSG music composition.
///
/// Defines note names, frequency lookup tables, channel types, instrument
/// configurations, rational timing arithmetic, and built-in waveforms.
/// All types are constexpr/consteval - no runtime overhead.
#pragma once

#include <array>
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace gba::music {

    // -- Timing constants ------------------------------------------------

    /// @brief GBA CPU clock frequency: 2^24 = 16,777,216 Hz.
    inline constexpr std::int64_t cpu_clock = 1 << 24;

    /// @brief CPU cycles per frame: 228 scanlines x 1232 cycles = 280,896.
    inline constexpr std::int64_t frame_clocks = 280896;

    // -- Rational arithmetic ---------------------------------------------

    /// @brief Compile-time rational number with GCD reduction.
    ///
    /// Used for exact frame-level timing. BPM -> frames-per-step is almost
    /// never an integer, so all timing uses rational accumulation.
    struct rational {
        std::int64_t num{};
        std::int64_t den{1};

        consteval rational() = default;
        consteval rational(std::int64_t n, std::int64_t d) : num{n}, den{d} { reduce(); }
        consteval explicit rational(std::int64_t n) : num{n}, den{1} {}

        consteval void reduce() {
            if (den < 0) {
                num = -num;
                den = -den;
            }
            auto g = gcd(num < 0 ? -num : num, den);
            if (g > 0) {
                num /= g;
                den /= g;
            }
        }

        consteval rational operator+(rational rhs) const { return {num * rhs.den + rhs.num * den, den * rhs.den}; }

        consteval rational operator-(rational rhs) const { return {num * rhs.den - rhs.num * den, den * rhs.den}; }

        consteval rational operator*(rational rhs) const { return {num * rhs.num, den * rhs.den}; }

        consteval rational operator/(rational rhs) const { return {num * rhs.den, den * rhs.num}; }

        consteval bool operator==(rational rhs) const { return num * rhs.den == rhs.num * den; }

        consteval bool operator<(rational rhs) const { return num * rhs.den < rhs.num * den; }

        consteval bool operator<=(rational rhs) const { return num * rhs.den <= rhs.num * den; }

        consteval bool operator>(rational rhs) const { return num * rhs.den > rhs.num * den; }

        consteval bool operator>=(rational rhs) const { return num * rhs.den >= rhs.num * den; }

    private:
        static consteval std::int64_t gcd(std::int64_t a, std::int64_t b) {
            while (b) {
                auto t = b;
                b = a % b;
                a = t;
            }
            return a;
        }
    };

    // -- Tempo (Strudel CPS model) -------------------------------------

    /// @brief Compile-time tempo value based on Strudel's cycles-per-second.
    ///
    /// Strudel thinks in cycles, not beats. A cycle is one full iteration
    /// of the pattern. The default is 0.5 cps (one cycle every 2 seconds),
    /// which at 4 notes per cycle equals 120 BPM.
    ///
    /// @see https://strudel.cc/understand/cycles/
    struct tempo {
        rational cps; ///< Cycles per second as a rational.

        /// @brief Compute frames per cycle at this tempo.
        ///
        /// frame_rate = cpu_clock / frame_clocks Hz.
        /// frames_per_cycle = frame_rate / cps = cpu_clock / (frame_clocks * cps).
        consteval rational frames_per_cycle() const { return rational{cpu_clock * cps.den, frame_clocks * cps.num}; }

        consteval bool operator==(const tempo&) const = default;
    };

    /// @brief Default tempo: 0.5 cps (= 120 BPM in 4/4 time).
    inline constexpr tempo default_tempo = {
        rational{1, 2}
    };

    /// @brief Compute frames per cycle for a given BPM and beats per measure.
    ///
    /// Convenience: converts BPM to CPS via cps = bpm / (60 * beats_per_measure).
    consteval rational frames_per_cycle(int bpm, int beats_per_measure = 4) {
        tempo t{
            rational{bpm, 60 * beats_per_measure}
        };
        return t.frames_per_cycle();
    }

    namespace literals {

        /// @brief Cycles per second: `1_cps` = 1 cycle/sec.
        consteval tempo operator""_cps(unsigned long long v) {
            if (v == 0) throw "cps: cycles per second must be > 0";
            return tempo{
                rational{static_cast<std::int64_t>(v), 1}
            };
        }

        /// @brief Cycles per second (fractional): `0.5_cps` = default Strudel tempo.
        consteval tempo operator""_cps(long double v) {
            if (v <= 0.0L) throw "cps: cycles per second must be > 0";
            // Convert to rational with millionths precision then reduce via GCD.
            auto num = static_cast<std::int64_t>(v * 1000000.0L);
            return tempo{
                rational{num, 1000000}
            };
        }

        /// @brief Cycles per minute: `30_cpm` = 0.5 cps.
        consteval tempo operator""_cpm(unsigned long long v) {
            if (v == 0) throw "cpm: cycles per minute must be > 0";
            return tempo{
                rational{static_cast<std::int64_t>(v), 60}
            };
        }

        /// @brief Beats per minute (assumes 4 beats/cycle): `120_bpm` = 0.5 cps.
        consteval tempo operator""_bpm(unsigned long long v) {
            if (v == 0) throw "bpm: beats per minute must be > 0";
            return tempo{
                rational{static_cast<std::int64_t>(v), 240}
            };
        }

    } // namespace literals

    // -- Channel ---------------------------------------------------------

    /// @brief PSG sound channel identifier.
    enum class channel : std::uint8_t {
        sq1 = 0,   ///< Square wave 1 (with sweep).
        sq2 = 1,   ///< Square wave 2.
        wav = 2,   ///< Wave channel.
        noise = 3, ///< Noise channel.
    };

    // -- Notes -----------------------------------------------------------

    /// @brief Musical note identifier.
    ///
    /// Chromatic notes C1-B8 plus
    /// named noise-channel drum presets. `rest` represents silence.
    /// `hold` (`_` in mini-notation) sustains the previous note without
    /// retriggering - no event is emitted, leaving the PSG channel
    /// playing its current frequency and envelope unchanged.
    enum class note : std::uint8_t {
        rest = 0,

        // Chromatic notes: octave 1-8
        c1,
        cs1,
        d1,
        ds1,
        e1,
        f1,
        fs1,
        g1,
        gs1,
        a1,
        as1,
        b1,
        c2,
        cs2,
        d2,
        ds2,
        e2,
        f2,
        fs2,
        g2,
        gs2,
        a2,
        as2,
        b2,
        c3,
        cs3,
        d3,
        ds3,
        e3,
        f3,
        fs3,
        g3,
        gs3,
        a3,
        as3,
        b3,
        c4,
        cs4,
        d4,
        ds4,
        e4,
        f4,
        fs4,
        g4,
        gs4,
        a4,
        as4,
        b4,
        c5,
        cs5,
        d5,
        ds5,
        e5,
        f5,
        fs5,
        g5,
        gs5,
        a5,
        as5,
        b5,
        c6,
        cs6,
        d6,
        ds6,
        e6,
        f6,
        fs6,
        g6,
        gs6,
        a6,
        as6,
        b6,
        c7,
        cs7,
        d7,
        ds7,
        e7,
        f7,
        fs7,
        g7,
        gs7,
        a7,
        as7,
        b7,
        c8,
        cs8,
        d8,
        ds8,
        e8,
        f8,
        fs8,
        g8,
        gs8,
        a8,
        as8,
        b8,

        // Noise channel drum presets
        bd,  ///< Bass drum (kick).
        sd,  ///< Snare drum.
        hh,  ///< Hi-hat (closed).
        oh,  ///< Open hi-hat.
        cp,  ///< Clap.
        rs,  ///< Rimshot (short).
        rim, ///< Rimshot (Strudel name - woody click).
        lt,  ///< Low tom.
        mt,  ///< Mid tom.
        ht,  ///< High tom.
        cb,  ///< Cowbell.
        cr,  ///< Crash cymbal.
        rd,  ///< Ride cymbal.
        hc,  ///< High conga.
        mc,  ///< Mid conga.
        lc,  ///< Low conga.
        cl,  ///< Claves.
        sh,  ///< Shaker.
        ma,  ///< Maracas.
        ag,  ///< Agogo bell.

        // Hold/tie - no event emitted; PSG channel sustains previous note.
        hold, ///< `_` - sustain previous note (no retrigger).
    };

    /// @brief First chromatic note.
    inline constexpr auto first_chromatic = note::c1;
    /// @brief Last chromatic note.
    inline constexpr auto last_chromatic = note::b8;
    /// @brief First drum preset.
    inline constexpr auto first_drum = note::bd;
    /// @brief Last drum preset.
    inline constexpr auto last_drum = note::ag;

    consteval bool is_chromatic(note n) {
        return n >= first_chromatic && n <= last_chromatic;
    }

    consteval bool is_drum(note n) {
        return n >= first_drum && n <= last_drum;
    }

    /// @brief Returns true if `n` is the hold/tie sentinel.
    consteval bool is_hold(note n) {
        return n == note::hold;
    }

    // -- DMG frequency table ---------------------------------------------

    /// @brief PSG frequency rate values for chromatic notes C1-B8.
    ///
    /// rate = 2048 - (2^17 / freq_hz), where freq_hz is 12-TET A4=440.
    /// Values from the DMG frequency table (furball project).
    /// Index 0 = C1, index 1 = C#1, ..., index 95 = B8.
    ///
    /// Octave 1 (C1-B1) is below the GBA PSG hardware floor (~64 Hz at rate=0).
    /// These entries are kept in the table so enum indexing stays contiguous, but
    /// `note_to_rate()` rejects them with a consteval hard error so the user gets
    /// a clear compile diagnostic instead of silent pitch saturation.
    inline constexpr std::array<std::uint16_t, 96> note_rate_table = {
        // Octave 1: C1-B1 (below hardware floor - note_to_rate() rejects these)
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        // Octave 2: C2-B2
        44,
        156,
        262,
        363,
        457,
        547,
        631,
        710,
        786,
        854,
        923,
        986,
        // Octave 3: C3-B3
        1046,
        1102,
        1155,
        1205,
        1253,
        1297,
        1339,
        1379,
        1417,
        1452,
        1486,
        1517,
        // Octave 4: C4-B4
        1546,
        1575,
        1602,
        1627,
        1650,
        1673,
        1694,
        1714,
        1732,
        1750,
        1767,
        1783,
        // Octave 5: C5-B5
        1798,
        1812,
        1825,
        1837,
        1849,
        1860,
        1871,
        1881,
        1890,
        1899,
        1907,
        1915,
        // Octave 6: C6-B6
        1923,
        1930,
        1936,
        1943,
        1949,
        1954,
        1959,
        1964,
        1969,
        1974,
        1978,
        1982,
        // Octave 7: C7-B7
        1985,
        1988,
        1992,
        1995,
        1998,
        2001,
        2004,
        2006,
        2009,
        2011,
        2013,
        2015,
        // Octave 8: C8-B8
        2017,
        2018,
        2020,
        2022,
        2023,
        2025,
        2026,
        2027,
        2028,
        2029,
        2031,
        2032,
    };

    /// @brief Get PSG frequency rate for a chromatic note.
    ///
    /// Rejects octave-1 notes (C1-B1) at compile time because the GBA square-wave
    /// hardware cannot represent frequencies below ~64 Hz. Use C2 or higher.
    consteval std::uint16_t note_to_rate(note n) {
        if (!is_chromatic(n)) throw "note_to_rate: not a chromatic note";
        if (n < note::c2)
            throw "note_to_rate: octave-1 notes (C1-B1) are below the GBA PSG hardware floor (~64 Hz) - use C2 or "
                  "higher";
        return note_rate_table[static_cast<int>(n) - static_cast<int>(first_chromatic)];
    }

    /// @brief Get wave channel rate for a chromatic note in 1x64 mode.
    ///
    /// The wave channel in 1x64 mode (64-sample waveform) has the frequency
    /// formula: f = 2097152 / (64 x (2048 - rate)) = 32768 / (2048 - rate).
    ///
    /// This is derived from the sq1/sq2 rate via:
    ///   wav_rate = 2048 - (2048 - sq_rate) / 4
    ///
    /// The sq channels use f = 131072 / (2048 - rate), so the wave channel
    /// at 64 samples needs a different rate value for the same pitch.
    consteval std::uint16_t note_to_wav_rate(note n) {
        auto sqRate = note_to_rate(n); // inherits all validation
        return static_cast<std::uint16_t>(2048 - (2048 - sqRate) / 4);
    }

    // -- Noise drum presets ----------------------------------------------

    /// @brief Noise channel frequency parameters for a drum preset.
    struct drum_preset {
        std::uint8_t div_ratio;     ///< Frequency divider (0-7).
        bool width;                 ///< Counter width (false=15-bit, true=7-bit).
        std::uint8_t shift;         ///< Shift clock frequency (0-15).
        std::uint8_t env_volume;    ///< Initial volume (0-15).
        std::uint8_t env_step;      ///< Envelope step time (0-7).
        std::uint8_t env_direction; ///< 0 = decrease, 1 = increase.
    };

    /// @brief Drum preset lookup table. Index by (note - first_drum).
    ///
    /// Noise frequency formula (Hz):
    ///   r > 0: f = 524288 / (r * 2^(s+1))
    ///   r = 0: f = 524288 / 2^s
    /// where r = div_ratio, s = shift.
    /// Higher shift = lower frequency.  7-bit width = metallic/tonal.
    ///
    /// Envelope timing: each step = env_step * (1/64) seconds.
    /// Total decay = env_volume * env_step * (1/64) s.
    /// env_step=0 disables envelope (constant volume).
    inline constexpr std::array<drum_preset, 20> drum_preset_table = {
        {
         // bd (bass drum): deep low thump, 15-bit full noise, punchy decay
            //   f = 524288 / (7 * 512) = 146 Hz. Decay = 15 * 15.6ms = 234ms.
            {7, false, 8, 15, 1, 0},
         // sd (snare): broadband crack, 15-bit full noise for natural body
            //   f = 524288 / (1 * 32) = 16384 Hz. Decay = 13 * 15.6ms = 203ms.
            {1, false, 4, 13, 1, 0},
         // hh (hi-hat closed): bright shimmer, 7-bit metallic, ultra-short tick
            //   f = 524288 / 2 = 262144 Hz. Decay = 8 * 15.6ms = 125ms.
            {0, true, 1, 8, 1, 0},
         // oh (open hi-hat): bright shimmer, 7-bit, longer ring
            //   f = 262144 Hz (matches hh). Decay = 10 * 46.9ms = 469ms.
            {0, true, 1, 10, 3, 0},
         // cp (clap): broadband snap, 15-bit full noise for natural body
            //   f = 524288 / (2 * 16) = 16384 Hz. Decay = 13 * 31.3ms = 406ms.
            {2, false, 3, 13, 2, 0},
         // rs (rimshot): sharp high tick, 7-bit, very short
            //   f = 524288 / 4 = 131072 Hz. Decay = 11 * 15.6ms = 172ms.
            {0, true, 2, 11, 1, 0},
         // rim (rimshot / Strudel name): woody click, 15-bit fuller body, very short
            //   f = 524288 / (1 * 8) = 65536 Hz. Decay = 12 * 15.6ms = 187ms.
            {1, false, 2, 12, 1, 0},
         // lt (low tom): deep, 15-bit, medium decay
            //   f = 524288 / (6 * 128) = 683 Hz. Decay = 14 * 31.3ms = 438ms.
            {6, false, 6, 14, 2, 0},
         // mt (mid tom): mid, 15-bit, medium decay
            //   f = 524288 / (4 * 32) = 4096 Hz. Decay = 14 * 31.3ms = 438ms.
            {4, false, 4, 14, 2, 0},
         // ht (high tom): brighter, 15-bit, medium decay
            //   f = 524288 / (2 * 8) = 32768 Hz. Decay = 14 * 31.3ms = 438ms.
            {2, false, 2, 14, 2, 0},
         // cb (cowbell): tonal ring, 7-bit, quick decay
            //   f = 524288 / 8 = 65536 Hz. Decay = 12 * 15.6ms = 187ms.
            {0, true, 3, 12, 1, 0},
         // cr (crash cymbal): wide noise, 15-bit, long ring
            //   f = 524288 / (2 * 8) = 32768 Hz. Decay = 13 * 62.5ms = 813ms.
            {2, false, 2, 13, 4, 0},
         // rd (ride cymbal): bright, 15-bit, medium ring
            //   f = 524288 / (1 * 4) = 131072 Hz. Decay = 11 * 46.9ms = 516ms.
            {1, false, 1, 11, 3, 0},
         // hc (high conga): bright, tonal pop, 7-bit, short decay
            //   f = 524288 / 8 = 65536 Hz. Decay = 12 * 15.6ms = 187ms.
            {0, true, 3, 12, 1, 0},
         // mc (mid conga): mid tonal pop, 7-bit, short-medium decay
            //   f = 524288 / (1 * 32) = 16384 Hz. Decay = 13 * 31.3ms = 406ms.
            {1, true, 4, 13, 2, 0},
         // lc (low conga): deep body, 15-bit, medium decay
            //   f = 524288 / (3 * 64) = 2731 Hz. Decay = 14 * 31.3ms = 438ms.
            {3, false, 5, 14, 2, 0},
         // cl (claves): ultra-sharp bright click, 7-bit, very short
            //   f = 524288 / 2 = 262144 Hz. Decay = 13 * 15.6ms = 203ms.
            {0, true, 1, 13, 1, 0},
         // sh (shaker): granular noise texture, 15-bit, medium sustain
            //   f = 524288 / 2 = 262144 Hz. Decay = 7 * 31.3ms = 219ms.
            {0, false, 1, 7, 2, 0},
         // ma (maracas): short noise burst, 15-bit, fast decay
            //   f = 524288 / 1 = 524288 Hz. Decay = 8 * 15.6ms = 125ms.
            {0, false, 0, 8, 1, 0},
         // ag (agogo): tonal bell, 7-bit, short-medium ring
            //   f = 524288 / 16 = 32768 Hz. Decay = 11 * 31.3ms = 344ms.
            {0, true, 4, 11, 2, 0},
         }
    };

    /// @brief Get noise drum preset for a drum note.
    consteval const drum_preset& note_to_drum(note n) {
        if (!is_drum(n)) throw "note_to_drum: not a drum note";
        return drum_preset_table[static_cast<int>(n) - static_cast<int>(first_drum)];
    }

    // -- Instrument configurations ---------------------------------------

    /// @brief SQ1 instrument: sweep + duty/envelope.
    struct sq1_instrument {
        // Sweep
        std::uint8_t sweep_shift     : 3 = 0;
        std::uint8_t sweep_direction : 1 = 0; ///< 0 = increase, 1 = decrease.
        std::uint8_t sweep_time      : 3 = 0;
        // Duty/envelope
        std::uint8_t duty          : 2 = 2; ///< Default 50% duty.
        std::uint8_t env_step      : 3 = 0;
        std::uint8_t env_direction : 1 = 0;
        std::uint8_t env_volume    : 4 = 5; ///< Default ~33% volume.

        consteval bool operator==(const sq1_instrument&) const = default;
    };

    /// @brief SQ2 instrument: duty/envelope (no sweep).
    struct sq2_instrument {
        std::uint8_t duty          : 2 = 2; ///< Default 50% duty.
        std::uint8_t env_step      : 3 = 0;
        std::uint8_t env_direction : 1 = 0;
        std::uint8_t env_volume    : 4 = 5; ///< Default ~33% volume.

        consteval bool operator==(const sq2_instrument&) const = default;
    };

    /// @brief WAV instrument: volume + waveform data.
    struct wav_instrument {
        std::uint8_t volume : 2 = 1; ///< 0=mute, 1=100%, 2=50%, 3=25%.
        bool force_75       : 1 = false;
        std::array<std::uint32_t, 8> wave_data = {
            0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210, 0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210,
        };

        consteval bool operator==(const wav_instrument&) const = default;
    };

    /// @brief NOISE instrument: envelope.
    ///
    /// Per-drum frequency params come from drum_preset_table. The instrument
    /// only controls the envelope defaults for pitched noise use.
    struct noise_instrument {
        std::uint8_t env_step      : 3 = 0;
        std::uint8_t env_direction : 1 = 0;
        std::uint8_t env_volume    : 4 = 5; ///< Default ~33% volume.

        consteval bool operator==(const noise_instrument&) const = default;
    };

    // -- Waveform helpers ----------------------------------------------

    /// @brief Create a wav_instrument from 64 raw 4-bit sample values (0-15).
    ///
    /// The GBA wave channel in 1x64 mode plays 64 4-bit samples across both
    /// banks. Samples are packed two per byte, high nibble first, into
    /// 8 x uint32_t words (bank 0 = words 0-3, bank 1 = words 4-7).
    ///
    /// Example:
    /// @code{.cpp}
    /// constexpr auto my_wave = gba::music::wav_from_samples({
    ///     8,  9, 11, 12, 13, 14, 14, 15,  15, 15, 14, 14, 13, 12, 11,  9,
    ///     8,  6,  5,  3,  2,  1,  1,  0,   0,  0,  1,  1,  2,  3,  5,  6,
    ///     8,  9, 11, 12, 13, 14, 14, 15,  15, 15, 14, 14, 13, 12, 11,  9,
    ///     8,  6,  5,  3,  2,  1,  1,  0,   0,  0,  1,  1,  2,  3,  5,  6,
    /// });
    /// @endcode
    consteval wav_instrument wav_from_samples(const std::array<std::uint8_t, 64>& samples, std::uint8_t volume = 1) {
        wav_instrument inst{};
        inst.volume = volume;
        for (int word = 0; word < 8; ++word) {
            std::uint32_t val = 0;
            for (int byte = 0; byte < 4; ++byte) {
                int idx = word * 8 + byte * 2;
                std::uint8_t hi = samples[idx] & 0xF;
                std::uint8_t lo = samples[idx + 1] & 0xF;
                val |= static_cast<std::uint32_t>((hi << 4) | lo) << (byte * 8);
            }
            inst.wave_data[word] = val;
        }
        return inst;
    }

    // -- Built-in waveforms ----------------------------------------------

    namespace waves {
        /// @brief Sine waveform - smooth, warm, only fundamental harmonic.
        inline constexpr wav_instrument sine = wav_from_samples({
            8,  8,  9,  10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 14, 14, 14,
            14, 13, 13, 13, 12, 12, 11, 11, 10, 10, 8,  7,  6,  6,  5,  4,  4,  3,  3,  2,  2,  2,
            1,  1,  1,  1,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  3,  3,  4,  4,  5,  5,
        });

        /// @brief Triangle waveform - bright, nasal, odd harmonics (1/n^2 rolloff).
        inline constexpr wav_instrument triangle = wav_from_samples({
            0,  0,  1,  1,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10, 11,
            11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10,
            9,  9,  8,  8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  1,  1,  0,  0,  0,
        });

        /// @brief Sawtooth waveform - buzzy, rich in all harmonics (1/n rolloff).
        inline constexpr wav_instrument saw = wav_from_samples({
            0,  0,  0,  1,  1, 1, 2, 2, 2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,
            7,  7,  8,  8,  8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14,
            14, 15, 15, 15, 0, 0, 0, 1, 1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,
        });

        /// @brief Square waveform - hollow, retro, strong odd harmonics (1/n rolloff).
        inline constexpr wav_instrument square = wav_from_samples({
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        });
    } // namespace waves

    /// @brief Create a wav_instrument from a PCM byte array (auto-resample).
    ///
    /// Accepts N unsigned 8-bit PCM samples, resamples to 64 samples, and
    /// quantizes to 4-bit for the GBA wave channel in 1x64 mode.
    template<std::size_t N>
    consteval wav_instrument wav_from_pcm8(const std::uint8_t (&pcm)[N], std::uint8_t volume = 1) {
        static_assert(N > 0, "PCM data must not be empty");
        std::array<std::uint8_t, 64> samples{};
        for (int i = 0; i < 64; ++i) {
            // Nearest-neighbour resample
            std::size_t srcIdx = static_cast<std::size_t>(i) * N / 64;
            // Quantize 8-bit to 4-bit
            samples[i] = pcm[srcIdx] >> 4;
        }
        return wav_from_samples(samples, volume);
    }

    // -- WAV file embed --------------------------------------------------

    namespace wav_detail {

        consteval std::uint16_t read_u16(const std::uint8_t* p) {
            return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
        }

        consteval std::uint32_t read_u32(const std::uint8_t* p) {
            return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
        }

        consteval bool tag_eq(const std::uint8_t* p, char a, char b, char c, char d) {
            return p[0] == static_cast<std::uint8_t>(a) && p[1] == static_cast<std::uint8_t>(b) &&
                   p[2] == static_cast<std::uint8_t>(c) && p[3] == static_cast<std::uint8_t>(d);
        }

        /// @brief Read one sample frame as unsigned 0-255, left channel only.
        consteval int read_sample_u8(const std::uint8_t* data, std::size_t data_start, std::size_t frame,
                                     std::size_t bytes_per_frame, std::uint16_t bits_per_sample) {
            std::size_t bytePos = data_start + frame * bytes_per_frame;
            if (bits_per_sample == 8) {
                return data[bytePos]; // unsigned 8-bit PCM
            } else {
                // 16-bit signed little-endian -> unsigned 8-bit
                auto signed_val =
                    static_cast<std::int16_t>(static_cast<std::uint16_t>(data[bytePos] | (data[bytePos + 1] << 8)));
                return (signed_val + 32768) >> 8;
            }
        }

        /// @brief Internal: parse WAV bytes and resample to wav_instrument.
        ///
        /// When max_samples == 0 (default), performs auto single-cycle extraction:
        /// skips the attack transient, finds two consecutive positive-going zero
        /// crossings in the sustain region, and resamples that one waveform period
        /// to 64 samples with min/max normalization. This produces a clean tonal
        /// waveform suitable for the GBA wave channel.
        ///
        /// When max_samples > 0, resamples the first N source frames with
        /// normalization (useful for hand-picked windows).
        consteval wav_instrument parse_wav(const std::uint8_t* data, std::size_t N, std::uint8_t volume,
                                           std::size_t max_samples) {
            if (N < 44) throw "wav_embed: file too small for WAV header";
            if (!tag_eq(data, 'R', 'I', 'F', 'F')) throw "wav_embed: not a RIFF file";
            if (!tag_eq(data + 8, 'W', 'A', 'V', 'E')) throw "wav_embed: not a WAVE file";

            // Walk chunks to find fmt and data
            std::uint16_t audio_format = 0;
            std::uint16_t channels = 0;
            std::uint16_t bits_per_sample = 0;
            std::size_t data_start = 0;
            std::size_t data_size = 0;

            std::size_t pos = 12;
            while (pos + 8 <= N) {
                auto chunk_size = static_cast<std::size_t>(read_u32(data + pos + 4));

                if (tag_eq(data + pos, 'f', 'm', 't', ' ')) {
                    if (pos + 24 > N) throw "wav_embed: fmt chunk truncated";
                    audio_format = read_u16(data + pos + 8);
                    channels = read_u16(data + pos + 10);
                    bits_per_sample = read_u16(data + pos + 22);
                    if (audio_format != 1) throw "wav_embed: only PCM format (1) is supported";
                }

                if (tag_eq(data + pos, 'd', 'a', 't', 'a')) {
                    data_start = pos + 8;
                    data_size = chunk_size;
                    break;
                }

                pos += 8 + chunk_size;
                if (chunk_size % 2) pos++; // RIFF pad byte
            }

            if (data_start == 0) throw "wav_embed: data chunk not found";
            if (channels == 0 || channels > 2) throw "wav_embed: only mono or stereo supported";
            if (bits_per_sample != 8 && bits_per_sample != 16)
                throw "wav_embed: only 8-bit or 16-bit samples supported";

            std::size_t bytes_per_frame = (bits_per_sample / 8) * channels;
            std::size_t total_frames = data_size / bytes_per_frame;
            if (total_frames == 0) throw "wav_embed: no samples in data chunk";

            // Determine the source window to resample
            std::size_t win_start = 0;
            std::size_t win_len = total_frames;

            if (max_samples > 0 && max_samples < total_frames) {
                // Manual mode: use first max_samples frames
                win_len = max_samples;
            } else if (total_frames > 128) {
                // Auto single-cycle extraction from sustain portion.
                // Skip attack transient (first 25%), search in the middle 50%.
                constexpr int midpoint = 128; // unsigned 8-bit midpoint
                std::size_t skip = total_frames / 4;
                std::size_t search_end = total_frames * 3 / 4;

                // Find first positive-going zero-crossing after skip
                std::size_t cycle_start = skip;
                for (std::size_t f = skip + 1; f < search_end; ++f) {
                    int prev = read_sample_u8(data, data_start, f - 1, bytes_per_frame, bits_per_sample);
                    int curr = read_sample_u8(data, data_start, f, bytes_per_frame, bits_per_sample);
                    if (prev < midpoint && curr >= midpoint) {
                        cycle_start = f;
                        break;
                    }
                }

                // Find next positive-going zero-crossing (one period later)
                std::size_t cycle_end = cycle_start + 2; // minimum 2 samples
                for (std::size_t f = cycle_start + 2; f < search_end; ++f) {
                    int prev = read_sample_u8(data, data_start, f - 1, bytes_per_frame, bits_per_sample);
                    int curr = read_sample_u8(data, data_start, f, bytes_per_frame, bits_per_sample);
                    if (prev < midpoint && curr >= midpoint) {
                        cycle_end = f;
                        break;
                    }
                }

                win_start = cycle_start;
                win_len = cycle_end - cycle_start;
                if (win_len < 2) win_len = 2; // safety
            }

            // First pass: find min/max in the source window for normalization
            int minVal = 255, maxVal = 0;
            for (std::size_t f = 0; f < win_len; ++f) {
                int v = read_sample_u8(data, data_start, win_start + f, bytes_per_frame, bits_per_sample);
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
            int range = maxVal - minVal;
            if (range == 0) range = 1; // avoid divide-by-zero for silence

            // Second pass: nearest-neighbour resample to 64 samples, normalized to 0-15
            std::array<std::uint8_t, 64> samples{};
            for (int i = 0; i < 64; ++i) {
                std::size_t srcFrame = win_start + static_cast<std::size_t>(i) * win_len / 64;
                int value = read_sample_u8(data, data_start, srcFrame, bytes_per_frame, bits_per_sample);
                samples[i] = static_cast<std::uint8_t>((value - minVal) * 15 / range);
            }

            return wav_from_samples(samples, volume);
        }

    } // namespace wav_detail

    /// @brief Parse a .wav file at compile time and create a wav_instrument.
    ///
    /// Supports PCM .wav files (format 1), mono or stereo, 8-bit or 16-bit.
    ///
    /// By default (max_samples == 0), performs **auto single-cycle extraction**:
    /// skips the attack transient, finds one waveform period via zero-crossing
    /// detection in the sustain region, and resamples it to 64 x 4-bit samples
    /// with min/max normalization. This produces a clean tonal waveform.
    ///
    /// Pass `max_samples` to manually limit to the first N source frames
    /// (useful for hand-picked windows or synthetic waveforms).
    ///
    /// By default the entire file is resampled. Pass `max_samples` to limit
    /// to the first N source samples (useful for extracting one waveform cycle
    /// from a longer recording).
    ///
    /// Usage with `#embed` (C++26 / GCC `-std=gnu++26`):
    /// @code{.cpp}
    /// static constexpr std::uint8_t piano_data[] = {
    ///     #embed "Piano.wav"
    /// };
    /// static constexpr auto piano = wav_embed(piano_data);
    ///
    /// // Extract first 64 source samples (one cycle at ~260 Hz / 8 kHz SR):
    /// static constexpr auto piano_cycle = wav_embed(piano_data, 1, 64);
    /// @endcode
    template<std::size_t N>
    consteval wav_instrument wav_embed(const std::uint8_t (&data)[N], std::uint8_t volume = 1,
                                       std::size_t max_samples = 0) {
        return wav_detail::parse_wav(data, N, volume, max_samples);
    }

    /// @brief Parse a .wav file from a supplier lambda at compile time.
    ///
    /// Follows the same supplier pattern as gba::embed::bitmap15(),
    /// gba::embed::indexed4(), etc. The supplier must return a
    /// `std::array<unsigned char, N>` or `std::to_array<unsigned char>(...)`.
    ///
    /// Usage with `#embed` (C++26 / GCC `-std=gnu++26`):
    /// @code{.cpp}
    /// static constexpr auto piano = gba::music::wav_embed([] {
    ///     return std::to_array<unsigned char>({
    /// #embed "Piano.wav"
    ///     });
    /// });
    ///
    /// // Extract first 64 source samples (one waveform cycle):
    /// static constexpr auto piano_cycle = gba::music::wav_embed([] {
    ///     return std::to_array<unsigned char>({
    /// #embed "Piano.wav"
    ///     });
    /// }, 1, 64);
    /// @endcode
    consteval wav_instrument wav_embed(auto supplier, std::uint8_t volume = 2, std::size_t max_samples = 0) {
        const auto raw = supplier();
        return wav_detail::parse_wav(raw.data(), raw.size(), volume, max_samples);
    }

    // -- Note name parsing -----------------------------------------------

    /// @brief Parse a note name from a string at compile time.
    ///
    /// Accepts: c2-b8 with optional #/s for sharp or b for flat,
    /// bare note letters (c, d, e, f, g, a, b) defaulting to octave 3,
    /// drum names (bd, sd, hh, oh, cp, rs, rim, lt, mt, ht, cb, cr, rd,
    /// hc, mc, lc, cl, sh, ma, ag),
    /// and ~ for rest, _ for hold.
    ///
    /// Octave 1 notes (c1-b1) are parseable but will cause a consteval error
    /// if compiled for a pitched PSG channel, since the hardware cannot
    /// represent frequencies below ~64 Hz (C2).
    ///
    /// Flat notation: eb3 = Eb3 = D#3. Bare flat: eb = Eb3.
    ///
    /// @return {parsed note, number of characters consumed}.
    struct parse_note_result {
        note value;
        std::size_t length;
    };

    consteval parse_note_result parse_note_name(const char* str, std::size_t pos, std::size_t end) {
        if (pos >= end) throw "parse_note_name: unexpected end of input";

        // Rest
        if (str[pos] == '~') return {note::rest, 1};

        // Hold / tie: _ sustains the previous note without retriggering.
        if (str[pos] == '_') return {note::hold, 1};

        // Try 3-letter drum names first (before 2-letter, to match greedily).
        // Must check that the 3rd char is NOT a digit (to avoid ambiguity
        // with hypothetical extensions), and that the match is followed by
        // a delimiter (space, operator, bracket, end).
        if (pos + 2 < end) {
            auto c0 = str[pos], c1 = str[pos + 1], c2 = str[pos + 2];
            // rim = rimshot (Strudel name)
            if (c0 == 'r' && c1 == 'i' && c2 == 'm') {
                // Verify it's not part of a longer identifier
                if (pos + 3 >= end || str[pos + 3] == ' ' || str[pos + 3] == '\t' || str[pos + 3] == '\n' ||
                    str[pos + 3] == '\r' || str[pos + 3] == ']' || str[pos + 3] == '>' || str[pos + 3] == ')' ||
                    str[pos + 3] == ',' || str[pos + 3] == '*' || str[pos + 3] == '/' || str[pos + 3] == '!' ||
                    str[pos + 3] == '(' || str[pos + 3] == '@' || str[pos + 3] == '}' || str[pos + 3] == '%')
                    return {note::rim, 3};
            }
        }

        // Try drum names (2-letter)
        // A drum name only matches if NOT followed by a digit (to disambiguate
        // from flat notation: cb4 = Cb4, not cowbell + 4).
        if (pos + 1 < end) {
            auto c0 = str[pos], c1 = str[pos + 1];
            bool is_pitched = (c0 >= 'a' && c0 <= 'g') && (c1 >= '0' && c1 <= '9');
            bool is_flat = (c0 >= 'a' && c0 <= 'g') && c1 == 'b' &&
                           (pos + 2 < end && str[pos + 2] >= '0' && str[pos + 2] <= '9');
            if (!is_pitched && !is_flat) {
                if (c0 == 'b' && c1 == 'd') return {note::bd, 2};
                if (c0 == 's' && c1 == 'd') return {note::sd, 2};
                if (c0 == 'h' && c1 == 'h') return {note::hh, 2};
                if (c0 == 'o' && c1 == 'h') return {note::oh, 2};
                if (c0 == 'c' && c1 == 'p') return {note::cp, 2};
                if (c0 == 'r' && c1 == 's') return {note::rs, 2};
                if (c0 == 'l' && c1 == 't') return {note::lt, 2};
                if (c0 == 'm' && c1 == 't') return {note::mt, 2};
                if (c0 == 'h' && c1 == 't') return {note::ht, 2};
                if (c0 == 'c' && c1 == 'b') return {note::cb, 2};
                if (c0 == 'c' && c1 == 'r') return {note::cr, 2};
                if (c0 == 'r' && c1 == 'd') return {note::rd, 2};
                if (c0 == 'h' && c1 == 'c') return {note::hc, 2};
                if (c0 == 'm' && c1 == 'c') return {note::mc, 2};
                if (c0 == 'l' && c1 == 'c') return {note::lc, 2};
                if (c0 == 'c' && c1 == 'l') return {note::cl, 2};
                if (c0 == 's' && c1 == 'h') return {note::sh, 2};
                if (c0 == 'm' && c1 == 'a') return {note::ma, 2};
                if (c0 == 'a' && c1 == 'g') return {note::ag, 2};
            }
        }

        // Pitched note: letter [#/s/b] octave
        char letter = str[pos];
        if (letter < 'a' || letter > 'g') throw "parse_note_name: expected note letter a-g";

        // Semitone offsets: c=0, d=2, e=4, f=5, g=7, a=9, b=11
        constexpr int offsets[] = {9, 11, 0, 2, 4, 5, 7}; // a=9,b=11,c=0,d=2,e=4,f=5,g=7
        int semitone = offsets[letter - 'a'];

        std::size_t consumed = 1;

        // Accidental: sharp (#, s) or flat (b)
        if (pos + consumed < end && str[pos + consumed] == '#') {
            semitone++;
            consumed++;
        } else if (pos + consumed < end && str[pos + consumed] == 's') {
            // 's' as sharp (alternative notation: cs4 = C#4)
            // Only if next char is a digit or end/space (to avoid matching 'sd' drum)
            if (pos + consumed + 1 < end && str[pos + consumed + 1] >= '0' && str[pos + consumed + 1] <= '9') {
                semitone++;
                consumed++;
            } else if (pos + consumed + 1 >= end || str[pos + consumed + 1] == ' ' || str[pos + consumed + 1] == '\n' ||
                       str[pos + consumed + 1] == '\r' || str[pos + consumed + 1] == '\t' ||
                       str[pos + consumed + 1] == ']' || str[pos + consumed + 1] == '>' ||
                       str[pos + consumed + 1] == ')' || str[pos + consumed + 1] == ',' ||
                       str[pos + consumed + 1] == '*' || str[pos + consumed + 1] == '/' ||
                       str[pos + consumed + 1] == '!' || str[pos + consumed + 1] == '(' ||
                       str[pos + consumed + 1] == '@') {
                // Bare sharp without octave: cs = C#3
                semitone++;
                consumed++;
            }
        } else if (pos + consumed < end && str[pos + consumed] == 'b') {
            // 'b' as flat (eb3 = E-flat3 = D#3)
            // Only if next char is a digit or end/space (to avoid consuming note letter 'b')
            if (pos + consumed + 1 < end && str[pos + consumed + 1] >= '0' && str[pos + consumed + 1] <= '9') {
                semitone--;
                consumed++;
            } else if (pos + consumed + 1 >= end || str[pos + consumed + 1] == ' ' || str[pos + consumed + 1] == '\n' ||
                       str[pos + consumed + 1] == '\r' || str[pos + consumed + 1] == '\t' ||
                       str[pos + consumed + 1] == ']' || str[pos + consumed + 1] == '>' ||
                       str[pos + consumed + 1] == ')' || str[pos + consumed + 1] == ',' ||
                       str[pos + consumed + 1] == '*' || str[pos + consumed + 1] == '/' ||
                       str[pos + consumed + 1] == '!' || str[pos + consumed + 1] == '(' ||
                       str[pos + consumed + 1] == '@') {
                // Bare flat without octave: eb = Eb3
                semitone--;
                consumed++;
            }
        }

        // Octave -- accept 1-8, or default to 3 if absent (Strudel convention).
        int octave = 3;
        if (pos + consumed < end && str[pos + consumed] >= '1' && str[pos + consumed] <= '8') {
            octave = str[pos + consumed] - '0';
            consumed++;
        }

        // Handle wrap-around for sharps (e.g., b#4 -> c5)
        if (semitone >= 12) {
            semitone -= 12;
            octave++;
        }
        // Handle wrap-around for flats (e.g., cb4 -> b3)
        if (semitone < 0) {
            semitone += 12;
            octave--;
        }

        if (octave < 1 || octave > 8) throw "parse_note_name: octave out of range (1-8)";

        int index = (octave - 1) * 12 + semitone;
        if (index < 0 || index >= 96) throw "parse_note_name: note out of PSG range";

        auto result = static_cast<note>(static_cast<int>(first_chromatic) + index);
        return {result, consumed};
    }

} // namespace gba::music
