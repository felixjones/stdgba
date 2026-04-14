/// @file bits/embed/png.hpp
/// @brief Consteval PNG decoder for compile-time image embedding.
///
/// Supports non-interlaced PNG with 8-bit channels:
/// - Color type 2 (RGB)
/// - Color type 3 (indexed with PLTE)
/// - Color type 6 (RGBA)
///
/// Consteval performance notes:
/// - Single-pass chunk scan (PLTE, tRNS, IDAT positions collected in one traversal)
/// - Streaming IDAT reader eliminates large intermediate copy buffer
/// - Counting-sort Huffman build: O(n) instead of O(n × max_bits)
/// - Filter type and color type dispatched outside inner pixel loops
#pragma once

#include <gba/video>

#include <array>
#include <cstddef>
#include <cstdint>


namespace gba::embed::bits {

    // ---------------------------------------------------------------
    // PNG signature / IHDR
    // ---------------------------------------------------------------

    struct png_ihdr {
        unsigned int width;
        unsigned int height;
        unsigned int bit_depth;
        unsigned int color_type;
    };

    template<std::size_t Size>
    consteval unsigned int png_read_u32(const std::array<unsigned char, Size>& data, std::size_t pos) {
        return (static_cast<unsigned int>(data[pos]) << 24) | (static_cast<unsigned int>(data[pos + 1]) << 16) |
               (static_cast<unsigned int>(data[pos + 2]) << 8) | static_cast<unsigned int>(data[pos + 3]);
    }

    template<std::size_t Size>
    consteval bool png_check_signature(const std::array<unsigned char, Size>& data) {
        if (Size < 8) return false;
        return data[0] == 137 && data[1] == 80 && data[2] == 78 && data[3] == 71 &&
               data[4] == 13 && data[5] == 10 && data[6] == 26 && data[7] == 10;
    }

    template<std::size_t Size>
    consteval bool png_chunk_is(const std::array<unsigned char, Size>& data, std::size_t pos,
                                unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
        return data[pos] == a && data[pos + 1] == b && data[pos + 2] == c && data[pos + 3] == d;
    }

    template<std::size_t Size>
    consteval png_ihdr png_parse_ihdr(const std::array<unsigned char, Size>& data) {
        if (Size < 33) throw "PNG: file too small for IHDR";
        if (png_read_u32(data, 8) != 13) throw "PNG: IHDR chunk length must be 13";
        if (!png_chunk_is(data, 12, 'I', 'H', 'D', 'R')) throw "PNG: first chunk must be IHDR";

        png_ihdr hdr{};
        hdr.width = png_read_u32(data, 16);
        hdr.height = png_read_u32(data, 20);
        hdr.bit_depth = data[24];
        hdr.color_type = data[25];
        if (data[26] != 0) throw "PNG: unsupported compression method";
        if (data[27] != 0) throw "PNG: unsupported filter method";
        if (data[28] != 0) throw "PNG: interlaced images not supported";

        if (hdr.width == 0 || hdr.height == 0) throw "PNG: invalid dimensions";
        if (hdr.bit_depth != 8) throw "PNG: only 8-bit depth supported";
        if (hdr.color_type != 2 && hdr.color_type != 3 && hdr.color_type != 6)
            throw "PNG: unsupported color type (only 2=RGB, 3=indexed, 6=RGBA)";

        return hdr;
    }

    // ---------------------------------------------------------------
    // Single-pass chunk scanner
    // ---------------------------------------------------------------

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

    // ---------------------------------------------------------------
    // Streaming IDAT reader (avoids large intermediate buffer)
    // ---------------------------------------------------------------

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

    // ---------------------------------------------------------------
    // Deflate decompressor (templatized on reader)
    // ---------------------------------------------------------------

    struct deflate_huff {
        unsigned int counts[16]{};
        unsigned int first_code[16]{};
        unsigned int first_index[16]{};
        unsigned int symbols[320]{};
        unsigned int max_bits = 0;
    };

