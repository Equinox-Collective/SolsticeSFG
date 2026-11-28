#include "sol_internal.h"

#if !defined(SOL_FREESTANDING) && !defined(SOL_NO_STDIO)
    #include <stdio.h>
#endif

#if !defined(SOL_FREESTANDING) && !defined(SOL_NO_STDIO)
    #if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
        #define _CRT_SECURE_NO_WARNINGS
    #endif
    #include <stdio.h>
#endif

SOL_PRIVATE uint16_t sol__read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

SOL_PRIVATE uint32_t sol__read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

SOL_PRIVATE uint32_t sol__read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

SOL_PRIVATE void sol__write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

SOL_PRIVATE void sol__write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

SOL_PRIVATE void sol__write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

uint32_t *sol__img_alloc_pixels(sol_image_t *img, int32_t w, int32_t h, sol_arena_t *arena) {
    size_t count;
    size_t bytes;
    uint32_t *px;

    if (img == NULL || w <= 0 || h <= 0 || w > SOL_IMG_MAX_DIM || h > SOL_IMG_MAX_DIM) {
        return NULL;
    }
    if (sol__mul_overflow_sz((size_t)w, (size_t)h, &count) ||
        sol__mul_overflow_sz(count, sizeof(uint32_t), &bytes)) {
        return NULL;
    }

    if (arena != NULL) {
        px = (uint32_t *)sol_arena_alloc_aligned(arena, bytes, 8u);
        img->owns_memory = false;
    } else {
        px = (uint32_t *)SOL_MALLOC(bytes);
        img->owns_memory = true;
    }

    if (px == NULL) {
        img->pixels = NULL;
        img->owns_memory = false;
        return NULL;
    }

    img->pixels = px;
    if (sol_surface_init(&img->surface, px, w, h, w) != SOL_OK) {
        if (img->owns_memory) {
            SOL_FREE(px);
        }
        img->pixels = NULL;
        img->owns_memory = false;
        return NULL;
    }
    return px;
}

void *sol__tmp_alloc(size_t size, sol_arena_t *arena) {
    if (size == 0u) {
        return NULL;
    }
    if (arena != NULL) {
        return sol_arena_alloc_aligned(arena, size, 8u);
    }
    return SOL_MALLOC(size);
}

void sol__tmp_free(void *ptr, const sol_arena_t *arena) {
    if (arena == NULL && ptr != NULL) {
        SOL_FREE(ptr);
    }
}

void sol_image_destroy(sol_image_t *img) {
    if (img == NULL) {
        return;
    }
    if (img->owns_memory && img->pixels != NULL) {
        SOL_FREE(img->pixels);
    }
    SOL_MEMSET(img, 0, sizeof(*img));
}

sol_image_format_t sol_image_detect(const uint8_t *data, size_t size) {
    if (data == NULL || size < 2u) {
        return SOL_IMG_UNKNOWN;
    }
    if (size >= 8u && data[0] == 0x89u && data[1] == 'P' && data[2] == 'N' && data[3] == 'G' &&
        data[4] == 0x0Du && data[5] == 0x0Au && data[6] == 0x1Au && data[7] == 0x0Au) {
        return SOL_IMG_PNG;
    }
    if (size >= 14u && data[0] == 'q' && data[1] == 'o' && data[2] == 'i' && data[3] == 'f') {
        return SOL_IMG_QOI;
    }
    if (size >= 26u && data[0] == 'B' && data[1] == 'M') {
        return SOL_IMG_BMP;
    }
    if (size >= 3u && data[0] == 'P' && (data[1] >= '1' && data[1] <= '6') &&
        (data[2] == ' ' || data[2] == '\t' || data[2] == '\n' || data[2] == '\r' ||
         data[2] == '#')) {
        return SOL_IMG_PPM;
    }
    if (size >= 44u && SOL_MEMCMP(data + size - 18u, "TRUEVISION-XFILE.", 17u) == 0) {
        return SOL_IMG_TGA;
    }
    return SOL_IMG_UNKNOWN;
}

/* ============================================================================
 * PPM / PGM / PBM (P1..P6)
 * ============================================================================ */

