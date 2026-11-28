#include "sol_internal.h"

#define SOL__MAX_BLUR_RADIUS 64

SOL_PRIVATE void sol__stackblur_span(uint32_t *base, int32_t len, int32_t step, int32_t rad) {
    uint32_t stack[SOL__MAX_BLUR_RADIUS * 2 + 1];
    int32_t div = rad * 2 + 1;
    uint32_t divsum = (uint32_t)(rad + 1) * (uint32_t)(rad + 1);
    uint32_t sum_a = 0u;
    uint32_t sum_r = 0u;
    uint32_t sum_g = 0u;
    uint32_t sum_b = 0u;
    uint32_t in_a = 0u;
    uint32_t in_r = 0u;
    uint32_t in_g = 0u;
    uint32_t in_b = 0u;
    uint32_t out_a = 0u;
    uint32_t out_r = 0u;
    uint32_t out_g = 0u;
    uint32_t out_b = 0u;
    int32_t stack_ptr = rad;
    int32_t i;
    int32_t pos;

    if (len <= 1) {
        return;
    }

    for (i = -rad; i <= rad; ++i) {
        int32_t s = (i < 0) ? 0 : ((i >= len) ? (len - 1) : i);
        uint32_t p = base[(size_t)s * (size_t)step];
        uint32_t w = (uint32_t)(rad + 1 - (i < 0 ? -i : i));
        uint32_t pa = SOL_COLOR_A(p);
        uint32_t pr = SOL_COLOR_R(p);
        uint32_t pg = SOL_COLOR_G(p);
        uint32_t pb = SOL_COLOR_B(p);

        stack[i + rad] = p;
        sum_a += pa * w;
        sum_r += pr * w;
        sum_g += pg * w;
        sum_b += pb * w;

        if (i > 0) {
            in_a += pa;
            in_r += pr;
            in_g += pg;
            in_b += pb;
        } else {
            out_a += pa;
            out_r += pr;
            out_g += pg;
            out_b += pb;
        }
    }

    for (pos = 0; pos < len; ++pos) {
        uint32_t nxt_c;
        uint32_t old_c;
        int32_t next_pos;
        int32_t stack_start;

        base[(size_t)pos * (size_t)step] =
            SOL_ARGB(sum_a / divsum, sum_r / divsum, sum_g / divsum, sum_b / divsum);

        sum_a -= out_a;
        sum_r -= out_r;
        sum_g -= out_g;
        sum_b -= out_b;

        stack_start = stack_ptr - rad + div;
        if (stack_start >= div) {
            stack_start -= div;
        }

        old_c = stack[stack_start];
        out_a -= SOL_COLOR_A(old_c);
        out_r -= SOL_COLOR_R(old_c);
        out_g -= SOL_COLOR_G(old_c);
        out_b -= SOL_COLOR_B(old_c);

        next_pos = pos + rad + 1;
        if (next_pos >= len) {
            next_pos = len - 1;
        }
        nxt_c = base[(size_t)next_pos * (size_t)step];
        stack[stack_start] = nxt_c;

        in_a += SOL_COLOR_A(nxt_c);
        in_r += SOL_COLOR_R(nxt_c);
        in_g += SOL_COLOR_G(nxt_c);
        in_b += SOL_COLOR_B(nxt_c);

        sum_a += in_a;
        sum_r += in_r;
        sum_g += in_g;
        sum_b += in_b;

        stack_ptr++;
        if (stack_ptr >= div) {
            stack_ptr = 0;
        }
        {
            uint32_t mid_c = stack[stack_ptr];
            out_a += SOL_COLOR_A(mid_c);
            out_r += SOL_COLOR_R(mid_c);
            out_g += SOL_COLOR_G(mid_c);
            out_b += SOL_COLOR_B(mid_c);
            in_a -= SOL_COLOR_A(mid_c);
            in_r -= SOL_COLOR_R(mid_c);
            in_g -= SOL_COLOR_G(mid_c);
            in_b -= SOL_COLOR_B(mid_c);
        }
    }
}

