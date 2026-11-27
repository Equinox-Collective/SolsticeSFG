#include "sol_internal.h"

/* Unchecked span plotter: caller guarantees y and [x0, x1] are inside surf->clip */
SOL_PRIVATE void sol__draw_hline_clipped(sol_surface_t *surf, int32_t x0, int32_t x1, int32_t y,
                                         sol_color_t color) {
    uint32_t *dst;
    int32_t count;
    int32_t i;
    uint32_t sa = SOL_COLOR_A(color);

    if (sa == 0u || x0 > x1) {
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

SOL_PRIVATE void sol__draw_hline_span(sol_surface_t *surf, int32_t x0, int32_t x1, int32_t y,
                                      sol_color_t color) {
    int32_t clip_x1;
    int32_t clip_y1;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    if (x0 > x1) {
        int32_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    clip_x1 = surf->clip.x + surf->clip.w - 1;
    clip_y1 = surf->clip.y + surf->clip.h - 1;

    if (y < surf->clip.y || y > clip_y1 || x1 < surf->clip.x || x0 > clip_x1) {
        return;
    }

    if (x0 < surf->clip.x) {
        x0 = surf->clip.x;
    }
    if (x1 > clip_x1) {
        x1 = clip_x1;
    }

    sol__draw_hline_clipped(surf, x0, x1, y, color);
}

SOL_PRIVATE void sol__draw_plot(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color) {
    if (x < surf->clip.x || y < surf->clip.y || x >= surf->clip.x + surf->clip.w ||
        y >= surf->clip.y + surf->clip.h) {
        return;
    }

    size_t idx = (size_t)y * (size_t)surf->stride + (size_t)x;
    surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], color);
}

void sol_draw_hline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len, sol_color_t color) {
    if (len <= 0) {
        return;
    }
    sol__draw_hline_span(surf, x, (int32_t)((int64_t)x + (int64_t)len - 1), y, color);
}

void sol_draw_vline(sol_surface_t *surf, int32_t x, int32_t y, int32_t len, sol_color_t color) {
    int32_t y0;
    int32_t y1;
    int32_t clip_x1;
    int32_t clip_y1;
    int32_t row;
    uint32_t sa;

    if (surf == NULL || surf->pixels == NULL || len <= 0) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    sa = SOL_COLOR_A(color);
    if (sa == 0u) {
        return;
    }

    clip_x1 = surf->clip.x + surf->clip.w - 1;
    clip_y1 = surf->clip.y + surf->clip.h - 1;
    if (x < surf->clip.x || x > clip_x1) {
        return;
    }

    y0 = y;
    y1 = (int32_t)SOL_MIN((int64_t)y + (int64_t)len - 1, (int64_t)clip_y1);
    if (y0 < surf->clip.y) {
        y0 = surf->clip.y;
    }
    if (y0 > y1) {
        return;
    }

    if (sa == 255u) {
        for (row = y0; row <= y1; ++row) {
            surf->pixels[(size_t)row * (size_t)surf->stride + (size_t)x] = color;
        }
    } else {
        for (row = y0; row <= y1; ++row) {
            size_t idx = (size_t)row * (size_t)surf->stride + (size_t)x;
            surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], color);
        }
    }
}

/* Cohen-Sutherland outcodes for line clipping */
#define SOL__OC_INSIDE 0
#define SOL__OC_LEFT 1
#define SOL__OC_RIGHT 2
#define SOL__OC_BOTTOM 4
#define SOL__OC_TOP 8

SOL_PRIVATE int sol__outcode(int64_t x, int64_t y, int64_t xmin, int64_t ymin, int64_t xmax,
                             int64_t ymax) {
    int code = SOL__OC_INSIDE;
    if (x < xmin) {
        code |= SOL__OC_LEFT;
    } else if (x > xmax) {
        code |= SOL__OC_RIGHT;
    }
    if (y < ymin) {
        code |= SOL__OC_TOP;
    } else if (y > ymax) {
        code |= SOL__OC_BOTTOM;
    }
    return code;
}