SOL_PRIVATE bool sol__pnm_is_space(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

SOL_PRIVATE void sol__pnm_skip_ws_comments(const uint8_t *data, size_t size, size_t *pos) {
    while (*pos < size) {
        if (sol__pnm_is_space(data[*pos])) {
            (*pos)++;
        } else if (data[*pos] == '#') {
            while (*pos < size && data[*pos] != '\n' && data[*pos] != '\r') {
                (*pos)++;
            }
        } else {
            break;
        }
    }
}

SOL_PRIVATE bool sol__pnm_read_int(const uint8_t *data, size_t size, size_t *pos, int32_t *out) {
    int64_t val = 0;
    bool any = false;

    sol__pnm_skip_ws_comments(data, size, pos);
    while (*pos < size && data[*pos] >= '0' && data[*pos] <= '9') {
        val = val * 10 + (int64_t)(data[*pos] - '0');
        if (val > 1000000) {
            return false;
        }
        (*pos)++;
        any = true;
    }
    if (!any) {
        return false;
    }
    *out = (int32_t)val;
    return true;
}

sol_status_t sol_image_load_ppm_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                    sol_arena_t *arena) {
    size_t pos = 0u;
    uint8_t magic;
    int32_t w = 0;
    int32_t h = 0;
    int32_t maxval = 1;
    uint32_t *px;
    size_t mark;
    int32_t total;
    int32_t i;

    if (img == NULL || data == NULL || size < 6u) {
        return SOL_ERR_INVALID_ARG;
    }
    SOL_MEMSET(img, 0, sizeof(*img));

    if (data[0] != 'P' || data[1] < '1' || data[1] > '6') {
        return SOL_ERR_INVALID_ARG;
    }
    magic = data[1];
    pos = 2u;

    if (!sol__pnm_read_int(data, size, &pos, &w) || !sol__pnm_read_int(data, size, &pos, &h)) {
        return SOL_ERR_INVALID_ARG;
    }
    if (w <= 0 || h <= 0 || w > SOL_IMG_MAX_DIM || h > SOL_IMG_MAX_DIM) {
        return SOL_ERR_INVALID_ARG;
    }

    if (magic != '1' && magic != '4') {
        if (!sol__pnm_read_int(data, size, &pos, &maxval) || maxval <= 0 || maxval > 65535) {
            return SOL_ERR_INVALID_ARG;
        }
    }

    if (magic >= '4') {
        if (pos >= size || !sol__pnm_is_space(data[pos])) {
            return SOL_ERR_INVALID_ARG;
        }
        pos++;
    }

    mark = (arena != NULL) ? sol_arena_mark(arena) : 0u;
    px = sol__img_alloc_pixels(img, w, h, arena);
    if (px == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    total = w * h;

    if (magic == '1') {
        for (i = 0; i < total; ++i) {
            int32_t bit = 0;
            sol__pnm_skip_ws_comments(data, size, &pos);
            if (pos >= size || (data[pos] != '0' && data[pos] != '1')) {
                goto fail;
            }
            bit = (data[pos++] == '0') ? 255 : 0;
            px[i] = SOL_RGB(bit, bit, bit);
        }
    } else if (magic == '2') {
        for (i = 0; i < total; ++i) {
            int32_t v = 0;
            uint8_t g;
            if (!sol__pnm_read_int(data, size, &pos, &v) || v < 0 || v > maxval) {
                goto fail;
            }
            g = (uint8_t)((v * 255) / maxval);
            px[i] = SOL_RGB(g, g, g);
        }
    } else if (magic == '3') {
        for (i = 0; i < total; ++i) {
            int32_t r = 0;
            int32_t g = 0;
            int32_t b = 0;
            if (!sol__pnm_read_int(data, size, &pos, &r) ||
                !sol__pnm_read_int(data, size, &pos, &g) ||
                !sol__pnm_read_int(data, size, &pos, &b)) {
                goto fail;
            }
            if (r < 0 || r > maxval || g < 0 || g > maxval || b < 0 || b > maxval) {
                goto fail;
            }
            px[i] = SOL_RGB((r * 255) / maxval, (g * 255) / maxval, (b * 255) / maxval);
        }
    } else if (magic == '4') {
        size_t row_bytes = (size_t)((w + 7) / 8);
        int32_t y;
        if (size - pos < row_bytes * (size_t)h) {
            goto fail;
        }
        for (y = 0; y < h; ++y) {
            const uint8_t *row = data + pos + (size_t)y * row_bytes;
            int32_t x;
            for (x = 0; x < w; ++x) {
                uint8_t bit = (row[x >> 3] >> (7 - (x & 7))) & 1u;
                uint8_t v = bit ? 0u : 255u;
                px[(size_t)y * (size_t)w + (size_t)x] = SOL_RGB(v, v, v);
            }
        }
    } else {
        size_t ch = (magic == '5') ? 1u : 3u;
        size_t bps = (maxval > 255) ? 2u : 1u;
        size_t needed = (size_t)total * ch * bps;
        if (size - pos < needed) {
            goto fail;
        }
        for (i = 0; i < total; ++i) {
            if (ch == 1u) {
                int32_t v = (bps == 1u) ? (int32_t)data[pos++]
                                        : (((int32_t)data[pos] << 8) | data[pos + 1]);
                uint8_t g;
                if (bps == 2u) {
                    pos += 2u;
                }
                g = (uint8_t)((SOL_MIN(v, maxval) * 255) / maxval);
                px[i] = SOL_RGB(g, g, g);
            } else if (bps == 1u) {
                uint8_t r = (uint8_t)(((int32_t)data[pos] * 255) / maxval);
                uint8_t g = (uint8_t)(((int32_t)data[pos + 1] * 255) / maxval);
                uint8_t b = (uint8_t)(((int32_t)data[pos + 2] * 255) / maxval);
                pos += 3u;
                px[i] = SOL_RGB(r, g, b);
            } else {
                int32_t r = ((int32_t)data[pos] << 8) | data[pos + 1];
                int32_t g = ((int32_t)data[pos + 2] << 8) | data[pos + 3];
                int32_t b = ((int32_t)data[pos + 4] << 8) | data[pos + 5];
                pos += 6u;
                px[i] = SOL_RGB((SOL_MIN(r, maxval) * 255) / maxval,
                                (SOL_MIN(g, maxval) * 255) / maxval,
                                (SOL_MIN(b, maxval) * 255) / maxval);
            }
        }
    }

    return SOL_OK;

fail:
    sol_image_destroy(img);
    if (arena != NULL) {
        sol_arena_rewind(arena, mark);
    }
    return SOL_ERR_INVALID_ARG;
}

sol_status_t sol_image_encode_ppm(const sol_surface_t *surf, uint8_t *out_buf, size_t capacity,
                                  size_t *out_written) {
    char hdr[48];
    size_t hdr_len = 0u;
    size_t needed;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }

    /* Format "P6\n<w> <h>\n255\n" without stdio snprintf */
    {
        int32_t nums[2];
        int k;
        nums[0] = surf->width;
        nums[1] = surf->height;
        hdr[hdr_len++] = 'P';
        hdr[hdr_len++] = '6';
        hdr[hdr_len++] = '\n';
        for (k = 0; k < 2; ++k) {
            char rev[16];
            size_t rlen = 0u;
            int32_t val = nums[k];
            do {
                rev[rlen++] = (char)('0' + (val % 10));
                val /= 10;
            } while (val > 0);
            while (rlen > 0u) {
                hdr[hdr_len++] = rev[--rlen];
            }
            hdr[hdr_len++] = (k == 0) ? ' ' : '\n';
        }
        hdr[hdr_len++] = '2';
        hdr[hdr_len++] = '5';
        hdr[hdr_len++] = '5';
        hdr[hdr_len++] = '\n';
    }

    {
        size_t px_bytes;
        if (sol__mul_overflow_sz((size_t)surf->width * (size_t)surf->height, 3u, &px_bytes)) {
            return SOL_ERR_OUT_OF_BOUNDS;
        }
        needed = hdr_len + px_bytes;
    }

    if (out_written != NULL) {
        *out_written = needed;
    }
    if (out_buf == NULL) {
        return SOL_OK;
    }
    if (capacity < needed) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    SOL_MEMCPY(out_buf, hdr, hdr_len);
    {
        int32_t y;
        int32_t x;
        size_t pos = hdr_len;
        for (y = 0; y < surf->height; ++y) {
            const uint32_t *row = surf->pixels + (size_t)y * (size_t)surf->stride;
            for (x = 0; x < surf->width; ++x) {
                uint32_t c = row[x];
                out_buf[pos++] = SOL_COLOR_R(c);
                out_buf[pos++] = SOL_COLOR_G(c);
                out_buf[pos++] = SOL_COLOR_B(c);
            }
        }
    }
    return SOL_OK;
}