    // Counting-sort Huffman build: O(count) instead of O(count × max_bits)
    consteval void deflate_build_huffman(deflate_huff& h, const unsigned int* lengths, unsigned int count) {
        for (unsigned int i = 0; i < 16; ++i) h.counts[i] = 0;
        for (unsigned int i = 0; i < count; ++i) {
            if (lengths[i] <= 15) ++h.counts[lengths[i]];
        }
        h.counts[0] = 0; // length 0 means unused

        // Compute first codes and first indices per bit length
        unsigned int code = 0;
        unsigned int idx = 0;
        h.max_bits = 0;
        unsigned int offsets[16]{};
        for (unsigned int bits = 1; bits <= 15; ++bits) {
            h.first_code[bits] = code;
            h.first_index[bits] = idx;
            offsets[bits] = idx;
            idx += h.counts[bits];
            code = (code + h.counts[bits]) << 1;
            if (h.counts[bits] > 0) h.max_bits = bits;
        }

        // Place symbols via counting sort — single O(count) pass
        for (unsigned int i = 0; i < count; ++i) {
            auto len = lengths[i];
            if (len > 0 && len <= 15) {
                h.symbols[offsets[len]++] = i;
            }
        }
    }

    consteval void deflate_build_fixed_lit(deflate_huff& h) {
        unsigned int lengths[288];
        for (unsigned int i = 0; i <= 143; ++i) lengths[i] = 8;
        for (unsigned int i = 144; i <= 255; ++i) lengths[i] = 9;
        for (unsigned int i = 256; i <= 279; ++i) lengths[i] = 7;
        for (unsigned int i = 280; i <= 287; ++i) lengths[i] = 8;
        deflate_build_huffman(h, lengths, 288);
    }

    consteval void deflate_build_fixed_dist(deflate_huff& h) {
        unsigned int lengths[32];
        for (unsigned int i = 0; i < 32; ++i) lengths[i] = 5;
        deflate_build_huffman(h, lengths, 32);
    }

