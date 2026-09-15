/// @file bits/embed/wav.hpp
/// @brief Compile-time 8-bit PCM WAV embedding.
#pragma once

#include <gba/bits/constexpr_assert.hpp>
#include <gba/peripherals>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace gba::embed {

    namespace bits {

        struct wav_layout {
            unsigned int sample_rate;
            std::size_t data_offset;
            std::size_t sample_count;
        };

        consteval std::uint16_t wav_read_u16(const unsigned char* data) {
            return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
        }

        consteval std::uint32_t wav_read_u32(const unsigned char* data) {
            return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
                   (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
        }

        consteval bool wav_tag(const unsigned char* data, char a, char b, char c, char d) {
            return data[0] == static_cast<unsigned char>(a) && data[1] == static_cast<unsigned char>(b) &&
                   data[2] == static_cast<unsigned char>(c) && data[3] == static_cast<unsigned char>(d);
        }

        template<std::size_t Size>
        consteval wav_layout parse_wav_layout(const std::array<unsigned char, Size>& data) {
            ::gba::bits::constexpr_assert(Size < 12, "embed::wav: file is too small");
            ::gba::bits::constexpr_assert(!wav_tag(data.data(), 'R', 'I', 'F', 'F'),
                                          "embed::wav: expected a RIFF file");
            ::gba::bits::constexpr_assert(!wav_tag(data.data() + 8, 'W', 'A', 'V', 'E'),
                                          "embed::wav: expected a WAVE file");

            bool found_format = false;
            std::uint16_t audio_format = 0;
            std::uint16_t channels = 0;
            std::uint16_t block_align = 0;
            std::uint16_t bits_per_sample = 0;
            std::uint32_t sample_rate = 0;
            std::size_t sample_offset = 0;
            std::size_t sample_count = 0;

            std::size_t position = 12;
            while (position + 8 <= Size) {
                const auto chunk_size = static_cast<std::size_t>(wav_read_u32(data.data() + position + 4));
                const auto chunk_data = position + 8;
                ::gba::bits::constexpr_assert(chunk_size > Size - chunk_data, "embed::wav: truncated chunk");

                if (wav_tag(data.data() + position, 'f', 'm', 't', ' ')) {
                    ::gba::bits::constexpr_assert(chunk_size < 16, "embed::wav: truncated fmt chunk");
                    audio_format = wav_read_u16(data.data() + chunk_data);
                    channels = wav_read_u16(data.data() + chunk_data + 2);
                    sample_rate = wav_read_u32(data.data() + chunk_data + 4);
                    block_align = wav_read_u16(data.data() + chunk_data + 12);
                    bits_per_sample = wav_read_u16(data.data() + chunk_data + 14);
                    found_format = true;
                } else if (wav_tag(data.data() + position, 'd', 'a', 't', 'a')) {
                    sample_offset = chunk_data;
                    sample_count = chunk_size;
                }

                position = chunk_data + chunk_size + (chunk_size & 1);
            }

            ::gba::bits::constexpr_assert(!found_format, "embed::wav: fmt chunk not found");
            ::gba::bits::constexpr_assert(sample_offset == 0, "embed::wav: data chunk not found");
            ::gba::bits::constexpr_assert(audio_format != 1, "embed::wav: only uncompressed PCM is supported");
            ::gba::bits::constexpr_assert(channels != 1, "embed::wav: only mono audio is supported");
            ::gba::bits::constexpr_assert(bits_per_sample != 8, "embed::wav: only 8-bit samples are supported");
            ::gba::bits::constexpr_assert(block_align != 1, "embed::wav: invalid 8-bit mono block alignment");
            ::gba::bits::constexpr_assert(sample_rate == 0, "embed::wav: sample rate must not be zero");
            ::gba::bits::constexpr_assert(sample_count == 0, "embed::wav: sample data is empty");

            return {sample_rate, sample_offset, sample_count};
        }

    } // namespace bits

    /// @brief Embedded mono 8-bit PCM ready for Direct Sound DMA.
    ///
    /// WAV stores 8-bit PCM as unsigned bytes, while Direct Sound expects signed
    /// bytes. `samples` contains converted signed data followed by a linear fade
    /// to silence for frame-counted playback and a silent Direct Sound FIFO lookahead.
    template<std::size_t SampleCount, unsigned int SampleRate>
    struct alignas(4) wav_result {
        static constexpr unsigned int clock_cycles_per_second = 16777216;
        static constexpr std::size_t clock_cycles_per_frame = 280896;
        static constexpr std::size_t fifo_lookahead = 16;

        static constexpr unsigned int sample_rate = SampleRate;
        static constexpr std::size_t sample_count = SampleCount;
        static constexpr unsigned int timer_period = static_cast<unsigned int>(
            (static_cast<std::uint64_t>(clock_cycles_per_second) + sample_rate / 2) / sample_rate);
        static_assert(timer_period > 0 && timer_period <= 65536,
                      "embed::wav: sample rate cannot be represented by a Direct Sound timer");
        static constexpr gba::timer_config timer{
            static_cast<unsigned short>(-timer_period), {.cycles = gba::cycles_1, .enabled = true}
        };

        static constexpr auto frame_count_wide =
            (static_cast<std::uint64_t>(sample_count) * timer_period + clock_cycles_per_frame - 1) /
            clock_cycles_per_frame;
        static constexpr auto samples_before_stop_wide = frame_count_wide * clock_cycles_per_frame / timer_period;
        static constexpr auto fade_sample_count_wide = samples_before_stop_wide - sample_count;
        static constexpr auto padded_sample_count_wide = (samples_before_stop_wide + fifo_lookahead + 15) &
                                                         ~std::uint64_t{15};
        static_assert(padded_sample_count_wide <= std::numeric_limits<std::size_t>::max(),
                      "embed::wav: padded sample data is too large");

        static constexpr std::size_t frame_count = static_cast<std::size_t>(frame_count_wide);
        static constexpr std::size_t fade_sample_count = static_cast<std::size_t>(fade_sample_count_wide);
        static constexpr std::size_t padded_sample_count = static_cast<std::size_t>(padded_sample_count_wide);

        std::array<std::int8_t, padded_sample_count> samples;
    };

    /// @brief Load an uncompressed mono 8-bit PCM WAV at compile time.
    ///
    /// @tparam TargetSampleRate Output sample rate. Zero preserves the source rate.
    /// @param supplier Callable returning the WAV file as `std::array<unsigned char, N>`.
    /// @return Signed PCM samples, frame count, timer period, and a silent DMA tail.
    ///
    /// Example:
    /// @code{.cpp}
    /// static constexpr auto sound = gba::embed::wav([] {
    ///     return std::to_array<unsigned char>({
    /// #embed "sound.wav"
    ///     });
    /// });
    /// @endcode
    template<unsigned int TargetSampleRate = 0>
    consteval auto wav(auto supplier) {
        constexpr auto raw = supplier();
        constexpr auto layout = bits::parse_wav_layout(raw);
        constexpr auto output_rate = TargetSampleRate == 0 ? layout.sample_rate : TargetSampleRate;
        static_assert(output_rate > 0, "embed::wav: target sample rate must not be zero");
        constexpr auto output_count_wide =
            (static_cast<std::uint64_t>(layout.sample_count) * output_rate + layout.sample_rate / 2) /
            layout.sample_rate;
        static_assert(output_count_wide <= std::numeric_limits<std::size_t>::max(),
                      "embed::wav: resampled data is too large");
        constexpr auto output_count = static_cast<std::size_t>(output_count_wide);
        static_assert(output_count > 0, "embed::wav: resampling produced no samples");

        return [&]<std::size_t SampleCount>(std::integral_constant<std::size_t, SampleCount>) consteval {
            wav_result<SampleCount, output_rate> result{};
            for (std::size_t index = 0; index < SampleCount; ++index) {
                const auto source_position = index * layout.sample_rate;
                const auto source_index = source_position / output_rate;
                const auto fraction = source_position % output_rate;
                const auto next_index = source_index + 1 < layout.sample_count ? source_index + 1
                                                                               : layout.sample_count - 1;
                const auto current = static_cast<int>(raw[layout.data_offset + source_index]) - 128;
                const auto next = static_cast<int>(raw[layout.data_offset + next_index]) - 128;
                result.samples[index] = static_cast<std::int8_t>(
                    current + (next - current) * static_cast<int>(fraction) / static_cast<int>(output_rate));
            }
            const auto final_sample = static_cast<std::int64_t>(result.samples[SampleCount - 1]);
            for (std::size_t index = 0; index < result.fade_sample_count; ++index) {
                const auto remaining = result.fade_sample_count - index;
                result.samples[SampleCount + index] =
                    static_cast<std::int8_t>(final_sample * static_cast<std::int64_t>(remaining) /
                                             static_cast<std::int64_t>(result.fade_sample_count + 1));
            }
            return result;
        }(std::integral_constant<std::size_t, output_count>{});
    }

} // namespace gba::embed