/* ============================================================================
 * BMP (24-bit & 32-bit BI_RGB / BI_BITFIELDS)
 * ============================================================================ */

SOL_PRIVATE uint32_t sol__mask_extract(uint32_t val, uint32_t mask) {
    uint32_t shift = 0u;
    uint32_t bits = 0u;
    uint32_t m;

    if (mask == 0u) {
        return 0u;
    }
    m = mask;
    while ((m & 1u) == 0u) {
        m >>= 1;
        shift++;
    }
    while ((m & 1u) != 0u) {
        m >>= 1;
        bits++;
    }
    if (bits == 0u) {
        return 0u;
    }
    {
        uint32_t v = (val & mask) >> shift;
        if (bits >= 8u) {
            return v >> (bits - 8u);
        }
        return (v * 255u) / ((1u << bits) - 1u);
    }
}

sol_status_t sol_image_load_bmp_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                    sol_arena_t *arena) {
    uint32_t px_offset;
    uint32_t dib_size;
    int32_t w;
    int32_t raw_h;
    int32_t h;
    bool top_down;
    uint16_t planes;
    uint16_t bpp;
    uint32_t comp;
    uint32_t r_mask = 0x00FF0000u;
    uint32_t g_mask = 0x0000FF00u;
    uint32_t b_mask = 0x000000FFu;
    uint32_t a_mask = 0xFF000000u;
    uint32_t *px;
    size_t row_stride;

    if (img == NULL || data == NULL || size < 54u) {
        return SOL_ERR_INVALID_ARG;
    }
    SOL_MEMSET(img, 0, sizeof(*img));

    if (data[0] != 'B' || data[1] != 'M') {
        return SOL_ERR_INVALID_ARG;
    }

    px_offset = sol__read_le32(data + 10);
    dib_size = sol__read_le32(data + 14);
    if (dib_size < 40u || 14u + (size_t)dib_size > size || px_offset < 14u + dib_size ||
        px_offset >= size) {
        return SOL_ERR_INVALID_ARG;
    }

    w = (int32_t)sol__read_le32(data + 18);
    raw_h = (int32_t)sol__read_le32(data + 22);
    planes = sol__read_le16(data + 26);
    bpp = sol__read_le16(data + 28);
    comp = sol__read_le32(data + 30);

    if (w <= 0 || w > SOL_IMG_MAX_DIM || raw_h == 0 || raw_h == INT32_MIN || planes != 1u) {
        return SOL_ERR_INVALID_ARG;
    }
    top_down = (raw_h < 0);
    h = top_down ? -raw_h : raw_h;
    if (h > SOL_IMG_MAX_DIM) {
        return SOL_ERR_INVALID_ARG;
    }
    if (bpp != 24u && bpp != 32u) {
        return SOL_ERR_INVALID_ARG;
    }
    if (comp != 0u && !(comp == 3u && bpp == 32u)) {
        return SOL_ERR_INVALID_ARG;
    }

    if (comp == 3u) {
        if (dib_size >= 56u && 14u + 56u <= size) {
            r_mask = sol__read_le32(data + 54);
            g_mask = sol__read_le32(data + 58);
            b_mask = sol__read_le32(data + 62);
            a_mask = sol__read_le32(data + 66);
        } else if (14u + 52u <= size) {
            r_mask = sol__read_le32(data + 54);
            g_mask = sol__read_le32(data + 58);
            b_mask = sol__read_le32(data + 62);
            a_mask = 0u;
        }
    }

    row_stride = (((size_t)w * (size_t)(bpp / 8u) + 3u) / 4u) * 4u;
    {
        size_t total_bytes;
        if (sol__mul_overflow_sz(row_stride, (size_t)h, &total_bytes) ||
            (size_t)px_offset + total_bytes > size) {
            return SOL_ERR_INVALID_ARG;
        }
    }

    px = sol__img_alloc_pixels(img, w, h, arena);
    if (px == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    {
        int32_t y;
        bool any_alpha = false;
        for (y = 0; y < h; ++y) {
            int32_t dst_y = top_down ? y : (h - 1 - y);
            const uint8_t *s_row = data + px_offset + (size_t)y * row_stride;
            uint32_t *d_row = px + (size_t)dst_y * (size_t)w;
            int32_t x;

            if (bpp == 24u) {
                for (x = 0; x < w; ++x) {
                    d_row[x] = SOL_RGB(s_row[x * 3 + 2], s_row[x * 3 + 1], s_row[x * 3 + 0]);
                }
            } else if (comp == 0u) {
                for (x = 0; x < w; ++x) {
                    uint8_t a = s_row[x * 4 + 3];
                    if (a != 0u) {
                        any_alpha = true;
                    }
                    d_row[x] = SOL_ARGB(a, s_row[x * 4 + 2], s_row[x * 4 + 1], s_row[x * 4 + 0]);
                }
            } else {
                for (x = 0; x < w; ++x) {
                    uint32_t raw = sol__read_le32(s_row + (size_t)x * 4u);
                    uint32_t a = (a_mask != 0u) ? sol__mask_extract(raw, a_mask) : 255u;
                    if (a_mask != 0u && a != 0u) {
                        any_alpha = true;
                    }
                    d_row[x] =
                        SOL_ARGB(a, sol__mask_extract(raw, r_mask), sol__mask_extract(raw, g_mask),
                                 sol__mask_extract(raw, b_mask));
                }
            }
        }

        /* Many 32-bit BI_RGB BMPs leave all alpha bytes 0; treat as opaque XRGB in that case */
        if (bpp == 32u && (comp == 0u || a_mask != 0u) && !any_alpha) {
            size_t total = (size_t)w * (size_t)h;
            size_t i;
            for (i = 0u; i < total; ++i) {
                px[i] |= 0xFF000000u;
            }
        }
    }

    return SOL_OK;
}

sol_status_t sol_image_encode_bmp(const sol_surface_t *surf, uint8_t *out_buf, size_t capacity,
                                  size_t *out_written) {
    size_t px_bytes;
    size_t total;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }
    if (sol__mul_overflow_sz((size_t)surf->width * (size_t)surf->height, 4u, &px_bytes)) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    total = 54u + px_bytes;

    if (out_written != NULL) {
        *out_written = total;
    }
    if (out_buf == NULL) {
        return SOL_OK;
    }
    if (capacity < total) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    SOL_MEMSET(out_buf, 0, 54u);
    out_buf[0] = 'B';
    out_buf[1] = 'M';
    sol__write_le32(out_buf + 2, (uint32_t)total);
    sol__write_le32(out_buf + 10, 54u);
    sol__write_le32(out_buf + 14, 40u);
    sol__write_le32(out_buf + 18, (uint32_t)surf->width);
    sol__write_le32(out_buf + 22, (uint32_t)(-surf->height)); /* Top-down 32-bit BGRA */
    sol__write_le16(out_buf + 26, 1u);
    sol__write_le16(out_buf + 28, 32u);
    sol__write_le32(out_buf + 34, (uint32_t)px_bytes);

    {
        int32_t y;
        int32_t x;
        size_t pos = 54u;
        for (y = 0; y < surf->height; ++y) {
            const uint32_t *row = surf->pixels + (size_t)y * (size_t)surf->stride;
            for (x = 0; x < surf->width; ++x) {
                uint32_t c = row[x];
                out_buf[pos++] = SOL_COLOR_B(c);
                out_buf[pos++] = SOL_COLOR_G(c);
                out_buf[pos++] = SOL_COLOR_R(c);
                out_buf[pos++] = SOL_COLOR_A(c);
            }
        }
    }
    return SOL_OK;
}

