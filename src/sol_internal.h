#ifndef SOL_INTERNAL_H
#define SOL_INTERNAL_H

#include "../include/solsticesfg.h"

#ifndef SOL_PRIVATE
    #define SOL_PRIVATE static
#endif

#define SOL_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SOL_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline uint32_t sol__div255(uint32_t x) {
    return (x + 128u + ((x + 128u) >> 8)) >> 8;
}

static inline sol_color_t sol__blend_argb_fast(sol_color_t dst, sol_color_t src) {
    uint32_t sa = SOL_COLOR_A(src);
    if (sa == 0u) {
        return dst;
    }
    if (sa == 255u) {
        return src;
    }

    uint32_t inv_a = 255u - sa;
    uint32_t da = SOL_COLOR_A(dst);

    uint32_t r = sol__div255((uint32_t)SOL_COLOR_R(src) * sa + (uint32_t)SOL_COLOR_R(dst) * inv_a);
    uint32_t g = sol__div255((uint32_t)SOL_COLOR_G(src) * sa + (uint32_t)SOL_COLOR_G(dst) * inv_a);
    uint32_t b = sol__div255((uint32_t)SOL_COLOR_B(src) * sa + (uint32_t)SOL_COLOR_B(dst) * inv_a);
    uint32_t a = sa + sol__div255(da * inv_a);

    return SOL_ARGB(a, r, g, b);
}

/* Overflow-safe integer math helpers */
static inline bool sol__add_overflow_i32(int32_t a, int32_t b, int32_t *out) {
    int64_t sum = (int64_t)a + (int64_t)b;
    if (sum < (int64_t)INT32_MIN || sum > (int64_t)INT32_MAX) {
        return true;
    }
    *out = (int32_t)sum;
    return false;
}

static inline bool sol__mul_overflow_sz(size_t a, size_t b, size_t *out) {
    if (a != 0u && b > ((size_t)-1) / a) {
        return true;
    }
    *out = a * b;
    return false;
}

/* Fixed-point 16.16 and 24.8 math helpers */
typedef int32_t sol_fx16_t;
typedef int32_t sol_fx24_8_t;

#define SOL_FX16_ONE ((sol_fx16_t)(1 << 16))
#define SOL_FX16_FROM_INT(x) ((sol_fx16_t)((int32_t)(x) * SOL_FX16_ONE))
#define SOL_FX16_TO_INT(x) ((int32_t)((x) >> 16))

static inline sol_fx16_t sol__fx16_mul(sol_fx16_t a, sol_fx16_t b) {
    return (sol_fx16_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline sol_fx16_t sol__fx16_div(sol_fx16_t a, sol_fx16_t b) {
    if (b == 0) {
        return 0;
    }
    return (sol_fx16_t)((((int64_t)a) << 16) / (int64_t)b);
}

#define SOL_FX24_8_ONE ((sol_fx24_8_t)(1 << 8))
#define SOL_FX24_8_FROM_INT(x) ((sol_fx24_8_t)((int32_t)(x) * SOL_FX24_8_ONE))
#define SOL_FX24_8_TO_INT(x) ((int32_t)((x) >> 8))

static inline sol_fx24_8_t sol__fx24_8_mul(sol_fx24_8_t a, sol_fx24_8_t b) {
    return (sol_fx24_8_t)(((int64_t)a * (int64_t)b) >> 8);
}

/* Exact 64-bit integer square root: floor(sqrt(n)) */
static inline uint32_t sol__isqrt64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = (uint64_t)1 << 62;

    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)res;
}

static inline void sol__plot_cov(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color,
                                 uint8_t cov) {
    uint32_t sa;
    size_t idx;

    if (cov == 0u) {
        return;
    }
    if (x < surf->clip.x || y < surf->clip.y || x >= surf->clip.x + surf->clip.w ||
        y >= surf->clip.y + surf->clip.h) {
        return;
    }

    sa = sol__div255((uint32_t)SOL_COLOR_A(color) * (uint32_t)cov);
    if (sa == 0u) {
        return;
    }

    idx = (size_t)y * (size_t)surf->stride + (size_t)x;
    surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], (color & 0x00FFFFFFu) | (sa << 24));
}

