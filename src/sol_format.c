#include "sol_internal.h"

/* 8x8 ordered Bayer matrix (values 0..63) for low-bit-depth dithering */
static const uint8_t k_bayer8x8[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};

static inline uint32_t sol__clamp_u8(uint32_t v) {
    return (v > 255u) ? 255u : v;
}

static inline uint16_t sol__pack_rgb565(uint32_t r, uint32_t g, uint32_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int32_t sol_format_bpp(sol_pixel_format_t fmt) {
    switch (fmt) {
    case SOL_FMT_ARGB8888:
    case SOL_FMT_XRGB8888:
    case SOL_FMT_RGBA8888:
    case SOL_FMT_BGRA8888:
    case SOL_FMT_ABGR8888:
        return 4;
    case SOL_FMT_RGB888:
    case SOL_FMT_BGR888:
        return 3;
    case SOL_FMT_RGB565:
    case SOL_FMT_BGR565:
        return 2;
    case SOL_FMT_A8:
    case SOL_FMT_GRAY8:
        return 1;
    default:
        return 0;
    }
}

sol_status_t sol_convert_row(void *dst, sol_pixel_format_t dst_fmt, const uint32_t *src_argb,
                             int32_t width) {
    int32_t i;

    if (dst == NULL || src_argb == NULL || width <= 0) {
        return SOL_ERR_INVALID_ARG;
    }

    switch (dst_fmt) {
    case SOL_FMT_ARGB8888:
        SOL_MEMCPY(dst, src_argb, (size_t)width * sizeof(uint32_t));
        return SOL_OK;

    case SOL_FMT_XRGB8888: {
        uint32_t *d = (uint32_t *)dst;
        for (i = 0; i < width; ++i) {
            d[i] = 0xFF000000u | (src_argb[i] & 0x00FFFFFFu);
        }
        return SOL_OK;
    }

    case SOL_FMT_RGBA8888: {
        uint32_t *d = (uint32_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            d[i] = ((c & 0x00FFFFFFu) << 8) | ((c >> 24) & 0xFFu);
        }
        return SOL_OK;
    }

    case SOL_FMT_BGRA8888: {
        uint32_t *d = (uint32_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            uint32_t a = SOL_COLOR_A(c);
            uint32_t r = SOL_COLOR_R(c);
            uint32_t g = SOL_COLOR_G(c);
            uint32_t b = SOL_COLOR_B(c);
            d[i] = (b << 24) | (g << 16) | (r << 8) | a;
        }
        return SOL_OK;
    }

    case SOL_FMT_ABGR8888: {
        uint32_t *d = (uint32_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            uint32_t a = SOL_COLOR_A(c);
            uint32_t r = SOL_COLOR_R(c);
            uint32_t g = SOL_COLOR_G(c);
            uint32_t b = SOL_COLOR_B(c);
            d[i] = (a << 24) | (b << 16) | (g << 8) | r;
        }
        return SOL_OK;
    }

    case SOL_FMT_RGB888: {
        uint8_t *d = (uint8_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            d[i * 3 + 0] = SOL_COLOR_R(c);
            d[i * 3 + 1] = SOL_COLOR_G(c);
            d[i * 3 + 2] = SOL_COLOR_B(c);
        }
        return SOL_OK;
    }

    case SOL_FMT_BGR888: {
        uint8_t *d = (uint8_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            d[i * 3 + 0] = SOL_COLOR_B(c);
            d[i * 3 + 1] = SOL_COLOR_G(c);
            d[i * 3 + 2] = SOL_COLOR_R(c);
        }
        return SOL_OK;
    }

    case SOL_FMT_RGB565: {
        uint16_t *d = (uint16_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            d[i] = sol__pack_rgb565(SOL_COLOR_R(c), SOL_COLOR_G(c), SOL_COLOR_B(c));
        }
        return SOL_OK;
    }

    case SOL_FMT_BGR565: {
        uint16_t *d = (uint16_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            d[i] = sol__pack_rgb565(SOL_COLOR_B(c), SOL_COLOR_G(c), SOL_COLOR_R(c));
        }
        return SOL_OK;
    }

    case SOL_FMT_A8: {
        uint8_t *d = (uint8_t *)dst;
        for (i = 0; i < width; ++i) {
            d[i] = SOL_COLOR_A(src_argb[i]);
        }
        return SOL_OK;
    }

    case SOL_FMT_GRAY8: {
        uint8_t *d = (uint8_t *)dst;
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            uint32_t lum = 77u * (uint32_t)SOL_COLOR_R(c) + 150u * (uint32_t)SOL_COLOR_G(c) +
                           29u * (uint32_t)SOL_COLOR_B(c) + 128u;
            d[i] = (uint8_t)(lum >> 8);
        }
        return SOL_OK;
    }

    default:
        return SOL_ERR_INVALID_ARG;
    }
}