void sol_draw_line(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                   sol_color_t color) {
    int64_t xmin;
    int64_t ymin;
    int64_t xmax;
    int64_t ymax;
    int64_t lx0 = x0;
    int64_t ly0 = y0;
    int64_t lx1 = x1;
    int64_t ly1 = y1;
    int oc0;
    int oc1;
    bool accept = false;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }

    xmin = surf->clip.x;
    ymin = surf->clip.y;
    xmax = (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1;
    ymax = (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1;

    oc0 = sol__outcode(lx0, ly0, xmin, ymin, xmax, ymax);
    oc1 = sol__outcode(lx1, ly1, xmin, ymin, xmax, ymax);

    for (;;) {
        if ((oc0 | oc1) == 0) {
            accept = true;
            break;
        }
        if ((oc0 & oc1) != 0) {
            break;
        }

        int out = oc0 != 0 ? oc0 : oc1;
        int64_t x = 0;
        int64_t y = 0;

        if ((out & SOL__OC_BOTTOM) != 0) {
            x = lx0 + (lx1 - lx0) * (ymax - ly0) / (ly1 - ly0);
            y = ymax;
        } else if ((out & SOL__OC_TOP) != 0) {
            x = lx0 + (lx1 - lx0) * (ymin - ly0) / (ly1 - ly0);
            y = ymin;
        } else if ((out & SOL__OC_RIGHT) != 0) {
            y = ly0 + (ly1 - ly0) * (xmax - lx0) / (lx1 - lx0);
            x = xmax;
        } else if ((out & SOL__OC_LEFT) != 0) {
            y = ly0 + (ly1 - ly0) * (xmin - lx0) / (lx1 - lx0);
            x = xmin;
        }

        if (out == oc0) {
            lx0 = x;
            ly0 = y;
            oc0 = sol__outcode(lx0, ly0, xmin, ymin, xmax, ymax);
        } else {
            lx1 = x;
            ly1 = y;
            oc1 = sol__outcode(lx1, ly1, xmin, ymin, xmax, ymax);
        }
    }

    if (!accept) {
        return;
    }

    if (ly0 == ly1) {
        sol__draw_hline_span(surf, (int32_t)lx0, (int32_t)lx1, (int32_t)ly0, color);
        return;
    }
    if (lx0 == lx1) {
        int32_t top = (int32_t)SOL_MIN(ly0, ly1);
        int32_t bot = (int32_t)SOL_MAX(ly0, ly1);
        sol_draw_vline(surf, (int32_t)lx0, top, bot - top + 1, color);
        return;
    }

    {
        int32_t cx = (int32_t)lx0;
        int32_t cy = (int32_t)ly0;
        int32_t ex = (int32_t)lx1;
        int32_t ey = (int32_t)ly1;
        int32_t dx = ex > cx ? (ex - cx) : (cx - ex);
        int32_t dy = ey > cy ? (ey - cy) : (cy - ey);
        int32_t sx = cx < ex ? 1 : -1;
        int32_t sy = cy < ey ? 1 : -1;
        int32_t err = dx - dy;

        for (;;) {
            size_t idx = (size_t)cy * (size_t)surf->stride + (size_t)cx;
            surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], color);

            if (cx == ex && cy == ey) {
                break;
            }
            int32_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                cx += sx;
            }
            if (e2 < dx) {
                err += dx;
                cy += sy;
            }
        }
    }
}

void sol_fill_rect(sol_surface_t *surf, sol_rect_t rect, sol_color_t color) {
    sol_rect_t r;
    int32_t row;

    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = 0; row < r.h; ++row) {
        sol__draw_hline_clipped(surf, r.x, r.x + r.w - 1, r.y + row, color);
    }
}

void sol_draw_rect(sol_surface_t *surf, sol_rect_t rect, sol_color_t color) {
    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0) {
        return;
    }
    if (rect.h == 1) {
        sol_draw_hline(surf, rect.x, rect.y, rect.w, color);
        return;
    }
    if (rect.w == 1) {
        sol_draw_vline(surf, rect.x, rect.y, rect.h, color);
        return;
    }

    sol_draw_hline(surf, rect.x, rect.y, rect.w, color);
    sol_draw_hline(surf, rect.x, rect.y + rect.h - 1, rect.w, color);
    if (rect.h > 2) {
        sol_draw_vline(surf, rect.x, rect.y + 1, rect.h - 2, color);
        sol_draw_vline(surf, rect.x + rect.w - 1, rect.y + 1, rect.h - 2, color);
    }
}

void sol_draw_rect_thick(sol_surface_t *surf, sol_rect_t rect, int32_t thickness,
                         sol_color_t color) {
    int32_t t = thickness;
    sol_rect_t top_r;
    sol_rect_t bot_r;
    sol_rect_t left_r;
    sol_rect_t right_r;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 || t <= 0) {
        return;
    }
    if (t * 2 >= rect.w || t * 2 >= rect.h) {
        sol_fill_rect(surf, rect, color);
        return;
    }

    top_r = SOL_RECT(rect.x, rect.y, rect.w, t);
    bot_r = SOL_RECT(rect.x, rect.y + rect.h - t, rect.w, t);
    left_r = SOL_RECT(rect.x, rect.y + t, t, rect.h - 2 * t);
    right_r = SOL_RECT(rect.x + rect.w - t, rect.y + t, t, rect.h - 2 * t);

    sol_fill_rect(surf, top_r, color);
    sol_fill_rect(surf, bot_r, color);
    sol_fill_rect(surf, left_r, color);
    sol_fill_rect(surf, right_r, color);
}