/* ============================================================================
 * TGA (Uncompressed & RLE, 8 / 24 / 32-bit)
 * ============================================================================ */

SOL_PRIVATE uint32_t sol__tga_read_pixel(const uint8_t *p, uint8_t bpp) {
    if (bpp == 8u) {
        return SOL_RGB(p[0], p[0], p[0]);
    }
    if (bpp == 24u) {
        return SOL_RGB(p[2], p[1], p[0]);
    }
    return SOL_ARGB(p[3], p[2], p[1], p[0]);
}

sol_status_t sol_image_load_tga_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                    sol_arena_t *arena) {
    uint8_t id_len;
    uint8_t cmap_type;
    uint8_t img_type;
    int32_t w;
    int32_t h;
    uint8_t bpp;
    uint8_t desc;
    bool top_down;
    bool is_rle;
    size_t pos;
    size_t bytes_pp;
    size_t total_px;
    uint32_t *px;
    size_t mark;

    if (img == NULL || data == NULL || size < 18u) {
        return SOL_ERR_INVALID_ARG;
    }
    SOL_MEMSET(img, 0, sizeof(*img));

    id_len = data[0];
    cmap_type = data[1];
    img_type = data[2];
    w = (int32_t)sol__read_le16(data + 12);
    h = (int32_t)sol__read_le16(data + 14);
    bpp = data[16];
    desc = data[17];

    if (cmap_type != 0u || w <= 0 || h <= 0 || w > SOL_IMG_MAX_DIM || h > SOL_IMG_MAX_DIM) {
        return SOL_ERR_INVALID_ARG;
    }
    if (img_type != 2u && img_type != 3u && img_type != 10u && img_type != 11u) {
        return SOL_ERR_INVALID_ARG;
    }
    if ((img_type == 3u || img_type == 11u) && bpp != 8u) {
        return SOL_ERR_INVALID_ARG;
    }
    if ((img_type == 2u || img_type == 10u) && bpp != 24u && bpp != 32u) {
        return SOL_ERR_INVALID_ARG;
    }

    pos = 18u + (size_t)id_len;
    if (pos >= size) {
        return SOL_ERR_INVALID_ARG;
    }

    is_rle = (img_type == 10u || img_type == 11u);
    top_down = (desc & 0x20u) != 0u;
    bytes_pp = (size_t)bpp / 8u;
    total_px = (size_t)w * (size_t)h;

    mark = (arena != NULL) ? sol_arena_mark(arena) : 0u;
    px = sol__img_alloc_pixels(img, w, h, arena);
    if (px == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    if (!is_rle) {
        size_t i;
        if (size - pos < total_px * bytes_pp) {
            goto fail;
        }
        for (i = 0u; i < total_px; ++i) {
            px[i] = sol__tga_read_pixel(data + pos, bpp);
            pos += bytes_pp;
        }
    } else {
        size_t i = 0u;
        while (i < total_px) {
            uint8_t hdr;
            size_t run;
            if (pos >= size) {
                goto fail;
            }
            hdr = data[pos++];
            run = (size_t)(hdr & 0x7Fu) + 1u;
            if (i + run > total_px) {
                goto fail;
            }
            if ((hdr & 0x80u) != 0u) {
                uint32_t c;
                size_t k;
                if (size - pos < bytes_pp) {
                    goto fail;
                }
                c = sol__tga_read_pixel(data + pos, bpp);
                pos += bytes_pp;
                for (k = 0u; k < run; ++k) {
                    px[i++] = c;
                }
            } else {
                size_t k;
                if (size - pos < run * bytes_pp) {
                    goto fail;
                }
                for (k = 0u; k < run; ++k) {
                    px[i++] = sol__tga_read_pixel(data + pos, bpp);
                    pos += bytes_pp;
                }
            }
        }
    }

    if (!top_down) {
        sol_surface_flip_v(&img->surface);
    }
    return SOL_OK;

fail:
    sol_image_destroy(img);
    if (arena != NULL) {
        sol_arena_rewind(arena, mark);
    }
    return SOL_ERR_INVALID_ARG;
}

sol_status_t sol_image_encode_tga(const sol_surface_t *surf, uint8_t *out_buf, size_t capacity,
                                  size_t *out_written) {
    size_t px_bytes;
    size_t total;
    int32_t y;
    int32_t x;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }
    if (sol__mul_overflow_sz((size_t)surf->width * (size_t)surf->height, 4u, &px_bytes)) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    total = 18u + px_bytes;

    if (out_written != NULL) {
        *out_written = total;
    }
    if (out_buf == NULL) {
        return SOL_OK;
    }
    if (capacity < total) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    SOL_MEMSET(out_buf, 0, 18u);
    out_buf[2] = 2u; /* Uncompressed true-color */
    sol__write_le16(out_buf + 12, (uint16_t)surf->width);
    sol__write_le16(out_buf + 14, (uint16_t)surf->height);
    out_buf[16] = 32u;
    out_buf[17] = 0x28u; /* Top-down origin + 8 alpha bits */

    {
        size_t pos = 18u;
        for (y = 0; y < surf->height; ++y) {
            const uint32_t *row = surf->pixels + (size_t)y * (size_t)surf->stride;
            for (x = 0; x < surf->width; ++x) {
                uint32_t c = row[x];
                out_buf[pos++] = SOL_COLOR_B(c);
                out_buf[pos++] = SOL_COLOR_G(c);
                out_buf[pos++] = SOL_COLOR_R(c);
                out_buf[pos++] = SOL_COLOR_A(c);
            }
        }
    }
    return SOL_OK;
}

