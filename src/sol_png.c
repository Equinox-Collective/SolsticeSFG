#include "sol_internal.h"

/* ============================================================================
 * CRC32 & Adler32 Helpers
 * ============================================================================ */

SOL_PRIVATE uint32_t sol__crc32(const uint8_t *data, size_t len, uint32_t init) {
    uint32_t crc = ~init;
    size_t i;

    for (i = 0u; i < len; ++i) {
        int bit;
        crc ^= (uint32_t)data[i];
        for (bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

SOL_PRIVATE uint32_t sol__png_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

SOL_PRIVATE void sol__png_wbe32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

/* ============================================================================
 * RFC 1951 DEFLATE / INFLATE Decoder
 * ============================================================================ */

typedef struct sol__bs {
    const uint8_t *data;
    size_t size;
    size_t pos;
    uint32_t buf;
    int cnt;
} sol__bs_t;

typedef struct sol__huff {
    uint16_t counts[16];
    uint16_t symbols[288];
    int max_sym;
} sol__huff_t;

SOL_PRIVATE bool sol__bs_bits(sol__bs_t *bs, int n, uint32_t *out) {
    while (bs->cnt < n) {
        if (bs->pos >= bs->size) {
            return false;
        }
        bs->buf |= ((uint32_t)bs->data[bs->pos++]) << bs->cnt;
        bs->cnt += 8;
    }
    *out = bs->buf & ((1u << n) - 1u);
    bs->buf >>= n;
    bs->cnt -= n;
    return true;
}

SOL_PRIVATE void sol__bs_align(sol__bs_t *bs) {
    int skip = bs->cnt & 7;
    bs->buf >>= skip;
    bs->cnt -= skip;
}

SOL_PRIVATE bool sol__huff_build(sol__huff_t *h, const uint8_t *lengths, int num_syms) {
    uint16_t offsets[16];
    int left = 1;
    int len;
    int sym;

    SOL_MEMSET(h->counts, 0, sizeof(h->counts));
    h->max_sym = num_syms;

    for (sym = 0; sym < num_syms; ++sym) {
        if (lengths[sym] > 15u) {
            return false;
        }
        h->counts[lengths[sym]]++;
    }
    if (h->counts[0] == (uint16_t)num_syms) {
        return true;
    }

    for (len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= (int)h->counts[len];
        if (left < 0) {
            return false;
        }
    }

    offsets[1] = 0u;
    for (len = 1; len < 15; ++len) {
        offsets[len + 1] = (uint16_t)(offsets[len] + h->counts[len]);
    }

    for (sym = 0; sym < num_syms; ++sym) {
        if (lengths[sym] != 0u) {
            h->symbols[offsets[lengths[sym]]++] = (uint16_t)sym;
        }
    }
    return true;
}

SOL_PRIVATE bool sol__huff_dec(sol__bs_t *bs, const sol__huff_t *h, int *out_sym) {
    int code = 0;
    int first = 0;
    int index = 0;
    int len;

    for (len = 1; len <= 15; ++len) {
        uint32_t bit = 0u;
        int count;
        if (!sol__bs_bits(bs, 1, &bit)) {
            return false;
        }
        code |= (int)bit;
        count = (int)h->counts[len];
        if (code - count < first) {
            int idx = index + (code - first);
            if (idx < 0 || idx >= h->max_sym) {
                return false;
            }
            *out_sym = (int)h->symbols[idx];
            return true;
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return false;
}

SOL_PRIVATE bool sol__inflate_codes(sol__bs_t *bs, const sol__huff_t *lit_h,
                                    const sol__huff_t *dist_h, uint8_t *out, size_t out_cap,
                                    size_t *out_pos) {
    static const uint16_t k_len_base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                            15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                            67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t k_len_ext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                          2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t k_dist_base[30] = {
        1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
        193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t k_dist_ext[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                           6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    for (;;) {
        int sym = 0;
        if (!sol__huff_dec(bs, lit_h, &sym)) {
            return false;
        }
        if (sym < 256) {
            if (*out_pos >= out_cap) {
                return false;
            }
            out[(*out_pos)++] = (uint8_t)sym;
        } else if (sym == 256) {
            return true;
        } else {
            int l_idx = sym - 257;
            uint32_t l_ext = 0u;
            uint32_t d_ext = 0u;
            int d_sym = 0;
            size_t len;
            size_t dist;
            size_t k;

            if (l_idx >= 29) {
                return false;
            }
            if (k_len_ext[l_idx] > 0u && !sol__bs_bits(bs, (int)k_len_ext[l_idx], &l_ext)) {
                return false;
            }
            len = (size_t)k_len_base[l_idx] + (size_t)l_ext;

            if (!sol__huff_dec(bs, dist_h, &d_sym) || d_sym < 0 || d_sym >= 30) {
                return false;
            }
            if (k_dist_ext[d_sym] > 0u && !sol__bs_bits(bs, (int)k_dist_ext[d_sym], &d_ext)) {
                return false;
            }
            dist = (size_t)k_dist_base[d_sym] + (size_t)d_ext;

            if (dist == 0u || dist > *out_pos || *out_pos + len > out_cap) {
                return false;
            }
            for (k = 0u; k < len; ++k) {
                out[*out_pos] = out[*out_pos - dist];
                (*out_pos)++;
            }
        }
    }
}

SOL_PRIVATE bool sol__zlib_inflate(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap,
                                   size_t *out_written) {
    sol__bs_t bs;
    uint32_t bfinal = 0u;

    if (src == NULL || src_len < 6u || dst == NULL) {
        return false;
    }
    /* Verify zlib CMF/FLG check and CM=8 (DEFLATE) without FDICT */
    if (((((uint32_t)src[0] << 8) | src[1]) % 31u) != 0u || (src[0] & 0x0Fu) != 8u ||
        (src[1] & 0x20u) != 0u) {
        return false;
    }

    bs.data = src + 2u;
    bs.size = src_len - 2u;
    bs.pos = 0u;
    bs.buf = 0u;
    bs.cnt = 0;
    *out_written = 0u;

    do {
        uint32_t btype = 0u;
        if (!sol__bs_bits(&bs, 1, &bfinal) || !sol__bs_bits(&bs, 2, &btype)) {
            return false;
        }

        if (btype == 0u) {
            uint16_t len;
            uint16_t nlen;
            sol__bs_align(&bs);
            /* Rewind bit buffer into byte stream cleanly */
            bs.pos -= (size_t)(bs.cnt >> 3);
            bs.buf = 0u;
            bs.cnt = 0;

            if (bs.pos + 4u > bs.size) {
                return false;
            }
            len = (uint16_t)bs.data[bs.pos] | ((uint16_t)bs.data[bs.pos + 1u] << 8);
            nlen = (uint16_t)bs.data[bs.pos + 2u] | ((uint16_t)bs.data[bs.pos + 3u] << 8);
            bs.pos += 4u;

            if ((uint16_t)(~len) != nlen || bs.pos + len > bs.size ||
                *out_written + len > dst_cap) {
                return false;
            }
            SOL_MEMCPY(dst + *out_written, bs.data + bs.pos, len);
            bs.pos += len;
            *out_written += len;
        } else if (btype == 1u) {
            uint8_t lit_lens[288];
            uint8_t dist_lens[32];
            sol__huff_t lit_h;
            sol__huff_t dist_h;
            int i;

            for (i = 0; i <= 143; ++i) {
                lit_lens[i] = 8u;
            }
            for (i = 144; i <= 255; ++i) {
                lit_lens[i] = 9u;
            }
            for (i = 256; i <= 279; ++i) {
                lit_lens[i] = 7u;
            }
            for (i = 280; i <= 287; ++i) {
                lit_lens[i] = 8u;
            }
            for (i = 0; i < 32; ++i) {
                dist_lens[i] = 5u;
            }

            if (!sol__huff_build(&lit_h, lit_lens, 288) ||
                !sol__huff_build(&dist_h, dist_lens, 32)) {
                return false;
            }
            if (!sol__inflate_codes(&bs, &lit_h, &dist_h, dst, dst_cap, out_written)) {
                return false;
            }
        } else if (btype == 2u) {
            static const uint8_t k_cl_order[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                   11, 4,  12, 3, 13, 2, 14, 1, 15};
            uint32_t hlit = 0u;
            uint32_t hdist = 0u;
            uint32_t hclen = 0u;
            uint8_t cl_lens[19];
            uint8_t lens[288 + 32];
            sol__huff_t cl_h;
            sol__huff_t lit_h;
            sol__huff_t dist_h;
            uint32_t n_lit;
            uint32_t n_dist;
            uint32_t total_codes;
            uint32_t i;

            if (!sol__bs_bits(&bs, 5, &hlit) || !sol__bs_bits(&bs, 5, &hdist) ||
                !sol__bs_bits(&bs, 4, &hclen)) {
                return false;
            }
            n_lit = hlit + 257u;
            n_dist = hdist + 1u;
            hclen += 4u;
            if (n_lit > 288u || n_dist > 32u || hclen > 19u) {
                return false;
            }

            SOL_MEMSET(cl_lens, 0, sizeof(cl_lens));
            for (i = 0u; i < hclen; ++i) {
                uint32_t v = 0u;
                if (!sol__bs_bits(&bs, 3, &v)) {
                    return false;
                }
                cl_lens[k_cl_order[i]] = (uint8_t)v;
            }
            if (!sol__huff_build(&cl_h, cl_lens, 19)) {
                return false;
            }

            total_codes = n_lit + n_dist;
            i = 0u;
            SOL_MEMSET(lens, 0, sizeof(lens));
            while (i < total_codes) {
                int sym = 0;
                uint32_t rep = 0u;
                uint32_t ext = 0u;
                uint8_t fill = 0u;

                if (!sol__huff_dec(&bs, &cl_h, &sym)) {
                    return false;
                }
                if (sym < 16) {
                    lens[i++] = (uint8_t)sym;
                    continue;
                }
                if (sym == 16) {
                    if (i == 0u || !sol__bs_bits(&bs, 2, &ext)) {
                        return false;
                    }
                    fill = lens[i - 1u];
                    rep = 3u + ext;
                } else if (sym == 17) {
                    if (!sol__bs_bits(&bs, 3, &ext)) {
                        return false;
                    }
                    rep = 3u + ext;
                } else if (sym == 18) {
                    if (!sol__bs_bits(&bs, 7, &ext)) {
                        return false;
                    }
                    rep = 11u + ext;
                } else {
                    return false;
                }
                if (i + rep > total_codes) {
                    return false;
                }
                SOL_MEMSET(lens + i, fill, rep);
                i += rep;
            }

            if (!sol__huff_build(&lit_h, lens, (int)n_lit) ||
                !sol__huff_build(&dist_h, lens + n_lit, (int)n_dist)) {
                return false;
            }
            if (!sol__inflate_codes(&bs, &lit_h, &dist_h, dst, dst_cap, out_written)) {
                return false;
            }
        } else {
            return false;
        }
    } while (bfinal == 0u);

    return true;
}

/* ============================================================================
 * PNG Unfiltering & Pixel Conversion (including Adam7 Interlace)
 * ============================================================================ */

SOL_PRIVATE uint8_t sol__paeth(uint8_t a, uint8_t b, uint8_t c) {
    int p = (int)a + (int)b - (int)c;
    int pa = (p > (int)a) ? (p - (int)a) : ((int)a - p);
    int pb = (p > (int)b) ? (p - (int)b) : ((int)b - p);
    int pc = (p > (int)c) ? (p - (int)c) : ((int)c - p);

    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

SOL_PRIVATE uint32_t sol__png_sample_pixel(const uint8_t *row, int32_t x, uint8_t color_type,
                                           uint8_t bit_depth, const uint8_t *plte, size_t plte_cnt,
                                           const uint8_t *trns, size_t trns_len) {
    if (bit_depth < 8u) {
        uint32_t per_byte = 8u / (uint32_t)bit_depth;
        uint32_t shift = (per_byte - 1u - ((uint32_t)x % per_byte)) * (uint32_t)bit_depth;
        uint32_t mask = (1u << bit_depth) - 1u;
        uint32_t idx = (row[(size_t)x / per_byte] >> shift) & mask;

        if (color_type == 3u) {
            if ((size_t)idx >= plte_cnt) {
                return SOL_COLOR_BLACK;
            }
            uint8_t a = ((size_t)idx < trns_len) ? trns[idx] : 255u;
            return SOL_ARGB(a, plte[idx * 3u], plte[idx * 3u + 1u], plte[idx * 3u + 2u]);
        }
        {
            uint8_t g = (uint8_t)((idx * 255u) / mask);
            return SOL_RGB(g, g, g);
        }
    }

    if (bit_depth == 16u) {
        if (color_type == 0u) {
            uint8_t g = row[x * 2];
            return SOL_RGB(g, g, g);
        }
        if (color_type == 2u) {
            return SOL_RGB(row[x * 6], row[x * 6 + 2], row[x * 6 + 4]);
        }
        if (color_type == 4u) {
            uint8_t g = row[x * 4];
            return SOL_ARGB(row[x * 4 + 2], g, g, g);
        }
        return SOL_ARGB(row[x * 8 + 6], row[x * 8], row[x * 8 + 2], row[x * 8 + 4]);
    }

    /* bit_depth == 8 */
    switch (color_type) {
    case 0u: {
        uint8_t g = row[x];
        uint8_t a = (trns_len >= 2u && g == trns[1]) ? 0u : 255u;
        return SOL_ARGB(a, g, g, g);
    }
    case 2u: {
        uint8_t r = row[x * 3];
        uint8_t g = row[x * 3 + 1];
        uint8_t b = row[x * 3 + 2];
        uint8_t a = (trns_len >= 6u && r == trns[1] && g == trns[3] && b == trns[5]) ? 0u : 255u;
        return SOL_ARGB(a, r, g, b);
    }
    case 3u: {
        uint8_t idx = row[x];
        if ((size_t)idx >= plte_cnt) {
            return SOL_COLOR_BLACK;
        }
        uint8_t a = ((size_t)idx < trns_len) ? trns[idx] : 255u;
        return SOL_ARGB(a, plte[idx * 3u], plte[idx * 3u + 1u], plte[idx * 3u + 2u]);
    }
    case 4u: {
        uint8_t g = row[x * 2];
        return SOL_ARGB(row[x * 2 + 1], g, g, g);
    }
    case 6u:
        return SOL_ARGB(row[x * 4 + 3], row[x * 4], row[x * 4 + 1], row[x * 4 + 2]);
    default:
        return SOL_COLOR_BLACK;
    }
}

sol_status_t sol_image_load_png_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                    sol_arena_t *arena) {
    static const uint8_t k_sig[8] = {0x89u, 'P', 'N', 'G', 0x0Du, 0x0Au, 0x1Au, 0x0Au};
    size_t entry_mark;
    size_t scratch_mark;
    size_t pos = 8u;
    int32_t w = 0;
    int32_t h = 0;
    uint8_t bit_depth = 0u;
    uint8_t color_type = 0u;
    uint8_t interlace = 0u;
    bool has_ihdr = false;
    bool has_iend = false;
    const uint8_t *plte = NULL;
    size_t plte_cnt = 0u;
    const uint8_t *trns = NULL;
    size_t trns_len = 0u;
    size_t idat_total = 0u;
    size_t idat_chunks = 0u;
    const uint8_t *single_idat = NULL;
    uint8_t *idat_buf = NULL;
    uint8_t *raw_buf = NULL;
    uint32_t *px;
    uint8_t ch_count;
    uint8_t filter_bpp;
    size_t raw_cap = 0u;

    if (img == NULL || data == NULL || size < 33u || SOL_MEMCMP(data, k_sig, 8u) != 0) {
        return SOL_ERR_INVALID_ARG;
    }
    SOL_MEMSET(img, 0, sizeof(*img));
    entry_mark = (arena != NULL) ? sol_arena_mark(arena) : 0u;

    /* Pass 1: validate chunks and gather metadata */
    while (pos + 12u <= size) {
        uint32_t len = sol__png_be32(data + pos);
        const uint8_t *ctype = data + pos + 4u;
        const uint8_t *cdata = data + pos + 8u;
        uint32_t file_crc;
        uint32_t calc_crc;

        if ((size_t)len > size - pos - 12u) {
            return SOL_ERR_INVALID_ARG;
        }
        file_crc = sol__png_be32(cdata + len);
        calc_crc = sol__crc32(ctype, (size_t)len + 4u, 0u);
        if (file_crc != calc_crc) {
            return SOL_ERR_INVALID_ARG;
        }

        if (SOL_MEMCMP(ctype, "IHDR", 4u) == 0) {
            if (has_ihdr || len != 13u) {
                return SOL_ERR_INVALID_ARG;
            }
            w = (int32_t)sol__png_be32(cdata);
            h = (int32_t)sol__png_be32(cdata + 4);
            bit_depth = cdata[8];
            color_type = cdata[9];
            interlace = cdata[12];
            if (w <= 0 || h <= 0 || w > SOL_IMG_MAX_DIM || h > SOL_IMG_MAX_DIM || cdata[10] != 0u ||
                cdata[11] != 0u || interlace > 1u) {
                return SOL_ERR_INVALID_ARG;
            }
            has_ihdr = true;
        } else if (SOL_MEMCMP(ctype, "PLTE", 4u) == 0) {
            if (!has_ihdr || (len % 3u) != 0u || len == 0u || len > 768u) {
                return SOL_ERR_INVALID_ARG;
            }
            plte = cdata;
            plte_cnt = (size_t)len / 3u;
        } else if (SOL_MEMCMP(ctype, "tRNS", 4u) == 0) {
            trns = cdata;
            trns_len = (size_t)len;
        } else if (SOL_MEMCMP(ctype, "IDAT", 4u) == 0) {
            if (!has_ihdr) {
                return SOL_ERR_INVALID_ARG;
            }
            if (idat_chunks == 0u) {
                single_idat = cdata;
            }
            idat_total += (size_t)len;
            if (idat_total > 64u * 1024u * 1024u) {
                return SOL_ERR_OUT_OF_BOUNDS;
            }
            idat_chunks++;
        } else if (SOL_MEMCMP(ctype, "IEND", 4u) == 0) {
            has_iend = true;
            break;
        }

        pos += 12u + (size_t)len;
    }

    if (!has_ihdr || !has_iend || idat_total < 6u) {
        return SOL_ERR_INVALID_ARG;
    }

    /* Validate color_type & bit_depth combinations per PNG spec */
    switch (color_type) {
    case 0u:
        if (bit_depth != 1u && bit_depth != 2u && bit_depth != 4u && bit_depth != 8u &&
            bit_depth != 16u) {
            return SOL_ERR_INVALID_ARG;
        }
        ch_count = 1u;
        break;
    case 2u:
        if (bit_depth != 8u && bit_depth != 16u) {
            return SOL_ERR_INVALID_ARG;
        }
        ch_count = 3u;
        break;
    case 3u:
        if (plte == NULL ||
            (bit_depth != 1u && bit_depth != 2u && bit_depth != 4u && bit_depth != 8u)) {
            return SOL_ERR_INVALID_ARG;
        }
        ch_count = 1u;
        break;
    case 4u:
        if (bit_depth != 8u && bit_depth != 16u) {
            return SOL_ERR_INVALID_ARG;
        }
        ch_count = 2u;
        break;
    case 6u:
        if (bit_depth != 8u && bit_depth != 16u) {
            return SOL_ERR_INVALID_ARG;
        }
        ch_count = 4u;
        break;
    default:
        return SOL_ERR_INVALID_ARG;
    }

    filter_bpp = (uint8_t)SOL_MAX(1u, ((uint32_t)ch_count * (uint32_t)bit_depth) / 8u);

    /* Allocate permanent output pixels first, then temporary scratch right after */
    px = sol__img_alloc_pixels(img, w, h, arena);
    if (px == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    scratch_mark = (arena != NULL) ? sol_arena_mark(arena) : 0u;

    if (interlace == 0u) {
        size_t bits_per_row = (size_t)w * (size_t)ch_count * (size_t)bit_depth;
        size_t row_bytes = (bits_per_row + 7u) / 8u;
        if (sol__mul_overflow_sz(row_bytes + 1u, (size_t)h, &raw_cap)) {
            goto fail;
        }
    } else {
        static const int32_t k_x0[7] = {0, 4, 0, 2, 0, 1, 0};
        static const int32_t k_dx[7] = {8, 8, 4, 4, 2, 2, 1};
        static const int32_t k_y0[7] = {0, 0, 4, 0, 2, 0, 1};
        static const int32_t k_dy[7] = {8, 8, 8, 4, 4, 2, 2};
        int p;
        for (p = 0; p < 7; ++p) {
            int32_t pw = (w > k_x0[p]) ? ((w - k_x0[p] + k_dx[p] - 1) / k_dx[p]) : 0;
            int32_t ph = (h > k_y0[p]) ? ((h - k_y0[p] + k_dy[p] - 1) / k_dy[p]) : 0;
            if (pw > 0 && ph > 0) {
                size_t rb = ((size_t)pw * (size_t)ch_count * (size_t)bit_depth + 7u) / 8u;
                size_t pass_sz;
                if (sol__mul_overflow_sz(rb + 1u, (size_t)ph, &pass_sz)) {
                    goto fail;
                }
                raw_cap += pass_sz;
            }
        }
    }

    raw_buf = (uint8_t *)sol__tmp_alloc(raw_cap, arena);
    if (raw_buf == NULL) {
        goto fail;
    }

    {
        const uint8_t *idat_src = single_idat;
        size_t inflated_len = 0u;

        if (idat_chunks > 1u) {
            size_t copy_pos = 0u;
            size_t scan = 8u;
            idat_buf = (uint8_t *)sol__tmp_alloc(idat_total, arena);
            if (idat_buf == NULL) {
                goto fail;
            }
            while (scan + 12u <= size) {
                uint32_t len = sol__png_be32(data + scan);
                const uint8_t *ctype = data + scan + 4u;
                const uint8_t *cdata = data + scan + 8u;
                if (SOL_MEMCMP(ctype, "IDAT", 4u) == 0) {
                    SOL_MEMCPY(idat_buf + copy_pos, cdata, len);
                    copy_pos += len;
                } else if (SOL_MEMCMP(ctype, "IEND", 4u) == 0) {
                    break;
                }
                scan += 12u + (size_t)len;
            }
            idat_src = idat_buf;
        }

        if (!sol__zlib_inflate(idat_src, idat_total, raw_buf, raw_cap, &inflated_len) ||
            inflated_len != raw_cap) {
            goto fail;
        }
    }

    if (interlace == 0u) {
        size_t row_bytes = ((size_t)w * (size_t)ch_count * (size_t)bit_depth + 7u) / 8u;
        size_t u_pos = 0u;
        size_t r_pos = 0u;
        int32_t y;

        /* Unfilter using byte stride per scanline */
        for (y = 0; y < h; ++y) {
            uint8_t ftype = raw_buf[u_pos++];
            uint8_t *row = raw_buf + u_pos;
            const uint8_t *prev = (y > 0) ? (row - 1u - row_bytes) : NULL;
            size_t i;

            switch (ftype) {
            case 0u:
                break;
            case 1u:
                for (i = filter_bpp; i < row_bytes; ++i) {
                    row[i] = (uint8_t)(row[i] + row[i - filter_bpp]);
                }
                break;
            case 2u:
                if (prev != NULL) {
                    for (i = 0u; i < row_bytes; ++i) {
                        row[i] = (uint8_t)(row[i] + prev[i]);
                    }
                }
                break;
            case 3u:
                for (i = 0u; i < row_bytes; ++i) {
                    uint32_t a = (i >= filter_bpp) ? row[i - filter_bpp] : 0u;
                    uint32_t b = (prev != NULL) ? prev[i] : 0u;
                    row[i] = (uint8_t)(row[i] + ((a + b) >> 1));
                }
                break;
            case 4u:
                for (i = 0u; i < row_bytes; ++i) {
                    uint8_t a = (i >= filter_bpp) ? row[i - filter_bpp] : 0u;
                    uint8_t b = (prev != NULL) ? prev[i] : 0u;
                    uint8_t c = (prev != NULL && i >= filter_bpp) ? prev[i - filter_bpp] : 0u;
                    row[i] = (uint8_t)(row[i] + sol__paeth(a, b, c));
                }
                break;
            default:
                goto fail;
            }
            u_pos += row_bytes;
        }

        for (y = 0; y < h; ++y) {
            const uint8_t *row = raw_buf + r_pos + 1u;
            int32_t x;
            for (x = 0; x < w; ++x) {
                px[(size_t)y * (size_t)w + (size_t)x] = sol__png_sample_pixel(
                    row, x, color_type, bit_depth, plte, plte_cnt, trns, trns_len);
            }
            r_pos += 1u + row_bytes;
        }
    } else {
        static const int32_t k_x0[7] = {0, 4, 0, 2, 0, 1, 0};
        static const int32_t k_dx[7] = {8, 8, 4, 4, 2, 2, 1};
        static const int32_t k_y0[7] = {0, 0, 4, 0, 2, 0, 1};
        static const int32_t k_dy[7] = {8, 8, 8, 4, 4, 2, 2};
        size_t u_pos = 0u;
        int p;

        for (p = 0; p < 7; ++p) {
            int32_t pw = (w > k_x0[p]) ? ((w - k_x0[p] + k_dx[p] - 1) / k_dx[p]) : 0;
            int32_t ph = (h > k_y0[p]) ? ((h - k_y0[p] + k_dy[p] - 1) / k_dy[p]) : 0;
            if (pw > 0 && ph > 0) {
                size_t pass_rb = ((size_t)pw * (size_t)ch_count * (size_t)bit_depth + 7u) / 8u;
                size_t pass_start = u_pos;
                size_t bpp_pass = filter_bpp;
                int32_t py;

                if (bit_depth < 8u) {
                    /* Sub-byte filter stride is 1 byte */
                    bpp_pass = 1u;
                }
                for (py = 0; py < ph; ++py) {
                    uint8_t ftype = raw_buf[u_pos++];
                    uint8_t *row = raw_buf + u_pos;
                    const uint8_t *prev = (py > 0) ? (row - 1u - pass_rb) : NULL;
                    size_t i;

                    switch (ftype) {
                    case 0u:
                        break;
                    case 1u:
                        for (i = bpp_pass; i < pass_rb; ++i) {
                            row[i] = (uint8_t)(row[i] + row[i - bpp_pass]);
                        }
                        break;
                    case 2u:
                        if (prev != NULL) {
                            for (i = 0u; i < pass_rb; ++i) {
                                row[i] = (uint8_t)(row[i] + prev[i]);
                            }
                        }
                        break;
                    case 3u:
                        for (i = 0u; i < pass_rb; ++i) {
                            uint32_t a = (i >= bpp_pass) ? row[i - bpp_pass] : 0u;
                            uint32_t b = (prev != NULL) ? prev[i] : 0u;
                            row[i] = (uint8_t)(row[i] + ((a + b) >> 1));
                        }
                        break;
                    case 4u:
                        for (i = 0u; i < pass_rb; ++i) {
                            uint8_t a = (i >= bpp_pass) ? row[i - bpp_pass] : 0u;
                            uint8_t b = (prev != NULL) ? prev[i] : 0u;
                            uint8_t c = (prev != NULL && i >= bpp_pass) ? prev[i - bpp_pass] : 0u;
                            row[i] = (uint8_t)(row[i] + sol__paeth(a, b, c));
                        }
                        break;
                    default:
                        goto fail;
                    }
                    u_pos += pass_rb;
                }

                for (py = 0; py < ph; ++py) {
                    const uint8_t *row = raw_buf + pass_start + (size_t)py * (1u + pass_rb) + 1u;
                    int32_t dy = k_y0[p] + py * k_dy[p];
                    int32_t px_i;
                    for (px_i = 0; px_i < pw; ++px_i) {
                        int32_t dx = k_x0[p] + px_i * k_dx[p];
                        px[(size_t)dy * (size_t)w + (size_t)dx] = sol__png_sample_pixel(
                            row, px_i, color_type, bit_depth, plte, plte_cnt, trns, trns_len);
                    }
                }
            }
        }
    }

    if (arena != NULL) {
        sol_arena_rewind(arena, scratch_mark);
    } else {
        sol__tmp_free(idat_buf, NULL);
        sol__tmp_free(raw_buf, NULL);
    }
    return SOL_OK;

fail:
    if (arena != NULL) {
        sol_arena_rewind(arena, entry_mark);
        SOL_MEMSET(img, 0, sizeof(*img));
    } else {
        sol__tmp_free(idat_buf, NULL);
        sol__tmp_free(raw_buf, NULL);
        sol_image_destroy(img);
    }
    return SOL_ERR_INVALID_ARG;
}

/* ============================================================================
 * Stored-DEFLATE PNG Encoder (100% Spec-Compliant RGBA8888, Zero Temp Heap)
 * ============================================================================ */

SOL_PRIVATE void sol__png_write_chunk(uint8_t *dst, size_t *pos, const char tag[4],
                                      const uint8_t *payload, size_t len) {
    sol__png_wbe32(dst + *pos, (uint32_t)len);
    dst[*pos + 4u] = (uint8_t)tag[0];
    dst[*pos + 5u] = (uint8_t)tag[1];
    dst[*pos + 6u] = (uint8_t)tag[2];
    dst[*pos + 7u] = (uint8_t)tag[3];
    if (payload != NULL && len > 0u) {
        SOL_MEMCPY(dst + *pos + 8u, payload, len);
    }
    {
        uint32_t crc = sol__crc32(dst + *pos + 4u, len + 4u, 0u);
        sol__png_wbe32(dst + *pos + 8u + len, crc);
    }
    *pos += 12u + len;
}

sol_status_t sol_image_encode_png(const sol_surface_t *surf, uint8_t *out_buf, size_t capacity,
                                  size_t *out_written) {
    size_t row_bytes;
    size_t raw_size;
    size_t n_blocks;
    size_t idat_payload_len;
    size_t total;
    size_t pos = 0u;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }
    if (sol__mul_overflow_sz((size_t)surf->width, 4u, &row_bytes) ||
        sol__mul_overflow_sz(row_bytes + 1u, (size_t)surf->height, &raw_size)) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    n_blocks = (raw_size + 65534u) / 65535u;
    if (n_blocks == 0u) {
        n_blocks = 1u;
    }
    idat_payload_len = 2u + n_blocks * 5u + raw_size + 4u;
    /* 8 sig + 25 IHDR + (12 + idat_payload_len) IDAT + 12 IEND */
    total = 8u + 25u + 12u + idat_payload_len + 12u;

    if (out_written != NULL) {
        *out_written = total;
    }
    if (out_buf == NULL) {
        return SOL_OK;
    }
    if (capacity < total) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    {
        static const uint8_t k_sig[8] = {0x89u, 'P', 'N', 'G', 0x0Du, 0x0Au, 0x1Au, 0x0Au};
        SOL_MEMCPY(out_buf, k_sig, 8u);
        pos = 8u;
    }

    /* IHDR */
    {
        uint8_t ihdr[13];
        sol__png_wbe32(ihdr, (uint32_t)surf->width);
        sol__png_wbe32(ihdr + 4, (uint32_t)surf->height);
        ihdr[8] = 8u;
        ihdr[9] = 6u; /* RGBA */
        ihdr[10] = 0u;
        ihdr[11] = 0u;
        ihdr[12] = 0u;
        sol__png_write_chunk(out_buf, &pos, "IHDR", ihdr, 13u);
    }

    /* IDAT: write header + stored DEFLATE stream directly in-place */
    {
        size_t idat_start = pos;
        size_t stream_pos;
        size_t raw_emitted = 0u;
        size_t block_rem = 0u;
        int32_t y;

        sol__png_wbe32(out_buf + pos, (uint32_t)idat_payload_len);
        out_buf[pos + 4u] = 'I';
        out_buf[pos + 5u] = 'D';
        out_buf[pos + 6u] = 'A';
        out_buf[pos + 7u] = 'T';
        stream_pos = pos + 8u;

        /* zlib header */
        out_buf[stream_pos++] = 0x78u;
        out_buf[stream_pos++] = 0x01u;

        {
            uint32_t s1 = 1u;
            uint32_t s2 = 0u;

            for (y = 0; y < surf->height; ++y) {
                const uint32_t *s_row = surf->pixels + (size_t)y * (size_t)surf->stride;
                size_t byte_idx = 0u;
                size_t scan_len = row_bytes + 1u;

                while (byte_idx < scan_len) {
                    uint8_t val;
                    if (block_rem == 0u) {
                        size_t rem_all = raw_size - raw_emitted;
                        uint16_t blk_len = (rem_all > 65535u) ? 65535u : (uint16_t)rem_all;
                        uint8_t bfinal = (rem_all <= 65535u) ? 1u : 0u;
                        out_buf[stream_pos++] = bfinal;
                        out_buf[stream_pos++] = (uint8_t)(blk_len & 0xFFu);
                        out_buf[stream_pos++] = (uint8_t)((blk_len >> 8) & 0xFFu);
                        out_buf[stream_pos++] = (uint8_t)((~blk_len) & 0xFFu);
                        out_buf[stream_pos++] = (uint8_t)(((~blk_len) >> 8) & 0xFFu);
                        block_rem = blk_len;
                    }

                    if (byte_idx == 0u) {
                        val = 0u; /* Filter type 0 (None) */
                    } else {
                        size_t px_i = (byte_idx - 1u) >> 2;
                        size_t ch_i = (byte_idx - 1u) & 3u;
                        uint32_t c = s_row[px_i];
                        val = (ch_i == 0u)   ? SOL_COLOR_R(c)
                              : (ch_i == 1u) ? SOL_COLOR_G(c)
                              : (ch_i == 2u) ? SOL_COLOR_B(c)
                                             : SOL_COLOR_A(c);
                    }

                    out_buf[stream_pos++] = val;
                    s1 = (s1 + val) % 65521u;
                    s2 = (s2 + s1) % 65521u;
                    byte_idx++;
                    raw_emitted++;
                    block_rem--;
                }
            }

            sol__png_wbe32(out_buf + stream_pos, (s2 << 16) | s1);
            stream_pos += 4u;
        }

        {
            uint32_t crc = sol__crc32(out_buf + idat_start + 4u, idat_payload_len + 4u, 0u);
            sol__png_wbe32(out_buf + stream_pos, crc);
            pos = stream_pos + 4u;
        }
    }

    /* IEND */
    sol__png_write_chunk(out_buf, &pos, "IEND", NULL, 0u);
    return SOL_OK;
}
