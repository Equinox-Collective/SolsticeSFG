#include "sol_internal.h"

sol_color_t sol_color_with_alpha(sol_color_t color, uint8_t alpha) {
    return (color & 0x00FFFFFFu) | (((sol_color_t)alpha) << 24);
}

sol_color_t sol_color_lerp(sol_color_t a, sol_color_t b, uint8_t t) {
    uint32_t wt = (uint32_t)t;
    uint32_t inv_t = 255u - wt;

    uint32_t out_a = sol__div255((uint32_t)SOL_COLOR_A(a) * inv_t + (uint32_t)SOL_COLOR_A(b) * wt);
    uint32_t out_r = sol__div255((uint32_t)SOL_COLOR_R(a) * inv_t + (uint32_t)SOL_COLOR_R(b) * wt);
    uint32_t out_g = sol__div255((uint32_t)SOL_COLOR_G(a) * inv_t + (uint32_t)SOL_COLOR_G(b) * wt);
    uint32_t out_b = sol__div255((uint32_t)SOL_COLOR_B(a) * inv_t + (uint32_t)SOL_COLOR_B(b) * wt);

    return SOL_ARGB(out_a, out_r, out_g, out_b);
}

sol_color_t sol_color_blend(sol_color_t dst, sol_color_t src) {
    return sol__blend_argb_fast(dst, src);
}

void sol_blend_pixel(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color) {
    size_t idx;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (x < surf->clip.x || y < surf->clip.y || x >= surf->clip.x + surf->clip.w ||
        y >= surf->clip.y + surf->clip.h) {
        return;
    }

    idx = (size_t)y * (size_t)surf->stride + (size_t)x;
    surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], color);
}
