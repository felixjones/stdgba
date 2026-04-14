/// @file bits/embed/png_chunks.hpp
/// @brief Compile-time PNG chunk scanner and streaming IDAT reader.
#pragma once

#include <gba/bits/embed/png_header.hpp>

namespace gba::embed::bits {


    struct png_chunk_info {
        struct span {
            std::size_t offset;
            std::size_t length;
        };

        std::size_t plte_offset = 0;
        unsigned int plte_count = 0;
        std::size_t trns_offset = 0;
        unsigned int trns_len = 0;
        span idat[64]{};
        unsigned int idat_count = 0;
        std::size_t idat_total = 0;
    };

    template<std::size_t Size>
    consteval png_chunk_info png_scan_chunks(const std::array<unsigned char, Size>& data) {
        png_chunk_info info{};
        std::size_t pos = 33; // skip signature(8) + IHDR(4+4+13+4)
        while (pos + 12 <= Size) {
            auto chunk_len = png_read_u32(data, pos);
            auto t0 = data[pos + 4], t1 = data[pos + 5], t2 = data[pos + 6], t3 = data[pos + 7];

            if (t0 == 'P' && t1 == 'L' && t2 == 'T' && t3 == 'E') {
                info.plte_offset = pos + 8;
                info.plte_count = chunk_len / 3;
            } else if (t0 == 't' && t1 == 'R' && t2 == 'N' && t3 == 'S') {
                info.trns_offset = pos + 8;
                info.trns_len = chunk_len;
            } else if (t0 == 'I' && t1 == 'D' && t2 == 'A' && t3 == 'T') {
                if (info.idat_count >= 64) throw "PNG: too many IDAT chunks";
                info.idat[info.idat_count++] = {pos + 8, chunk_len};
                info.idat_total += chunk_len;
            } else if (t0 == 'I' && t1 == 'E' && t2 == 'N' && t3 == 'D') {
                break;
            }
            pos += 12 + chunk_len;
        }
        if (info.idat_count == 0) throw "PNG: no IDAT data found";
        return info;
    }


    template<std::size_t Size>
    struct png_idat_reader {
        const std::array<unsigned char, Size>& file;
        const png_chunk_info& chunks;
        unsigned int cur_span = 0;
        std::size_t cur_offset = 0;
        unsigned int bit_buf = 0;
        unsigned int bits_left = 0;

        consteval unsigned char next_byte() {
            while (cur_span < chunks.idat_count) {
                if (cur_offset < chunks.idat[cur_span].length) {
                    return file[chunks.idat[cur_span].offset + cur_offset++];
                }
                ++cur_span;
                cur_offset = 0;
            }
            throw "PNG deflate: unexpected end of IDAT stream";
        }

        consteval void skip_zlib_header() {
            auto cmf = next_byte();
            next_byte(); // FLG
            if ((cmf & 0x0F) != 8) throw "PNG: zlib compression must be deflate";
        }

        consteval void align_to_byte() {
            bit_buf = 0;
            bits_left = 0;
        }
    };

} // namespace gba::embed::bits