void sol_fill_rect_gradient_v(sol_surface_t *surf, sol_rect_t rect, sol_color_t top,
                              sol_color_t bottom) {
    sol_rect_t r;
    int32_t row;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    if (rect.h == 1) {
        sol__draw_hline_clipped(surf, r.x, r.x + r.w - 1, r.y, top);
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        uint8_t t = (uint8_t)(((int64_t)(row - rect.y) * 255) / (rect.h - 1));
        sol_color_t c = sol_color_lerp(top, bottom, t);
        sol__draw_hline_clipped(surf, r.x, r.x + r.w - 1, row, c);
    }
}

void sol_fill_rect_gradient_h(sol_surface_t *surf, sol_rect_t rect, sol_color_t left,
                              sol_color_t right) {
    sol_rect_t r;
    int32_t col;
    int32_t row;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    if (rect.w == 1) {
        sol_fill_rect(surf, r, left);
        return;
    }

    for (col = r.x; col < r.x + r.w; ++col) {
        uint8_t t = (uint8_t)(((int64_t)(col - rect.x) * 255) / (rect.w - 1));
        sol_color_t c = sol_color_lerp(left, right, t);
        uint32_t sa = SOL_COLOR_A(c);

        if (sa == 0u) {
            continue;
        }
        if (sa == 255u) {
            for (row = r.y; row < r.y + r.h; ++row) {
                surf->pixels[(size_t)row * (size_t)surf->stride + (size_t)col] = c;
            }
        } else {
            for (row = r.y; row < r.y + r.h; ++row) {
                size_t idx = (size_t)row * (size_t)surf->stride + (size_t)col;
                surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], c);
            }
        }
    }
}

void sol_fill_circle(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                     sol_color_t color) {
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    uint64_t r2;

    if (surf == NULL || surf->pixels == NULL || radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }
    if (radius == 0) {
        sol__draw_plot(surf, cx, cy, color);
        return;
    }

    y_min = (int32_t)SOL_MAX((int64_t)cy - (int64_t)radius, (int64_t)surf->clip.y);
    y_max = (int32_t)SOL_MIN((int64_t)cy + (int64_t)radius,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);
    r2 = (uint64_t)radius * (uint64_t)radius;

    /* Per-scanline exact chord guarantees zero overdraw for translucent colors */
    for (y = y_min; y <= y_max; ++y) {
        int64_t dy = (int64_t)y - (int64_t)cy;
        uint64_t dy2 = (uint64_t)(dy * dy);
        int32_t dx = (int32_t)sol__isqrt64(r2 - dy2);
        sol__draw_hline_span(surf, cx - dx, cx + dx, y, color);
    }
}