void sol_blur_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius) {
    sol_rect_t r;
    int32_t rad;
    int32_t x;
    int32_t y;

    if (surf == NULL || surf->pixels == NULL || radius <= 0) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 1 || r.h <= 1) {
        return;
    }

    rad = (radius > SOL__MAX_BLUR_RADIUS) ? SOL__MAX_BLUR_RADIUS : radius;

    for (y = 0; y < r.h; ++y) {
        sol__stackblur_span(surf->pixels + (size_t)(r.y + y) * (size_t)surf->stride + (size_t)r.x,
                            r.w, 1, rad);
    }
    for (x = 0; x < r.w; ++x) {
        sol__stackblur_span(surf->pixels + (size_t)r.y * (size_t)surf->stride + (size_t)(r.x + x),
                            r.h, surf->stride, rad);
    }
}

void sol_blur_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t corner_radius,
                           int32_t blur_radius) {
    sol_rect_t r;
    int32_t cr;
    int32_t rad;
    int32_t row;
    int32_t col;
    uint64_t cr2;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 1 || rect.h <= 1 || blur_radius <= 0) {
        return;
    }
    if (corner_radius <= 0) {
        sol_blur_rect(surf, rect, blur_radius);
        return;
    }

    cr = SOL_MIN(corner_radius, SOL_MIN(rect.w, rect.h) / 2);
    if (cr <= 0) {
        sol_blur_rect(surf, rect, blur_radius);
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 1 || r.h <= 1) {
        return;
    }

    rad = (blur_radius > SOL__MAX_BLUR_RADIUS) ? SOL__MAX_BLUR_RADIUS : blur_radius;
    cr2 = (uint64_t)cr * (uint64_t)cr;

    /* Pass 1: Horizontal spans trimmed to rounded corners */
    for (row = r.y; row < r.y + r.h; ++row) {
        int32_t ly = row - rect.y;
        int32_t inset = 0;

        if (ly < cr) {
            int64_t dy = (int64_t)cr - (int64_t)ly;
            inset = cr - (int32_t)sol__isqrt64(cr2 - (uint64_t)(dy * dy));
        } else if (ly >= rect.h - cr) {
            int64_t dy = (int64_t)ly - (int64_t)(rect.h - 1 - cr);
            inset = cr - (int32_t)sol__isqrt64(cr2 - (uint64_t)(dy * dy));
        }

        {
            int32_t x0 = (int32_t)SOL_MAX((int64_t)rect.x + inset, (int64_t)r.x);
            int32_t x1 =
                (int32_t)SOL_MIN((int64_t)rect.x + rect.w - 1 - inset, (int64_t)r.x + r.w - 1);
            if (x1 > x0) {
                sol__stackblur_span(surf->pixels + (size_t)row * (size_t)surf->stride + (size_t)x0,
                                    x1 - x0 + 1, 1, rad);
            }
        }
    }

    /* Pass 2: Vertical spans trimmed to rounded corners */
    for (col = r.x; col < r.x + r.w; ++col) {
        int32_t lx = col - rect.x;
        int32_t inset = 0;

        if (lx < cr) {
            int64_t dx = (int64_t)cr - (int64_t)lx;
            inset = cr - (int32_t)sol__isqrt64(cr2 - (uint64_t)(dx * dx));
        } else if (lx >= rect.w - cr) {
            int64_t dx = (int64_t)lx - (int64_t)(rect.w - 1 - cr);
            inset = cr - (int32_t)sol__isqrt64(cr2 - (uint64_t)(dx * dx));
        }

        {
            int32_t y0 = (int32_t)SOL_MAX((int64_t)rect.y + inset, (int64_t)r.y);
            int32_t y1 =
                (int32_t)SOL_MIN((int64_t)rect.y + rect.h - 1 - inset, (int64_t)r.y + r.h - 1);
            if (y1 > y0) {
                sol__stackblur_span(surf->pixels + (size_t)y0 * (size_t)surf->stride + (size_t)col,
                                    y1 - y0 + 1, surf->stride, rad);
            }
        }
    }
}