sol_status_t sol_convert_row_dithered(void *dst, sol_pixel_format_t dst_fmt,
                                      const uint32_t *src_argb, int32_t width, int32_t y) {
    const uint8_t *row_bayer;
    uint16_t *d;
    int32_t i;

    if (dst == NULL || src_argb == NULL || width <= 0) {
        return SOL_ERR_INVALID_ARG;
    }

    if (dst_fmt != SOL_FMT_RGB565 && dst_fmt != SOL_FMT_BGR565) {
        return sol_convert_row(dst, dst_fmt, src_argb, width);
    }

    row_bayer = k_bayer8x8[(uint32_t)y & 7u];
    d = (uint16_t *)dst;

    if (dst_fmt == SOL_FMT_RGB565) {
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            uint32_t bayer = row_bayer[(uint32_t)i & 7u];
            uint32_t d5 = bayer >> 3;
            uint32_t d6 = bayer >> 4;
            uint32_t r = sol__clamp_u8((uint32_t)SOL_COLOR_R(c) + d5);
            uint32_t g = sol__clamp_u8((uint32_t)SOL_COLOR_G(c) + d6);
            uint32_t b = sol__clamp_u8((uint32_t)SOL_COLOR_B(c) + d5);
            d[i] = sol__pack_rgb565(r, g, b);
        }
    } else {
        for (i = 0; i < width; ++i) {
            uint32_t c = src_argb[i];
            uint32_t bayer = row_bayer[(uint32_t)i & 7u];
            uint32_t d5 = bayer >> 3;
            uint32_t d6 = bayer >> 4;
            uint32_t r = sol__clamp_u8((uint32_t)SOL_COLOR_R(c) + d5);
            uint32_t g = sol__clamp_u8((uint32_t)SOL_COLOR_G(c) + d6);
            uint32_t b = sol__clamp_u8((uint32_t)SOL_COLOR_B(c) + d5);
            d[i] = sol__pack_rgb565(b, g, r);
        }
    }

    return SOL_OK;
}

sol_status_t sol_present_to_buffer(void *dst_pixels, int32_t dst_stride_bytes,
                                   sol_pixel_format_t dst_fmt, const sol_surface_t *src,
                                   bool dither) {
    int32_t bpp;
    int32_t y;
    size_t min_stride_bytes;
    uint8_t *dst_row;
    const uint32_t *src_row;

    if (dst_pixels == NULL || src == NULL || src->pixels == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (src->width <= 0 || src->height <= 0 || src->stride < src->width) {
        return SOL_ERR_INVALID_ARG;
    }

    bpp = sol_format_bpp(dst_fmt);
    if (bpp <= 0) {
        return SOL_ERR_INVALID_ARG;
    }

    if (sol__mul_overflow_sz((size_t)src->width, (size_t)bpp, &min_stride_bytes) ||
        dst_stride_bytes <= 0 || (size_t)dst_stride_bytes < min_stride_bytes) {
        return SOL_ERR_INVALID_ARG;
    }

    dst_row = (uint8_t *)dst_pixels;
    src_row = src->pixels;

    for (y = 0; y < src->height; ++y) {
        sol_status_t st = dither
                              ? sol_convert_row_dithered(dst_row, dst_fmt, src_row, src->width, y)
                              : sol_convert_row(dst_row, dst_fmt, src_row, src->width);
        if (st != SOL_OK) {
            return st;
        }
        dst_row += (size_t)dst_stride_bytes;
        src_row += (size_t)src->stride;
    }

    return SOL_OK;
}