    consteval unsigned int deflate_length_base(unsigned int sym) {
        constexpr unsigned int b[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        return b[sym - 257];
    }

    consteval unsigned int deflate_length_extra(unsigned int sym) {
        constexpr unsigned int e[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        return e[sym - 257];
    }

    consteval unsigned int deflate_dist_base(unsigned int sym) {
        constexpr unsigned int b[] = {1,    2,    3,    4,    5,    7,     9,     13,    17,   25,
                                      33,   49,   65,   97,   129,  193,   257,   385,   513,  769,
                                      1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
        return b[sym];
    }

    consteval unsigned int deflate_dist_extra(unsigned int sym) {
        constexpr unsigned int e[] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,
                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        return e[sym];
    }

    template<typename Reader>
    consteval unsigned int deflate_read_bits(Reader& r, unsigned int n) {
        if (n == 0) return 0;
        while (r.bits_left < n) {
            r.bit_buf |= static_cast<unsigned int>(r.next_byte()) << r.bits_left;
            r.bits_left += 8;
        }
        unsigned int val = r.bit_buf & ((1u << n) - 1);
        r.bit_buf >>= n;
        r.bits_left -= n;
        return val;
    }

    template<typename Reader>
    consteval unsigned int deflate_decode_symbol(Reader& r, const deflate_huff& h) {
        unsigned int code = 0;
        for (unsigned int bits = 1; bits <= h.max_bits; ++bits) {
            code = (code << 1) | deflate_read_bits(r, 1);
            if (h.counts[bits] > 0) {
                int idx = static_cast<int>(code) - static_cast<int>(h.first_code[bits]);
                if (idx >= 0 && static_cast<unsigned int>(idx) < h.counts[bits]) {
                    return h.symbols[h.first_index[bits] + idx];
                }
            }
        }
        throw "PNG deflate: invalid Huffman code";
    }

    template<typename Reader>
    consteval void deflate_decode_dynamic(Reader& r, deflate_huff& lit_h, deflate_huff& dist_h) {
        unsigned int hlit = deflate_read_bits(r, 5) + 257;
        unsigned int hdist = deflate_read_bits(r, 5) + 1;
        unsigned int hclen = deflate_read_bits(r, 4) + 4;

        constexpr unsigned int order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        unsigned int cl_lengths[19]{};
        for (unsigned int i = 0; i < hclen; ++i) cl_lengths[order[i]] = deflate_read_bits(r, 3);

        deflate_huff cl_h{};
        deflate_build_huffman(cl_h, cl_lengths, 19);

        unsigned int all_lengths[320]{};
        unsigned int total = hlit + hdist;
        unsigned int pos = 0;
        while (pos < total) {
            auto sym = deflate_decode_symbol(r, cl_h);
            if (sym < 16) {
                all_lengths[pos++] = sym;
            } else if (sym == 16) {
                if (pos == 0) throw "PNG deflate: repeat without previous length";
                auto repeat = deflate_read_bits(r, 2) + 3;
                auto prev = all_lengths[pos - 1];
                for (unsigned int i = 0; i < repeat && pos < total; ++i) all_lengths[pos++] = prev;
            } else if (sym == 17) {
                auto repeat = deflate_read_bits(r, 3) + 3;
                for (unsigned int i = 0; i < repeat && pos < total; ++i) all_lengths[pos++] = 0;
            } else if (sym == 18) {
                auto repeat = deflate_read_bits(r, 7) + 11;
                for (unsigned int i = 0; i < repeat && pos < total; ++i) all_lengths[pos++] = 0;
            } else {
                throw "PNG deflate: invalid code length symbol";
            }
        }

        deflate_build_huffman(lit_h, all_lengths, hlit);
        deflate_build_huffman(dist_h, all_lengths + hlit, hdist);
    }

    /// Inflate from reader into output buffer. Returns bytes written.
    template<typename Reader>
    consteval std::size_t deflate_inflate(Reader& r, unsigned char* dst, std::size_t dst_cap) {
        std::size_t out_pos = 0;

        bool final_block = false;
        while (!final_block) {
            final_block = deflate_read_bits(r, 1) != 0;
            auto btype = deflate_read_bits(r, 2);

            if (btype == 0) {
                // Stored block: align to byte boundary
                r.align_to_byte();
                unsigned int lo = r.next_byte();
                unsigned int hi = r.next_byte();
                unsigned int len = lo | (hi << 8);
                r.next_byte(); // nlen lo
                r.next_byte(); // nlen hi
                for (unsigned int i = 0; i < len; ++i) {
                    if (out_pos >= dst_cap) throw "PNG deflate: output buffer overflow";
                    dst[out_pos++] = r.next_byte();
                }
            } else if (btype == 1 || btype == 2) {
                deflate_huff lit_h{}, dist_h{};
                if (btype == 1) {
                    deflate_build_fixed_lit(lit_h);
                    deflate_build_fixed_dist(dist_h);
                } else {
                    deflate_decode_dynamic(r, lit_h, dist_h);
                }

                while (true) {
                    auto sym = deflate_decode_symbol(r, lit_h);
                    if (sym == 256) break;
                    if (sym < 256) {
                        if (out_pos >= dst_cap) throw "PNG deflate: output buffer overflow";
                        dst[out_pos++] = static_cast<unsigned char>(sym);
                    } else {
                        auto extra = deflate_length_extra(sym);
                        auto length = deflate_length_base(sym) + deflate_read_bits(r, extra);
                        auto dist_sym = deflate_decode_symbol(r, dist_h);
                        auto dextra = deflate_dist_extra(dist_sym);
                        auto distance = deflate_dist_base(dist_sym) + deflate_read_bits(r, dextra);
                        if (distance > out_pos) throw "PNG deflate: back reference before start";
                        for (unsigned int i = 0; i < length; ++i) {
                            if (out_pos >= dst_cap) throw "PNG deflate: output buffer overflow";
                            dst[out_pos] = dst[out_pos - distance];
                            ++out_pos;
                        }
                    }
                }
            } else {
                throw "PNG deflate: invalid block type 3";
            }
        }
        return out_pos;
    }

    // ---------------------------------------------------------------
    // PNG unfilter — filter type hoisted outside inner loop
    // ---------------------------------------------------------------

    consteval unsigned char paeth_predict(unsigned int a, unsigned int b, unsigned int c) {
        int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
        int pa = p > static_cast<int>(a) ? p - static_cast<int>(a) : static_cast<int>(a) - p;
        int pb = p > static_cast<int>(b) ? p - static_cast<int>(b) : static_cast<int>(b) - p;
        int pc = p > static_cast<int>(c) ? p - static_cast<int>(c) : static_cast<int>(c) - p;
        if (pa <= pb && pa <= pc) return static_cast<unsigned char>(a);
        if (pb <= pc) return static_cast<unsigned char>(b);
        return static_cast<unsigned char>(c);
    }

    consteval void png_unfilter(unsigned char* raw, unsigned int width, unsigned int height, unsigned int bpp_bytes) {
        auto stride = width * bpp_bytes;
        for (unsigned int y = 0; y < height; ++y) {
            auto row_start = y * (1 + stride);
            auto filter = raw[row_start];
            auto* row = raw + row_start + 1;
            const unsigned char* prev = (y > 0) ? (raw + (y - 1) * (1 + stride) + 1) : nullptr;

            switch (filter) {
                case 0: // None
                    break;
                case 1: // Sub
                    for (unsigned int x = bpp_bytes; x < stride; ++x)
                        row[x] = static_cast<unsigned char>(row[x] + row[x - bpp_bytes]);
                    break;
                case 2: // Up
                    if (prev) {
                        for (unsigned int x = 0; x < stride; ++x)
                            row[x] = static_cast<unsigned char>(row[x] + prev[x]);
                    }
                    break;
                case 3: // Average
                    for (unsigned int x = 0; x < stride; ++x) {
                        unsigned int a = (x >= bpp_bytes) ? row[x - bpp_bytes] : 0;
                        unsigned int b = prev ? prev[x] : 0;
                        row[x] = static_cast<unsigned char>(row[x] + ((a + b) >> 1));
                    }
                    break;
                case 4: // Paeth
                    for (unsigned int x = 0; x < stride; ++x) {
                        unsigned int a = (x >= bpp_bytes) ? row[x - bpp_bytes] : 0;
                        unsigned int b = prev ? prev[x] : 0;
                        unsigned int c = (prev && x >= bpp_bytes) ? prev[x - bpp_bytes] : 0;
                        row[x] = static_cast<unsigned char>(row[x] + paeth_predict(a, b, c));
                    }
                    break;
                default:
                    throw "PNG: unsupported filter type";
            }
        }
    }

    // ---------------------------------------------------------------
    // Pixel conversion — color_type dispatched outside loops
    // ---------------------------------------------------------------

    consteval void png_convert_rgb(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                   gba::color* pixels, bool* transparent,
                                   bool has_trns, unsigned char trns_r, unsigned char trns_g, unsigned char trns_b) {
        auto stride = width * 3;
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto off = x * 3;
                auto r = row[off];
                auto g = row[off + 1];
                auto b = row[off + 2];
                auto di = y * width + x;
                pixels[di] = {.red = static_cast<unsigned short>(r >> 3),
                              .green = static_cast<unsigned short>(g >> 3),
                              .blue = static_cast<unsigned short>(b >> 3),
                              .grn_lo = static_cast<unsigned short>((g >> 2) & 1)};
                transparent[di] = has_trns && r == trns_r && g == trns_g && b == trns_b;
            }
        }
    }

    consteval void png_convert_rgba(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                    gba::color* pixels, bool* transparent) {
        auto stride = width * 4;
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto off = x * 4;
                auto r = row[off];
                auto g = row[off + 1];
                auto b = row[off + 2];
                auto di = y * width + x;
                pixels[di] = {.red = static_cast<unsigned short>(r >> 3),
                              .green = static_cast<unsigned short>(g >> 3),
                              .blue = static_cast<unsigned short>(b >> 3),
                              .grn_lo = static_cast<unsigned short>((g >> 2) & 1)};
                transparent[di] = (row[off + 3] < 128);
            }
        }
    }

    template<std::size_t Size>
    consteval void png_convert_indexed(const unsigned char* scanlines, unsigned int width, unsigned int height,
                                       const std::array<unsigned char, Size>& data, const png_chunk_info& info,
                                       gba::color* pixels, bool* transparent) {
        if (info.plte_count == 0) throw "PNG: indexed image requires PLTE chunk";

        // Build PLTE lookup (only the entries we need)
        gba::color plte[256]{};
        for (unsigned int i = 0; i < info.plte_count && i < 256; ++i) {
            auto off = info.plte_offset + i * 3;
            plte[i] = {.red = static_cast<unsigned short>(data[off] >> 3),
                       .green = static_cast<unsigned short>(data[off + 1] >> 3),
                       .blue = static_cast<unsigned short>(data[off + 2] >> 3),
                       .grn_lo = static_cast<unsigned short>((data[off + 1] >> 2) & 1)};
        }

        // Build tRNS lookup
        bool plte_trans[256]{};
        if (info.trns_len > 0) {
            for (unsigned int i = 0; i < info.trns_len && i < 256; ++i)
                plte_trans[i] = (data[info.trns_offset + i] < 128);
        }

        auto stride = width; // 1 byte per pixel for indexed
        for (unsigned int y = 0; y < height; ++y) {
            auto* row = scanlines + y * (1 + stride) + 1;
            for (unsigned int x = 0; x < width; ++x) {
                auto idx = row[x];
                auto di = y * width + x;
                if (idx < info.plte_count) {
                    pixels[di] = plte[idx];
                    transparent[di] = plte_trans[idx];
                } else {
                    pixels[di] = {};
                    transparent[di] = true;
                }
            }
        }
    }

    // ---------------------------------------------------------------
    // Full PNG decode — single-pass chunks, streaming inflate
    // ---------------------------------------------------------------

    template<std::size_t Size>
    consteval void png_decode_all(const std::array<unsigned char, Size>& data, unsigned int width, unsigned int height,
                                  unsigned int color_type, gba::color* pixels, bool* transparent) {
        auto bpp_bytes = (color_type == 2) ? 3u : (color_type == 6) ? 4u : 1u;
        auto stride = width * bpp_bytes;
        auto raw_size = height * (1 + stride);

        // Single-pass chunk scan
        auto info = png_scan_chunks(data);

        // Streaming inflate directly from IDAT spans (no intermediate buffer)
        png_idat_reader<Size> reader{data, info};
        reader.skip_zlib_header();

        unsigned char scanlines[raw_size];
        auto inflated = deflate_inflate(reader, scanlines, raw_size);
        if (inflated != raw_size) throw "PNG: inflated size does not match expected scanline data";

        // Unfilter (filter type hoisted outside inner loop)
        png_unfilter(scanlines, width, height, bpp_bytes);

        // Convert to pixels (color type dispatched outside loops)
        if (color_type == 2) {
            bool has_trns = false;
            unsigned char trns_r = 0, trns_g = 0, trns_b = 0;
            if (info.trns_len >= 6) {
                trns_r = data[info.trns_offset + 1];
                trns_g = data[info.trns_offset + 3];
                trns_b = data[info.trns_offset + 5];
                has_trns = true;
            }
            png_convert_rgb(scanlines, width, height, pixels, transparent, has_trns, trns_r, trns_g, trns_b);
        } else if (color_type == 6) {
            png_convert_rgba(scanlines, width, height, pixels, transparent);
        } else {
            png_convert_indexed(scanlines, width, height, data, info, pixels, transparent);
        }
    }

} // namespace gba::embed::bits
