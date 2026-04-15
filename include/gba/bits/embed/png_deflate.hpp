/// @file bits/embed/png_deflate.hpp
/// @brief Compile-time DEFLATE decompressor for PNG IDAT streams.
///
/// Supports stored blocks (type 0), fixed Huffman (type 1),
/// and dynamic Huffman (type 2). Uses a counting-sort Huffman build
/// for O(n) table construction instead of O(n × max_bits).
#pragma once

#include <gba/bits/embed/png_chunks.hpp>

namespace gba::embed::bits {


    struct deflate_huff {
        unsigned int counts[16]{};
        unsigned int first_code[16]{};
        unsigned int first_index[16]{};
        unsigned int symbols[320]{};
        unsigned int max_bits = 0;
    };

    consteval void deflate_build_huffman(deflate_huff& h, const unsigned int* lengths, unsigned int count) {
        for (unsigned int i = 0; i < 16; ++i) h.counts[i] = 0;
        for (unsigned int i = 0; i < count; ++i) {
            if (lengths[i] <= 15) ++h.counts[lengths[i]];
        }
        h.counts[0] = 0;

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

    /// @brief Inflate from reader into output buffer. Returns bytes written.
    template<typename Reader>
    consteval std::size_t deflate_inflate(Reader& r, unsigned char* dst, std::size_t dst_cap) {
        std::size_t out_pos = 0;

        bool final_block = false;
        while (!final_block) {
            final_block = deflate_read_bits(r, 1) != 0;
            auto btype = deflate_read_bits(r, 2);

            if (btype == 0) {
                r.align_to_byte();
                unsigned int lo = r.next_byte();
                unsigned int hi = r.next_byte();
                unsigned int len = lo | (hi << 8);
                unsigned int nlen_lo = r.next_byte();
                unsigned int nlen_hi = r.next_byte();
                unsigned int nlen = nlen_lo | (nlen_hi << 8);
                if ((len ^ 0xFFFFu) != nlen) throw "PNG deflate: stored block LEN/NLEN mismatch";
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

} // namespace gba::embed::bits