/* ============================================================================
 * QOI (Quite OK Image)
 * ============================================================================ */

SOL_PRIVATE uint32_t sol__qoi_hash(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r * 3u + (uint32_t)g * 5u + (uint32_t)b * 7u + (uint32_t)a * 11u) & 63u;
}

sol_status_t sol_image_load_qoi_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                    sol_arena_t *arena) {
    int32_t w;
    int32_t h;
    uint8_t channels;
    uint8_t colorspace;
    uint32_t *px;
    size_t total;
    size_t pos = 14u;
    uint32_t index[64];
    uint8_t pr = 0u;
    uint8_t pg = 0u;
    uint8_t pb = 0u;
    uint8_t pa = 255u;
    size_t run = 0u;
    size_t i;

    if (img == NULL || data == NULL || size < 22u) {
        return SOL_ERR_INVALID_ARG;
    }
    SOL_MEMSET(img, 0, sizeof(*img));

    if (data[0] != 'q' || data[1] != 'o' || data[2] != 'i' || data[3] != 'f') {
        return SOL_ERR_INVALID_ARG;
    }

    w = (int32_t)sol__read_be32(data + 4);
    h = (int32_t)sol__read_be32(data + 8);
    channels = data[12];
    colorspace = data[13];

    if (w <= 0 || h <= 0 || w > SOL_IMG_MAX_DIM || h > SOL_IMG_MAX_DIM ||
        (channels != 3u && channels != 4u) || colorspace > 1u) {
        return SOL_ERR_INVALID_ARG;
    }

    px = sol__img_alloc_pixels(img, w, h, arena);
    if (px == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    SOL_MEMSET(index, 0, sizeof(index));
    total = (size_t)w * (size_t)h;

    for (i = 0u; i < total; ++i) {
        if (run > 0u) {
            run--;
        } else if (pos < size) {
            uint8_t b1 = data[pos++];
            if (b1 == 0xFEu) {
                if (pos + 3u > size) {
                    break;
                }
                pr = data[pos++];
                pg = data[pos++];
                pb = data[pos++];
            } else if (b1 == 0xFFu) {
                if (pos + 4u > size) {
                    break;
                }
                pr = data[pos++];
                pg = data[pos++];
                pb = data[pos++];
                pa = data[pos++];
            } else {
                uint8_t tag = b1 & 0xC0u;
                if (tag == 0x00u) {
                    uint32_t c = index[b1 & 0x3Fu];
                    pa = SOL_COLOR_A(c);
                    pr = SOL_COLOR_R(c);
                    pg = SOL_COLOR_G(c);
                    pb = SOL_COLOR_B(c);
                } else if (tag == 0x40u) {
                    pr = (uint8_t)(pr + ((b1 >> 4) & 3u) - 2u);
                    pg = (uint8_t)(pg + ((b1 >> 2) & 3u) - 2u);
                    pb = (uint8_t)(pb + (b1 & 3u) - 2u);
                } else if (tag == 0x80u) {
                    if (pos >= size) {
                        break;
                    }
                    {
                        uint8_t b2 = data[pos++];
                        int vg = (int)(b1 & 0x3Fu) - 32;
                        pr = (uint8_t)((int)pr + vg - 8 + (int)((b2 >> 4) & 0x0Fu));
                        pg = (uint8_t)((int)pg + vg);
                        pb = (uint8_t)((int)pb + vg - 8 + (int)(b2 & 0x0Fu));
                    }
                } else {
                    run = (size_t)(b1 & 0x3Fu);
                }
            }
            index[sol__qoi_hash(pr, pg, pb, pa)] = SOL_ARGB(pa, pr, pg, pb);
        }

        px[i] = SOL_ARGB(pa, pr, pg, pb);
    }

    return SOL_OK;
}

sol_status_t sol_image_encode_qoi(const sol_surface_t *surf, uint8_t *out_buf, size_t capacity,
                                  size_t *out_written) {
    size_t max_bytes;
    uint32_t index[64];
    uint8_t pr = 0u;
    uint8_t pg = 0u;
    uint8_t pb = 0u;
    uint8_t pa = 255u;
    size_t run = 0u;
    size_t pos = 14u;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 0) {
        return SOL_ERR_INVALID_ARG;
    }
    if (sol__mul_overflow_sz((size_t)surf->width * (size_t)surf->height, 5u, &max_bytes)) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    max_bytes += 14u + 8u;

    if (out_buf == NULL) {
        if (out_written != NULL) {
            *out_written = max_bytes;
        }
        return SOL_OK;
    }
    if (capacity < 22u) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    out_buf[0] = 'q';
    out_buf[1] = 'o';
    out_buf[2] = 'i';
    out_buf[3] = 'f';
    sol__write_be32(out_buf + 4, (uint32_t)surf->width);
    sol__write_be32(out_buf + 8, (uint32_t)surf->height);
    out_buf[12] = 4u;
    out_buf[13] = 0u;

    SOL_MEMSET(index, 0, sizeof(index));

    {
        int32_t y;
        for (y = 0; y < surf->height; ++y) {
            const uint32_t *row = surf->pixels + (size_t)y * (size_t)surf->stride;
            int32_t x;
            for (x = 0; x < surf->width; ++x) {
                uint32_t c = row[x];
                uint8_t a = SOL_COLOR_A(c);
                uint8_t r = SOL_COLOR_R(c);
                uint8_t g = SOL_COLOR_G(c);
                uint8_t b = SOL_COLOR_B(c);
                bool is_last = (y == surf->height - 1) && (x == surf->width - 1);

                if (r == pr && g == pg && b == pb && a == pa) {
                    run++;
                    if (run == 62u || is_last) {
                        if (pos + 1u > capacity) {
                            return SOL_ERR_OUT_OF_BOUNDS;
                        }
                        out_buf[pos++] = (uint8_t)(0xC0u | (run - 1u));
                        run = 0u;
                    }
                } else {
                    if (run > 0u) {
                        if (pos + 1u > capacity) {
                            return SOL_ERR_OUT_OF_BOUNDS;
                        }
                        out_buf[pos++] = (uint8_t)(0xC0u | (run - 1u));
                        run = 0u;
                    }

                    {
                        uint32_t h_idx = sol__qoi_hash(r, g, b, a);
                        if (index[h_idx] == c) {
                            if (pos + 1u > capacity) {
                                return SOL_ERR_OUT_OF_BOUNDS;
                            }
                            out_buf[pos++] = (uint8_t)h_idx;
                        } else {
                            index[h_idx] = c;
                            if (a == pa) {
                                int8_t vr = (int8_t)((int)r - (int)pr);
                                int8_t vg = (int8_t)((int)g - (int)pg);
                                int8_t vb = (int8_t)((int)b - (int)pb);
                                int8_t vrg = (int8_t)(vr - vg);
                                int8_t vbg = (int8_t)(vb - vg);

                                if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 && vb >= -2 &&
                                    vb <= 1) {
                                    if (pos + 1u > capacity) {
                                        return SOL_ERR_OUT_OF_BOUNDS;
                                    }
                                    out_buf[pos++] =
                                        (uint8_t)(0x40u | (((uint8_t)(vr + 2)) << 4) |
                                                  (((uint8_t)(vg + 2)) << 2) | ((uint8_t)(vb + 2)));
                                } else if (vrg >= -8 && vrg <= 7 && vg >= -32 && vg <= 31 &&
                                           vbg >= -8 && vbg <= 7) {
                                    if (pos + 2u > capacity) {
                                        return SOL_ERR_OUT_OF_BOUNDS;
                                    }
                                    out_buf[pos++] = (uint8_t)(0x80u | (uint8_t)(vg + 32));
                                    out_buf[pos++] = (uint8_t)((((uint8_t)(vrg + 8)) << 4) |
                                                               ((uint8_t)(vbg + 8)));
                                } else {
                                    if (pos + 4u > capacity) {
                                        return SOL_ERR_OUT_OF_BOUNDS;
                                    }
                                    out_buf[pos++] = 0xFEu;
                                    out_buf[pos++] = r;
                                    out_buf[pos++] = g;
                                    out_buf[pos++] = b;
                                }
                            } else {
                                if (pos + 5u > capacity) {
                                    return SOL_ERR_OUT_OF_BOUNDS;
                                }
                                out_buf[pos++] = 0xFFu;
                                out_buf[pos++] = r;
                                out_buf[pos++] = g;
                                out_buf[pos++] = b;
                                out_buf[pos++] = a;
                            }
                        }
                    }
                }
                pr = r;
                pg = g;
                pb = b;
                pa = a;
            }
        }

        if (pos + 8u > capacity) {
            return SOL_ERR_OUT_OF_BOUNDS;
        }
        SOL_MEMSET(out_buf + pos, 0, 7u);
        out_buf[pos + 7u] = 1u;
        pos += 8u;

        if (out_written != NULL) {
            *out_written = pos;
        }
        return SOL_OK;
    }
}