void sol_draw_circle(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                     sol_color_t color) {
    int32_t x;
    int32_t y;
    int32_t err;

    if (surf == NULL || surf->pixels == NULL || radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (radius == 0) {
        sol__draw_plot(surf, cx, cy, color);
        return;
    }

    x = 0;
    y = radius;
    err = 1 - radius;

    /* Cardinal points (plotted once each) */
    sol__draw_plot(surf, cx, cy - radius, color);
    sol__draw_plot(surf, cx, cy + radius, color);
    sol__draw_plot(surf, cx - radius, cy, color);
    sol__draw_plot(surf, cx + radius, cy, color);

    while (x < y) {
        if (err < 0) {
            err += 2 * x + 3;
        } else {
            err += 2 * (x - y) + 5;
            y--;
        }
        x++;

        if (x > y) {
            break;
        }

        if (x == y) {
            /* 4 diagonal points (plotted once each) */
            sol__draw_plot(surf, cx + x, cy + y, color);
            sol__draw_plot(surf, cx - x, cy + y, color);
            sol__draw_plot(surf, cx + x, cy - y, color);
            sol__draw_plot(surf, cx - x, cy - y, color);
        } else {
            /* 8 distinct octant points */
            sol__draw_plot(surf, cx + x, cy + y, color);
            sol__draw_plot(surf, cx - x, cy + y, color);
            sol__draw_plot(surf, cx + x, cy - y, color);
            sol__draw_plot(surf, cx - x, cy - y, color);
            sol__draw_plot(surf, cx + y, cy + x, color);
            sol__draw_plot(surf, cx - y, cy + x, color);
            sol__draw_plot(surf, cx + y, cy - x, color);
            sol__draw_plot(surf, cx - y, cy - x, color);
        }
    }
}

void sol_fill_ellipse(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                      sol_color_t color) {
    int64_t rx2;
    int64_t ry2;
    int64_t two_rx2;
    int64_t two_ry2;
    int64_t x;
    int64_t y;
    int64_t px;
    int64_t py;
    int64_t p;

    if (surf == NULL || surf->pixels == NULL || rx < 0 || ry < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (rx == 0) {
        sol_draw_vline(surf, cx, cy - ry, 2 * ry + 1, color);
        return;
    }
    if (ry == 0) {
        sol_draw_hline(surf, cx - rx, cy, 2 * rx + 1, color);
        return;
    }
    if (rx == ry) {
        sol_fill_circle(surf, cx, cy, rx, color);
        return;
    }

    rx2 = (int64_t)rx * (int64_t)rx;
    ry2 = (int64_t)ry * (int64_t)ry;
    two_rx2 = 2 * rx2;
    two_ry2 = 2 * ry2;

    x = 0;
    y = ry;
    px = 0;
    py = two_rx2 * y;

    /* Region 1: emit scanline right before y decrements (zero overdraw) */
    p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) {
        x++;
        px += two_ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            sol__draw_hline_span(surf, (int32_t)(cx - (x - 1)), (int32_t)(cx + (x - 1)),
                                 (int32_t)(cy + y), color);
            sol__draw_hline_span(surf, (int32_t)(cx - (x - 1)), (int32_t)(cx + (x - 1)),
                                 (int32_t)(cy - y), color);
            y--;
            py -= two_rx2;
            p += ry2 + px - py;
        }
    }

    /* Region 2: one scanline per y step down to y = 0 */
    p = ry2 * x * x + ry2 * x + (ry2 / 4) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y >= 0) {
        if (y == 0) {
            sol__draw_hline_span(surf, (int32_t)(cx - rx), (int32_t)(cx + rx), cy, color);
            break;
        }

        if (p > 0) {
            sol__draw_hline_span(surf, (int32_t)(cx - x), (int32_t)(cx + x), (int32_t)(cy + y),
                                 color);
            sol__draw_hline_span(surf, (int32_t)(cx - x), (int32_t)(cx + x), (int32_t)(cy - y),
                                 color);
            y--;
            py -= two_rx2;
            p += rx2 - py;
        } else {
            x++;
            px += two_ry2;
            sol__draw_hline_span(surf, (int32_t)(cx - (x - 1)), (int32_t)(cx + (x - 1)),
                                 (int32_t)(cy + y), color);
            sol__draw_hline_span(surf, (int32_t)(cx - (x - 1)), (int32_t)(cx + (x - 1)),
                                 (int32_t)(cy - y), color);
            y--;
            py -= two_rx2;
            p += rx2 - py + px;
        }
    }
}

void sol_draw_ellipse(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                      sol_color_t color) {
    int64_t rx2;
    int64_t ry2;
    int64_t two_rx2;
    int64_t two_ry2;
    int64_t x;
    int64_t y;
    int64_t px;
    int64_t py;
    int64_t p;

    if (surf == NULL || surf->pixels == NULL || rx < 0 || ry < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (rx == 0) {
        sol_draw_vline(surf, cx, cy - ry, 2 * ry + 1, color);
        return;
    }
    if (ry == 0) {
        sol_draw_hline(surf, cx - rx, cy, 2 * rx + 1, color);
        return;
    }
    if (rx == ry) {
        sol_draw_circle(surf, cx, cy, rx, color);
        return;
    }

    rx2 = (int64_t)rx * (int64_t)rx;
    ry2 = (int64_t)ry * (int64_t)ry;
    two_rx2 = 2 * rx2;
    two_ry2 = 2 * ry2;

    x = 0;
    y = ry;
    px = 0;
    py = two_rx2 * y;

    /* Region 1 */
    p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) {
        if (x == 0) {
            sol__draw_plot(surf, cx, (int32_t)(cy + y), color);
            sol__draw_plot(surf, cx, (int32_t)(cy - y), color);
        } else {
            sol__draw_plot(surf, (int32_t)(cx + x), (int32_t)(cy + y), color);
            sol__draw_plot(surf, (int32_t)(cx - x), (int32_t)(cy + y), color);
            sol__draw_plot(surf, (int32_t)(cx + x), (int32_t)(cy - y), color);
            sol__draw_plot(surf, (int32_t)(cx - x), (int32_t)(cy - y), color);
        }

        x++;
        px += two_ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= two_rx2;
            p += ry2 + px - py;
        }
    }

    /* Region 2 */
    p = ry2 * x * x + ry2 * x + (ry2 / 4) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y >= 0) {
        if (y == 0) {
            sol__draw_plot(surf, (int32_t)(cx + x), cy, color);
            sol__draw_plot(surf, (int32_t)(cx - x), cy, color);
        } else {
            sol__draw_plot(surf, (int32_t)(cx + x), (int32_t)(cy + y), color);
            sol__draw_plot(surf, (int32_t)(cx - x), (int32_t)(cy + y), color);
            sol__draw_plot(surf, (int32_t)(cx + x), (int32_t)(cy - y), color);
            sol__draw_plot(surf, (int32_t)(cx - x), (int32_t)(cy - y), color);
        }

        y--;
        py -= two_rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += two_ry2;
            p += rx2 - py + px;
        }
    }
}