static inline void sol__hline_solid_clipped(sol_surface_t *surf, int32_t x0, int32_t x1, int32_t y,
                                            sol_color_t color) {
    uint32_t sa = SOL_COLOR_A(color);
    uint32_t *dst;
    int32_t count;
    int32_t i;

    if (sa == 0u || x0 > x1) {
        return;
    }
    if (y < surf->clip.y || y >= surf->clip.y + surf->clip.h) {
        return;
    }
    if (x0 < surf->clip.x) {
        x0 = surf->clip.x;
    }
    if (x1 >= surf->clip.x + surf->clip.w) {
        x1 = surf->clip.x + surf->clip.w - 1;
    }
    if (x0 > x1) {
        return;
    }

    dst = surf->pixels + (size_t)y * (size_t)surf->stride + (size_t)x0;
    count = x1 - x0 + 1;
    if (sa == 255u) {
        for (i = 0; i < count; ++i) {
            dst[i] = color;
        }
    } else {
        for (i = 0; i < count; ++i) {
            dst[i] = sol__blend_argb_fast(dst[i], color);
        }
    }
}

/* Signed distance in 1/256 px units from point (px,py) to rounded box [bx0..bx1, by0..by1] */
static inline int32_t sol__sdf_rounded_box_fp8(int32_t px, int32_t py, int32_t bx0, int32_t by0,
                                               int32_t bx1, int32_t by1, int32_t r) {
    /* Distance to inner rectangle [bx0+r .. bx1-r] x [by0+r .. by1-r] in 1/256 px (pixel center
     * +0.5) */
    int64_t cx2 = (int64_t)px * 2 + 1;
    int64_t cy2 = (int64_t)py * 2 + 1;
    int64_t ix0 = (int64_t)(bx0 + r) * 2;
    int64_t ix1 = (int64_t)(bx1 - r) * 2;
    int64_t iy0 = (int64_t)(by0 + r) * 2;
    int64_t iy1 = (int64_t)(by1 - r) * 2;
    int64_t qx = 0;
    int64_t qy = 0;
    int64_t r_fp8 = (int64_t)r * 256;

    if (cx2 < ix0) {
        qx = (ix0 - cx2) * 128; /* (ix0 - cx2) / 2 * 256 */
    } else if (cx2 > ix1) {
        qx = (cx2 - ix1) * 128;
    }

    if (cy2 < iy0) {
        qy = (iy0 - cy2) * 128;
    } else if (cy2 > iy1) {
        qy = (cy2 - iy1) * 128;
    }

    if (qx > 0 && qy > 0) {
        uint64_t d =
            (uint64_t)sol__isqrt64((uint64_t)qx * (uint64_t)qx + (uint64_t)qy * (uint64_t)qy);
        return (int32_t)((int64_t)d - r_fp8);
    }

    if (qx > 0 || qy > 0) {
        return (int32_t)(SOL_MAX(qx, qy) - r_fp8);
    }

    /* Strictly inside inner core: negative distance to nearest edge */
    {
        int64_t dx_in = SOL_MIN(cx2 - (int64_t)bx0 * 2, (int64_t)bx1 * 2 - cx2) * 128;
        int64_t dy_in = SOL_MIN(cy2 - (int64_t)by0 * 2, (int64_t)by1 * 2 - cy2) * 128;
        return (int32_t)(-SOL_MIN(dx_in, dy_in));
    }
}

int32_t sol__sin_deg_fp16(int32_t deg);
int32_t sol__cos_deg_fp16(int32_t deg);

#define SOL_IMG_MAX_DIM 16384

uint32_t *sol__img_alloc_pixels(sol_image_t *img, int32_t w, int32_t h, sol_arena_t *arena);
void *sol__tmp_alloc(size_t size, sol_arena_t *arena);
void sol__tmp_free(void *ptr, const sol_arena_t *arena);

#endif /* SOL_INTERNAL_H */