sol_status_t sol_image_load_mem(sol_image_t *img, const uint8_t *data, size_t size,
                                sol_arena_t *arena) {
    sol_image_format_t fmt = sol_image_detect(data, size);

    switch (fmt) {
    case SOL_IMG_PNG:
        return sol_image_load_png_mem(img, data, size, arena);
    case SOL_IMG_QOI:
        return sol_image_load_qoi_mem(img, data, size, arena);
    case SOL_IMG_BMP:
        return sol_image_load_bmp_mem(img, data, size, arena);
    case SOL_IMG_PPM:
        return sol_image_load_ppm_mem(img, data, size, arena);
    case SOL_IMG_TGA:
        return sol_image_load_tga_mem(img, data, size, arena);
    default:
        return sol_image_load_tga_mem(img, data, size, arena);
    }
}

#if !defined(SOL_FREESTANDING) && !defined(SOL_NO_STDIO)
sol_status_t sol_image_load_file(sol_image_t *img, const char *path, sol_arena_t *arena) {
    FILE *fp;
    long fsize;
    uint8_t *buf;
    size_t read_sz;
    sol_status_t st;

    if (img == NULL || path == NULL) {
        return SOL_ERR_INVALID_ARG;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return SOL_ERR_INVALID_ARG;
    }
    fsize = ftell(fp);
    if (fsize <= 0 || fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return SOL_ERR_INVALID_ARG;
    }

    buf = (uint8_t *)SOL_MALLOC((size_t)fsize);
    if (buf == NULL) {
        fclose(fp);
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    read_sz = fread(buf, 1u, (size_t)fsize, fp);
    fclose(fp);
    if (read_sz != (size_t)fsize) {
        SOL_FREE(buf);
        return SOL_ERR_INVALID_ARG;
    }

    st = sol_image_load_mem(img, buf, read_sz, arena);
    SOL_FREE(buf);
    return st;
}

sol_status_t sol_image_save_file(const sol_surface_t *surf, const char *path,
                                 sol_image_format_t fmt) {
    size_t needed = 0u;
    size_t written = 0u;
    uint8_t *buf;
    sol_status_t st;
    FILE *fp;

    if (surf == NULL || path == NULL) {
        return SOL_ERR_INVALID_ARG;
    }

    switch (fmt) {
    case SOL_IMG_PPM:
        st = sol_image_encode_ppm(surf, NULL, 0u, &needed);
        break;
    case SOL_IMG_BMP:
        st = sol_image_encode_bmp(surf, NULL, 0u, &needed);
        break;
    case SOL_IMG_TGA:
        st = sol_image_encode_tga(surf, NULL, 0u, &needed);
        break;
    case SOL_IMG_QOI:
        st = sol_image_encode_qoi(surf, NULL, 0u, &needed);
        break;
    case SOL_IMG_PNG:
        st = sol_image_encode_png(surf, NULL, 0u, &needed);
        break;
    default:
        return SOL_ERR_INVALID_ARG;
    }
    if (st != SOL_OK || needed == 0u) {
        return st;
    }

    buf = (uint8_t *)SOL_MALLOC(needed);
    if (buf == NULL) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    switch (fmt) {
    case SOL_IMG_PPM:
        st = sol_image_encode_ppm(surf, buf, needed, &written);
        break;
    case SOL_IMG_BMP:
        st = sol_image_encode_bmp(surf, buf, needed, &written);
        break;
    case SOL_IMG_TGA:
        st = sol_image_encode_tga(surf, buf, needed, &written);
        break;
    case SOL_IMG_QOI:
        st = sol_image_encode_qoi(surf, buf, needed, &written);
        break;
    case SOL_IMG_PNG:
        st = sol_image_encode_png(surf, buf, needed, &written);
        break;
    default:
        st = SOL_ERR_INVALID_ARG;
        break;
    }

    if (st != SOL_OK) {
        SOL_FREE(buf);
        return st;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        SOL_FREE(buf);
        return SOL_ERR_INVALID_ARG;
    }
    if (fwrite(buf, 1u, written, fp) != written) {
        fclose(fp);
        SOL_FREE(buf);
        return SOL_ERR_INVALID_ARG;
    }

    fclose(fp);
    SOL_FREE(buf);
    return SOL_OK;
}
#endif