void sol_fill_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                           sol_color_t color) {
    int32_t r;
    int32_t min_dim;
    int32_t dy;
    uint64_t r2;
    int32_t left_cx;
    int32_t right_cx;
    int32_t top_cy;
    int32_t bot_cy;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    if (radius <= 0) {
        sol_fill_rect(surf, rect, color);
        return;
    }

    min_dim = SOL_MIN(rect.w, rect.h);
    r = SOL_MIN(radius, min_dim / 2);
    if (r <= 0) {
        sol_fill_rect(surf, rect, color);
        return;
    }

    /* Center flat body (non-overlapping with corner arcs) */
    if (rect.h > 2 * r) {
        sol_fill_rect(surf, SOL_RECT(rect.x, rect.y + r, rect.w, rect.h - 2 * r), color);
    }

    left_cx = rect.x + r;
    right_cx = rect.x + rect.w - 1 - r;
    top_cy = rect.y + r;
    bot_cy = rect.y + rect.h - 1 - r;
    r2 = (uint64_t)r * (uint64_t)r;

    /* Top and bottom rounded bands (dy = 1..r away from corner centers) */
    for (dy = 1; dy <= r; ++dy) {
        int32_t dx = (int32_t)sol__isqrt64(r2 - (uint64_t)dy * (uint64_t)dy);
        sol__draw_hline_span(surf, left_cx - dx, right_cx + dx, top_cy - dy, color);
        sol__draw_hline_span(surf, left_cx - dx, right_cx + dx, bot_cy + dy, color);
    }
}

void sol_draw_rounded_rect(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                           sol_color_t color) {
    int32_t r;
    int32_t min_dim;
    int32_t left_cx;
    int32_t right_cx;
    int32_t top_cy;
    int32_t bot_cy;
    int32_t x;
    int32_t y;
    int32_t err;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    if (radius <= 0) {
        sol_draw_rect(surf, rect, color);
        return;
    }

    min_dim = SOL_MIN(rect.w, rect.h);
    r = SOL_MIN(radius, min_dim / 2);
    if (r <= 0) {
        sol_draw_rect(surf, rect, color);
        return;
    }

    left_cx = rect.x + r;
    right_cx = rect.x + rect.w - 1 - r;
    top_cy = rect.y + r;
    bot_cy = rect.y + rect.h - 1 - r;

    /* Straight edge segments (between corner tangents) */
    if (rect.w > 2 * r) {
        sol_draw_hline(surf, left_cx, rect.y, rect.w - 2 * r, color);
        sol_draw_hline(surf, left_cx, rect.y + rect.h - 1, rect.w - 2 * r, color);
    }
    if (rect.h > 2 * r) {
        sol_draw_vline(surf, rect.x, top_cy, rect.h - 2 * r, color);
        sol_draw_vline(surf, rect.x + rect.w - 1, top_cy, rect.h - 2 * r, color);
    }

    /* Four quarter-circle corner arcs */
    x = 0;
    y = r;
    err = 1 - r;

    while (x < y) {
        if (err < 0) {
            err += 2 * x + 3;
        } else {
            err += 2 * (x - y) + 5;
            y--;
        }
        x++;

        if (x > y) {
            break;
        }

        if (x == y) {
            sol__draw_plot(surf, left_cx - x, top_cy - y, color);
            sol__draw_plot(surf, right_cx + x, top_cy - y, color);
            sol__draw_plot(surf, left_cx - x, bot_cy + y, color);
            sol__draw_plot(surf, right_cx + x, bot_cy + y, color);
        } else {
            /* Top-left */
            sol__draw_plot(surf, left_cx - x, top_cy - y, color);
            sol__draw_plot(surf, left_cx - y, top_cy - x, color);
            /* Top-right */
            sol__draw_plot(surf, right_cx + x, top_cy - y, color);
            sol__draw_plot(surf, right_cx + y, top_cy - x, color);
            /* Bottom-left */
            sol__draw_plot(surf, left_cx - x, bot_cy + y, color);
            sol__draw_plot(surf, left_cx - y, bot_cy + x, color);
            /* Bottom-right */
            sol__draw_plot(surf, right_cx + x, bot_cy + y, color);
            sol__draw_plot(surf, right_cx + y, bot_cy + x, color);
        }
    }
}

