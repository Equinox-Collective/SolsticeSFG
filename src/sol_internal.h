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

#endif /* SOL_INTERNAL_H */