void sol_draw_box_shadow(sol_surface_t *surf, sol_rect_t rect, int32_t corner_radius,
                         int32_t offset_x, int32_t offset_y, int32_t blur_radius, int32_t spread,
                         sol_color_t color) {
    sol_rect_t box;
    sol_rect_t outer;
    sol_rect_t clip_r;
    int32_t rad;
    int32_t blur;
    int64_t blur_fp8;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    box = sol_rect_inset(SOL_RECT(rect.x + offset_x, rect.y + offset_y, rect.w, rect.h), -spread,
                         -spread);
    if (box.w <= 0 || box.h <= 0) {
        return;
    }

    blur = (blur_radius < 0) ? 0 : blur_radius;
    rad = SOL_MIN(SOL_MAX(corner_radius + spread, 0), SOL_MIN(box.w, box.h) / 2);
    if (blur == 0) {
        sol_fill_rounded_rect_aa(surf, box, rad, color);
        return;
    }

    outer = SOL_RECT(box.x - blur, box.y - blur, box.w + blur * 2, box.h + blur * 2);
    clip_r = sol_rect_intersect(outer, surf->clip);
    if (clip_r.w <= 0 || clip_r.h <= 0) {
        return;
    }

    blur_fp8 = (int64_t)blur * 256;

    {
        int32_t core_x0 = box.x + SOL_MAX(rad, blur);
        int32_t core_x1 = box.x + box.w - 1 - SOL_MAX(rad, blur);
        int32_t core_y0 = box.y + SOL_MAX(rad, blur);
        int32_t core_y1 = box.y + box.h - 1 - SOL_MAX(rad, blur);
        int32_t row;
        int32_t col;

        for (row = clip_r.y; row < clip_r.y + clip_r.h; ++row) {
            bool in_core_y = (core_x0 <= core_x1) && (row >= core_y0 && row <= core_y1);

            for (col = clip_r.x; col < clip_r.x + clip_r.w; ++col) {
                if (in_core_y && col == core_x0) {
                    int32_t span_end = SOL_MIN(core_x1, clip_r.x + clip_r.w - 1);
                    sol__hline_solid_clipped(surf, core_x0, span_end, row, color);
                    col = span_end;
                    continue;
                }

                {
                    int32_t sd = sol__sdf_rounded_box_fp8(col, row, box.x, box.y, box.x + box.w,
                                                          box.y + box.h, rad);
                    if (sd <= -blur_fp8) {
                        sol__plot_cov(surf, col, row, color, 255u);
                    } else if (sd < blur_fp8) {
                        /* Smooth cubic S-curve falloff across [-blur, +blur] */
                        uint64_t t256 =
                            (uint64_t)((blur_fp8 - sd) * 256) / (uint64_t)(2 * blur_fp8);
                        if (t256 > 256u) {
                            t256 = 256u;
                        }
                        uint64_t smooth = (t256 * t256 * (768u - 2u * t256)) >> 16;
                        uint8_t cov = (smooth >= 255u) ? 255u : (uint8_t)smooth;
                        sol__plot_cov(surf, col, row, color, cov);
                    }
                }
            }
        }
    }
}

void sol_apply_noise(sol_surface_t *surf, sol_rect_t rect, uint8_t amount, uint32_t seed) {
    sol_rect_t r;
    int32_t row;
    int32_t col;
    uint32_t state = (seed == 0u) ? 0x9E3779B9u : seed;

    if (surf == NULL || surf->pixels == NULL || amount == 0u) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = 0; row < r.h; ++row) {
        uint32_t *dst = surf->pixels + (size_t)(r.y + row) * (size_t)surf->stride + (size_t)r.x;
        for (col = 0; col < r.w; ++col) {
            int32_t jitter;
            uint32_t c = dst[col];
            int32_t cr;
            int32_t cg;
            int32_t cb;

            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            jitter = (int32_t)(state % (uint32_t)(amount * 2u + 1u)) - (int32_t)amount;
            cr = (int32_t)SOL_COLOR_R(c) + jitter;
            cg = (int32_t)SOL_COLOR_G(c) + jitter;
            cb = (int32_t)SOL_COLOR_B(c) + jitter;

            cr = (cr < 0) ? 0 : ((cr > 255) ? 255 : cr);
            cg = (cg < 0) ? 0 : ((cg > 255) ? 255 : cg);
            cb = (cb < 0) ? 0 : ((cb > 255) ? 255 : cb);

            dst[col] = SOL_ARGB(SOL_COLOR_A(c), cr, cg, cb);
        }
    }
}