void sol_draw_triangle(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       int32_t x2, int32_t y2, sol_color_t color) {
    sol_draw_line(surf, x0, y0, x1, y1, color);
    sol_draw_line(surf, x1, y1, x2, y2, color);
    sol_draw_line(surf, x2, y2, x0, y0, color);
}

SOL_PRIVATE void sol__swap_i32(int32_t *a, int32_t *b) {
    int32_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void sol_fill_triangle(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       int32_t x2, int32_t y2, sol_color_t color) {
    int32_t y;

    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    /* Sort vertices ascending by Y: y0 <= y1 <= y2 */
    if (y0 > y1) {
        sol__swap_i32(&y0, &y1);
        sol__swap_i32(&x0, &x1);
    }
    if (y1 > y2) {
        sol__swap_i32(&y1, &y2);
        sol__swap_i32(&x1, &x2);
    }
    if (y0 > y1) {
        sol__swap_i32(&y0, &y1);
        sol__swap_i32(&x0, &x1);
    }

    if (y0 == y2) {
        int32_t min_x = SOL_MIN(x0, SOL_MIN(x1, x2));
        int32_t max_x = SOL_MAX(x0, SOL_MAX(x1, x2));
        sol__draw_hline_span(surf, min_x, max_x, y0, color);
        return;
    }

    /* Upper half: y0 .. (y1 - 1), or y0 .. y1 if flat-bottom (y1 == y2) */
    if (y1 > y0) {
        int64_t total_h = (int64_t)y2 - (int64_t)y0;
        int64_t seg_h = (int64_t)y1 - (int64_t)y0;
        int32_t last_upper = (y1 == y2) ? y1 : (y1 - 1);

        for (y = y0; y <= last_upper; ++y) {
            int64_t dy = (int64_t)y - (int64_t)y0;
            int32_t ax = (int32_t)((int64_t)x0 + ((int64_t)(x2 - x0) * dy) / total_h);
            int32_t bx = (int32_t)((int64_t)x0 + ((int64_t)(x1 - x0) * dy) / seg_h);
            sol__draw_hline_span(surf, ax, bx, y, color);
        }
    }

    /* Lower half: y1 .. y2 */
    if (y2 > y1) {
        int64_t total_h = (int64_t)y2 - (int64_t)y0;
        int64_t seg_h = (int64_t)y2 - (int64_t)y1;

        for (y = y1; y <= y2; ++y) {
            int64_t dy_total = (int64_t)y - (int64_t)y0;
            int64_t dy_seg = (int64_t)y - (int64_t)y1;
            int32_t ax = (int32_t)((int64_t)x0 + ((int64_t)(x2 - x0) * dy_total) / total_h);
            int32_t bx = (int32_t)((int64_t)x1 + ((int64_t)(x2 - x1) * dy_seg) / seg_h);
            sol__draw_hline_span(surf, ax, bx, y, color);
        }
    }
}

SOL_PRIVATE int32_t sol__atan2_deg(int64_t dy, int64_t dx) {
    uint64_t ax = (uint64_t)(dx < 0 ? -dx : dx);
    uint64_t ay = (uint64_t)(dy < 0 ? -dy : dy);
    int32_t base_deg;

    if (ax == 0u && ay == 0u) {
        return 0;
    }

    if (ax >= ay) {
        uint64_t t = (ay * 1024u) / ax;
        base_deg = (int32_t)((45u * t) / 1024u + (t * (1024u - t) * 16u) / (1024u * 1024u));
    } else {
        uint64_t t = (ax * 1024u) / ay;
        base_deg = 90 - (int32_t)((45u * t) / 1024u + (t * (1024u - t) * 16u) / (1024u * 1024u));
    }

    if (dx >= 0 && dy >= 0) {
        return base_deg;
    }
    if (dx < 0 && dy >= 0) {
        return 180 - base_deg;
    }
    if (dx < 0 && dy < 0) {
        return 180 + base_deg;
    }
    return (base_deg == 0) ? 0 : (360 - base_deg);
}

SOL_PRIVATE bool sol__angle_in_range(int32_t deg, int32_t start_deg, int32_t span_deg) {
    int32_t rel = (deg - start_deg) % 360;
    if (rel < 0) {
        rel += 360;
    }
    return rel <= span_deg;
}

SOL_PRIVATE void sol__normalize_corners(sol_corner_radii_t *cr, int32_t w, int32_t h) {
    if (cr->tl < 0) {
        cr->tl = 0;
    }
    if (cr->tr < 0) {
        cr->tr = 0;
    }
    if (cr->br < 0) {
        cr->br = 0;
    }
    if (cr->bl < 0) {
        cr->bl = 0;
    }

    if (cr->tl + cr->tr > w) {
        int32_t s = cr->tl + cr->tr;
        cr->tl = (int32_t)(((int64_t)cr->tl * w) / s);
        cr->tr = w - cr->tl;
    }
    if (cr->bl + cr->br > w) {
        int32_t s = cr->bl + cr->br;
        cr->bl = (int32_t)(((int64_t)cr->bl * w) / s);
        cr->br = w - cr->bl;
    }
    if (cr->tl + cr->bl > h) {
        int32_t s = cr->tl + cr->bl;
        cr->tl = (int32_t)(((int64_t)cr->tl * h) / s);
        cr->bl = h - cr->tl;
    }
    if (cr->tr + cr->br > h) {
        int32_t s = cr->tr + cr->br;
        cr->tr = (int32_t)(((int64_t)cr->tr * h) / s);
        cr->br = h - cr->tr;
    }
}

void sol_fill_rounded_rect_ex(sol_surface_t *surf, sol_rect_t rect, sol_corner_radii_t radii,
                              sol_color_t color) {
    sol_rect_t r;
    int32_t row;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    sol__normalize_corners(&radii, rect.w, rect.h);
    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        int32_t local_y = row - rect.y;
        int32_t left_inset = 0;
        int32_t right_inset = 0;

        if (radii.tl > 0 && local_y < radii.tl) {
            int64_t dy = (int64_t)radii.tl - (int64_t)local_y;
            int32_t dx = (int32_t)sol__isqrt64((uint64_t)radii.tl * (uint64_t)radii.tl -
                                               (uint64_t)(dy * dy));
            left_inset = radii.tl - dx;
        } else if (radii.bl > 0 && local_y >= rect.h - radii.bl) {
            int64_t dy = (int64_t)local_y - (int64_t)(rect.h - 1 - radii.bl);
            int32_t dx = (int32_t)sol__isqrt64((uint64_t)radii.bl * (uint64_t)radii.bl -
                                               (uint64_t)(dy * dy));
            left_inset = radii.bl - dx;
        }

        if (radii.tr > 0 && local_y < radii.tr) {
            int64_t dy = (int64_t)radii.tr - (int64_t)local_y;
            int32_t dx = (int32_t)sol__isqrt64((uint64_t)radii.tr * (uint64_t)radii.tr -
                                               (uint64_t)(dy * dy));
            right_inset = radii.tr - dx;
        } else if (radii.br > 0 && local_y >= rect.h - radii.br) {
            int64_t dy = (int64_t)local_y - (int64_t)(rect.h - 1 - radii.br);
            int32_t dx = (int32_t)sol__isqrt64((uint64_t)radii.br * (uint64_t)radii.br -
                                               (uint64_t)(dy * dy));
            right_inset = radii.br - dx;
        }

        {
            int32_t x0 = (int32_t)SOL_MAX((int64_t)rect.x + left_inset, (int64_t)r.x);
            int32_t x1 = (int32_t)SOL_MIN((int64_t)rect.x + rect.w - 1 - right_inset,
                                          (int64_t)r.x + r.w - 1);
            if (x0 <= x1) {
                sol__draw_hline_clipped(surf, x0, x1, row, color);
            }
        }
    }
}

