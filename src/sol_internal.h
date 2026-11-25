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

#endif /* SOL_INTERNAL_H */