void sol_draw_line_dashed(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                          int32_t dash_len, int32_t gap_len, sol_color_t color) {
    int64_t dx;
    int64_t dy;
    int32_t sx;
    int32_t sy;
    int64_t err;
    int32_t period;
    int32_t step = 0;

    if (surf == NULL || surf->pixels == NULL || dash_len <= 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (gap_len <= 0) {
        sol_draw_line(surf, x0, y0, x1, y1, color);
        return;
    }

    dx = (int64_t)x1 - (int64_t)x0;
    dy = (int64_t)y1 - (int64_t)y0;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    /* Guard extreme unclipped dashed spans */
    if (dx > 65536 || dy > 65536) {
        sol_draw_line(surf, x0, y0, x1, y1, color);
        return;
    }

    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;
    period = dash_len + gap_len;

    for (;;) {
        if ((step % period) < dash_len) {
            sol__draw_plot(surf, x0, y0, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int64_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
        step++;
    }
}

void sol_draw_arc(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius, int32_t start_deg,
                  int32_t end_deg, sol_color_t color) {
    int32_t span;
    int32_t s_norm;
    int32_t x;
    int32_t y;
    int32_t err;

    if (surf == NULL || surf->pixels == NULL || radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }

    span = end_deg - start_deg;
    if (span <= 0) {
        return;
    }
    if (span >= 360) {
        sol_draw_circle(surf, cx, cy, radius, color);
        return;
    }

    s_norm = start_deg % 360;
    if (s_norm < 0) {
        s_norm += 360;
    }

    if (radius == 0) {
        sol__draw_plot(surf, cx, cy, color);
        return;
    }

    x = 0;
    y = radius;
    err = 1 - radius;

    {
        int32_t pts[4][2] = {{0, -radius}, {0, radius}, {-radius, 0}, {radius, 0}};
        int i;
        for (i = 0; i < 4; ++i) {
            if (sol__angle_in_range(sol__atan2_deg(pts[i][1], pts[i][0]), s_norm, span)) {
                sol__draw_plot(surf, cx + pts[i][0], cy + pts[i][1], color);
            }
        }
    }

    while (x < y) {
        if (err < 0) {
            err += 2 * x + 3;
        } else {
            err += 2 * (x - y) + 5;
            y--;
        }
        x++;
        if (x > y) {
            break;
        }

        {
            int32_t oct[8][2] = {{x, y}, {-x, y}, {x, -y}, {-x, -y},
                                 {y, x}, {-y, x}, {y, -x}, {-y, -x}};
            int count = (x == y) ? 4 : 8;
            int i;
            for (i = 0; i < count; ++i) {
                if (sol__angle_in_range(sol__atan2_deg(oct[i][1], oct[i][0]), s_norm, span)) {
                    sol__draw_plot(surf, cx + oct[i][0], cy + oct[i][1], color);
                }
            }
        }
    }
}

void sol_fill_pie(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius, int32_t start_deg,
                  int32_t end_deg, sol_color_t color) {
    int32_t span;
    int32_t s_norm;
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    uint64_t r2;

    if (surf == NULL || surf->pixels == NULL || radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }

    span = end_deg - start_deg;
    if (span <= 0) {
        return;
    }
    if (span >= 360) {
        sol_fill_circle(surf, cx, cy, radius, color);
        return;
    }

    s_norm = start_deg % 360;
    if (s_norm < 0) {
        s_norm += 360;
    }

    y_min = (int32_t)SOL_MAX((int64_t)cy - (int64_t)radius, (int64_t)surf->clip.y);
    y_max = (int32_t)SOL_MIN((int64_t)cy + (int64_t)radius,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);
    r2 = (uint64_t)radius * (uint64_t)radius;

    for (y = y_min; y <= y_max; ++y) {
        int64_t dy = (int64_t)y - (int64_t)cy;
        int32_t dx_max = (int32_t)sol__isqrt64(r2 - (uint64_t)(dy * dy));
        int32_t x_min = (int32_t)SOL_MAX((int64_t)cx - dx_max, (int64_t)surf->clip.x);
        int32_t x_max = (int32_t)SOL_MIN((int64_t)cx + dx_max,
                                         (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1);
        int32_t x;

        for (x = x_min; x <= x_max; ++x) {
            if (sol__angle_in_range(sol__atan2_deg(dy, (int64_t)x - (int64_t)cx), s_norm, span)) {
                size_t idx = (size_t)y * (size_t)surf->stride + (size_t)x;
                surf->pixels[idx] = sol__blend_argb_fast(surf->pixels[idx], color);
            }
        }
    }
}

void sol_fill_ring(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t inner_radius,
                   int32_t outer_radius, sol_color_t color) {
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    uint64_t ro2;
    uint64_t ri2;

    if (surf == NULL || surf->pixels == NULL || outer_radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (inner_radius <= 0) {
        sol_fill_circle(surf, cx, cy, outer_radius, color);
        return;
    }
    if (inner_radius >= outer_radius) {
        return;
    }

    y_min = (int32_t)SOL_MAX((int64_t)cy - (int64_t)outer_radius, (int64_t)surf->clip.y);
    y_max = (int32_t)SOL_MIN((int64_t)cy + (int64_t)outer_radius,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);
    ro2 = (uint64_t)outer_radius * (uint64_t)outer_radius;
    ri2 = (uint64_t)inner_radius * (uint64_t)inner_radius;

    for (y = y_min; y <= y_max; ++y) {
        int64_t dy = (int64_t)y - (int64_t)cy;
        uint64_t dy2 = (uint64_t)(dy * dy);
        int32_t dx_out = (int32_t)sol__isqrt64(ro2 - dy2);

        if (dy2 >= ri2) {
            sol__draw_hline_span(surf, cx - dx_out, cx + dx_out, y, color);
        } else {
            int32_t dx_in = (int32_t)sol__isqrt64(ri2 - dy2);
            if (dx_out > dx_in) {
                sol__draw_hline_span(surf, cx - dx_out, cx - dx_in - 1, y, color);
                sol__draw_hline_span(surf, cx + dx_in + 1, cx + dx_out, y, color);
            }
        }
    }
}
