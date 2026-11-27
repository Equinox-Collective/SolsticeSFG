#include "sol_internal.h"

void sol_draw_line_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      sol_color_t color) {
    bool steep;
    int64_t dx;
    int64_t dy;
    int64_t grad;
    int32_t x_start;
    int32_t x_end;
    int32_t min_major;
    int32_t max_major;
    int64_t intery;
    int32_t x;

    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }
    if (x0 == x1) {
        int32_t top = SOL_MIN(y0, y1);
        int32_t bot = SOL_MAX(y0, y1);
        sol_draw_vline(surf, x0, top, bot - top + 1, color);
        return;
    }
    if (y0 == y1) {
        int32_t left = SOL_MIN(x0, x1);
        int32_t right = SOL_MAX(x0, x1);
        sol_draw_hline(surf, left, y0, right - left + 1, color);
        return;
    }

    {
        int64_t adx = (int64_t)x1 - (int64_t)x0;
        int64_t ady = (int64_t)y1 - (int64_t)y0;
        if (adx < 0) {
            adx = -adx;
        }
        if (ady < 0) {
            ady = -ady;
        }
        steep = ady > adx;
    }

    if (steep) {
        int32_t t0 = x0;
        int32_t t1 = x1;
        x0 = y0;
        y0 = t0;
        x1 = y1;
        y1 = t1;
    }
    if (x0 > x1) {
        int32_t tx = x0;
        int32_t ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }

    dx = (int64_t)x1 - (int64_t)x0;
    dy = (int64_t)y1 - (int64_t)y0;
    grad = (dy << 16) / dx;

    min_major = steep ? (surf->clip.y - 1) : (surf->clip.x - 1);
    max_major = steep ? (surf->clip.y + surf->clip.h) : (surf->clip.x + surf->clip.w);

    x_start = (x0 < min_major) ? min_major : x0;
    x_end = (x1 > max_major) ? max_major : x1;
    if (x_start > x_end) {
        return;
    }

    intery = ((int64_t)y0 << 16) + grad * ((int64_t)x_start - (int64_t)x0);

    for (x = x_start; x <= x_end; ++x) {
        int64_t floor_y = (intery >= 0) ? (intery >> 16) : -(((-intery) + 0xFFFF) >> 16);
        uint8_t frac = (uint8_t)(((intery - (floor_y << 16)) >> 8) & 0xFF);
        int32_t iy = (int32_t)floor_y;

        if (steep) {
            sol__plot_cov(surf, iy, x, color, (uint8_t)(255u - frac));
            sol__plot_cov(surf, iy + 1, x, color, frac);
        } else {
            sol__plot_cov(surf, x, iy, color, (uint8_t)(255u - frac));
            sol__plot_cov(surf, x, iy + 1, color, frac);
        }
        intery += grad;
    }
}

void sol_draw_line_thick_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                            int32_t thickness, sol_line_cap_t cap, sol_color_t color) {
    int64_t dx;
    int64_t dy;
    int64_t len2;
    int32_t r_fp8;
    int32_t pad;
    int32_t min_y;
    int32_t max_y;
    int32_t min_x;
    int32_t max_x;
    int32_t y;

    if (surf == NULL || surf->pixels == NULL || thickness <= 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }
    if (thickness == 1 && cap == SOL_CAP_BUTT) {
        sol_draw_line_aa(surf, x0, y0, x1, y1, color);
        return;
    }

    r_fp8 = thickness * 128; /* thickness / 2 in 1/256 px */
    pad = (thickness / 2) + 2;

    if (x0 == x1 && y0 == y1) {
        if (cap == SOL_CAP_ROUND) {
            sol_fill_circle_aa(surf, x0, y0, thickness / 2, color);
        } else {
            sol_fill_rect(surf,
                          SOL_RECT(x0 - thickness / 2, y0 - thickness / 2, thickness, thickness),
                          color);
        }
        return;
    }

    dx = (int64_t)x1 - (int64_t)x0;
    dy = (int64_t)y1 - (int64_t)y0;
    len2 = dx * dx + dy * dy;

    min_y = (int32_t)SOL_MAX(SOL_MIN((int64_t)y0, (int64_t)y1) - pad, (int64_t)surf->clip.y);
    max_y = (int32_t)SOL_MIN(SOL_MAX((int64_t)y0, (int64_t)y1) + pad,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);
    min_x = (int32_t)SOL_MAX(SOL_MIN((int64_t)x0, (int64_t)x1) - pad, (int64_t)surf->clip.x);
    max_x = (int32_t)SOL_MIN(SOL_MAX((int64_t)x0, (int64_t)x1) + pad,
                             (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1);

    if (min_y > max_y || min_x > max_x) {
        return;
    }

    {
        uint32_t len_fp8 = sol__isqrt64((uint64_t)len2 << 16);
        int64_t ady = (dy < 0) ? -dy : dy;
        int64_t x_span_margin = (ady > 0)
                                    ? (((int64_t)pad * (int64_t)(len_fp8 >> 8) + ady - 1) / ady) + 2
                                    : (max_x - min_x + 1);

        for (y = min_y; y <= max_y; ++y) {
            int32_t row_x0 = min_x;
            int32_t row_x1 = max_x;
            int32_t x;

            if (ady > 0 && len2 > 0) {
                int64_t xc = (int64_t)x0 + (dx * ((int64_t)y - (int64_t)y0)) / dy;
                row_x0 = (int32_t)SOL_MAX((int64_t)min_x, xc - x_span_margin);
                row_x1 = (int32_t)SOL_MIN((int64_t)max_x, xc + x_span_margin);
            }

            for (x = row_x0; x <= row_x1; ++x) {
                int64_t px = (int64_t)x - (int64_t)x0;
                int64_t py = (int64_t)y - (int64_t)y0;
                int64_t dot = px * dx + py * dy;
                int64_t cross = px * dy - py * dx;
                int64_t dist_perp_fp8 = ((cross < 0 ? -cross : cross) << 8) / (int64_t)len_fp8;
                int64_t dist_fp8;

                if (cap == SOL_CAP_ROUND) {
                    if (dot <= 0) {
                        dist_fp8 = (int64_t)sol__isqrt64((uint64_t)(px * px + py * py) << 16);
                    } else if (dot >= len2) {
                        int64_t qx = (int64_t)x - (int64_t)x1;
                        int64_t qy = (int64_t)y - (int64_t)y1;
                        dist_fp8 = (int64_t)sol__isqrt64((uint64_t)(qx * qx + qy * qy) << 16);
                    } else {
                        dist_fp8 = dist_perp_fp8;
                    }
                } else {
                    int64_t along_fp8 = (dot << 8) / (int64_t)len_fp8;
                    int64_t max_along_fp8 = (int64_t)len_fp8;
                    int64_t ext = (cap == SOL_CAP_SQUARE) ? (int64_t)r_fp8 : 0;
                    int64_t cap_dist = 0;

                    if (along_fp8 < -ext) {
                        cap_dist = (-ext) - along_fp8;
                    } else if (along_fp8 > max_along_fp8 + ext) {
                        cap_dist = along_fp8 - (max_along_fp8 + ext);
                    }
                    dist_fp8 = SOL_MAX(dist_perp_fp8, cap_dist);
                }

                if (dist_fp8 < (int64_t)r_fp8 + 128) {
                    int64_t cov = (int64_t)r_fp8 + 128 - dist_fp8;
                    uint8_t c8 = (cov >= 255) ? 255u : (uint8_t)cov;
                    sol__plot_cov(surf, x, y, color, c8);
                }
            }
        }
    }
}

void sol_fill_circle_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                        sol_color_t color) {
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    int64_t r_fp8;
    int64_t r_inner;

    if (surf == NULL || surf->pixels == NULL || radius < 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (radius == 0) {
        sol_blend_pixel(surf, cx, cy, color);
        return;
    }

    r_fp8 = (int64_t)radius * 256;
    r_inner = (radius > 1) ? (radius - 1) : 0;

    y_min = (int32_t)SOL_MAX((int64_t)cy - (int64_t)radius - 1, (int64_t)surf->clip.y);
    y_max = (int32_t)SOL_MIN((int64_t)cy + (int64_t)radius + 1,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);

    for (y = y_min; y <= y_max; ++y) {
        int64_t dy = (int64_t)y - (int64_t)cy;
        uint64_t dy2_fp = (uint64_t)(dy * dy) << 16;
        int32_t dx_outer = radius + 1;
        int32_t dx_solid = 0;

        if ((uint64_t)(dy * dy) < (uint64_t)(r_inner * r_inner)) {
            dx_solid = (int32_t)sol__isqrt64((uint64_t)(r_inner * r_inner) - (uint64_t)(dy * dy));
        }

        if (dx_solid > 0) {
            sol__hline_solid_clipped(surf, cx - dx_solid + 1, cx + dx_solid - 1, y, color);
        }

        {
            int32_t x_left_start = cx - dx_outer;
            int32_t x_left_end = (dx_solid > 0) ? (cx - dx_solid) : (cx + dx_outer);
            int32_t x;

            for (x = x_left_start; x <= x_left_end; ++x) {
                int64_t dx = (int64_t)x - (int64_t)cx;
                int64_t dist_fp = (int64_t)sol__isqrt64(((uint64_t)(dx * dx) << 16) + dy2_fp);
                int64_t cov = r_fp8 + 128 - dist_fp;
                if (cov > 0) {
                    sol__plot_cov(surf, x, y, color, (cov >= 255) ? 255u : (uint8_t)cov);
                }
            }

            if (dx_solid > 0) {
                for (x = cx + dx_solid; x <= cx + dx_outer; ++x) {
                    int64_t dx = (int64_t)x - (int64_t)cx;
                    int64_t dist_fp = (int64_t)sol__isqrt64(((uint64_t)(dx * dx) << 16) + dy2_fp);
                    int64_t cov = r_fp8 + 128 - dist_fp;
                    if (cov > 0) {
                        sol__plot_cov(surf, x, y, color, (cov >= 255) ? 255u : (uint8_t)cov);
                    }
                }
            }
        }
    }
}

void sol_draw_circle_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t radius,
                        int32_t thickness, sol_color_t color) {
    int32_t outer_r;
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    int64_t r_mid_fp8;
    int64_t half_t_fp8;

    if (surf == NULL || surf->pixels == NULL || radius <= 0 || thickness <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    half_t_fp8 = (int64_t)thickness * 128;
    r_mid_fp8 = (int64_t)radius * 256;
    outer_r = radius + (thickness / 2) + 2;

    y_min = (int32_t)SOL_MAX((int64_t)cy - outer_r, (int64_t)surf->clip.y);
    y_max =
        (int32_t)SOL_MIN((int64_t)cy + outer_r, (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);

    for (y = y_min; y <= y_max; ++y) {
        int64_t dy = (int64_t)y - (int64_t)cy;
        uint64_t dy2_fp = (uint64_t)(dy * dy) << 16;
        int32_t x_min = (int32_t)SOL_MAX((int64_t)cx - outer_r, (int64_t)surf->clip.x);
        int32_t x_max = (int32_t)SOL_MIN((int64_t)cx + outer_r,
                                         (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1);
        int32_t x;

        for (x = x_min; x <= x_max; ++x) {
            int64_t dx = (int64_t)x - (int64_t)cx;
            int64_t dist_fp = (int64_t)sol__isqrt64(((uint64_t)(dx * dx) << 16) + dy2_fp);
            int64_t diff = dist_fp - r_mid_fp8;
            int64_t adiff = (diff < 0) ? -diff : diff;
            int64_t cov = half_t_fp8 + 128 - adiff;

            if (cov > 0) {
                sol__plot_cov(surf, x, y, color, (cov >= 255) ? 255u : (uint8_t)cov);
            }
        }
    }
}

void sol_fill_ellipse_aa(sol_surface_t *surf, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                         sol_color_t color) {
    int32_t y_min;
    int32_t y_max;
    int32_t y;
    int64_t rx2;
    int64_t ry2;
    int64_t threshold;

    if (surf == NULL || surf->pixels == NULL || rx <= 0 || ry <= 0 || SOL_COLOR_A(color) == 0u) {
        return;
    }
    if (rx == ry) {
        sol_fill_circle_aa(surf, cx, cy, rx, color);
        return;
    }

    rx2 = (int64_t)rx * (int64_t)rx;
    ry2 = (int64_t)ry * (int64_t)ry;
    threshold = rx2 * ry2 * 16; /* Scaled for 4x4 subpixel sampling */

    y_min = (int32_t)SOL_MAX((int64_t)cy - (int64_t)ry - 1, (int64_t)surf->clip.y);
    y_max = (int32_t)SOL_MIN((int64_t)cy + (int64_t)ry + 1,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);

    for (y = y_min; y <= y_max; ++y) {
        int32_t x_min = (int32_t)SOL_MAX((int64_t)cx - (int64_t)rx - 1, (int64_t)surf->clip.x);
        int32_t x_max = (int32_t)SOL_MIN((int64_t)cx + (int64_t)rx + 1,
                                         (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1);
        int32_t x;

        for (x = x_min; x <= x_max; ++x) {
            static const int32_t k_sub[4] = {-3, -1, 1, 3}; /* Eighths around pixel center */
            int32_t hits = 0;
            int sy;
            int sx;

            /* Fast center interior check */
            int64_t cdx = ((int64_t)x - (int64_t)cx);
            int64_t cdy = ((int64_t)y - (int64_t)cy);
            if (cdx < 0) {
                cdx = -cdx;
            }
            if (cdy < 0) {
                cdy = -cdy;
            }
            if ((cdx + 1) * (cdx + 1) * ry2 + (cdy + 1) * (cdy + 1) * rx2 <= rx2 * ry2) {
                sol__plot_cov(surf, x, y, color, 255u);
                continue;
            }

            for (sy = 0; sy < 4; ++sy) {
                int64_t qy = ((int64_t)y - (int64_t)cy) * 4 + k_sub[sy];
                int64_t qy2 = qy * qy * rx2;
                for (sx = 0; sx < 4; ++sx) {
                    int64_t qx = ((int64_t)x - (int64_t)cx) * 4 + k_sub[sx];
                    if (qx * qx * ry2 + qy2 <= threshold) {
                        hits++;
                    }
                }
            }
            if (hits > 0) {
                sol__plot_cov(surf, x, y, color, (uint8_t)((hits * 255) / 16));
            }
        }
    }
}

void sol_fill_rounded_rect_ex_aa(sol_surface_t *surf, sol_rect_t rect, sol_corner_radii_t radii,
                                 sol_color_t color) {
    sol_rect_t r;
    int32_t row;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    if (radii.tl < 0) {
        radii.tl = 0;
    }
    if (radii.tr < 0) {
        radii.tr = 0;
    }
    if (radii.br < 0) {
        radii.br = 0;
    }
    if (radii.bl < 0) {
        radii.bl = 0;
    }
    if (radii.tl + radii.tr > rect.w) {
        int32_t s = radii.tl + radii.tr;
        radii.tl = (int32_t)(((int64_t)radii.tl * rect.w) / s);
        radii.tr = rect.w - radii.tl;
    }
    if (radii.bl + radii.br > rect.w) {
        int32_t s = radii.bl + radii.br;
        radii.bl = (int32_t)(((int64_t)radii.bl * rect.w) / s);
        radii.br = rect.w - radii.bl;
    }
    if (radii.tl + radii.bl > rect.h) {
        int32_t s = radii.tl + radii.bl;
        radii.tl = (int32_t)(((int64_t)radii.tl * rect.h) / s);
        radii.bl = rect.h - radii.tl;
    }
    if (radii.tr + radii.br > rect.h) {
        int32_t s = radii.tr + radii.br;
        radii.tr = (int32_t)(((int64_t)radii.tr * rect.h) / s);
        radii.br = rect.h - radii.tr;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        int32_t ly = row - rect.y;
        int32_t left_r = 0;
        int32_t right_r = 0;
        bool left_top = false;
        bool right_top = false;

        if (ly < radii.tl) {
            left_r = radii.tl;
            left_top = true;
        } else if (ly >= rect.h - radii.bl) {
            left_r = radii.bl;
        }

        if (ly < radii.tr) {
            right_r = radii.tr;
            right_top = true;
        } else if (ly >= rect.h - radii.br) {
            right_r = radii.br;
        }

        if (left_r == 0 && right_r == 0) {
            sol__hline_solid_clipped(surf, r.x, r.x + r.w - 1, row, color);
        } else {
            int32_t solid_x0 = rect.x + left_r;
            int32_t solid_x1 = rect.x + rect.w - 1 - right_r;
            int32_t col;

            if (solid_x0 <= solid_x1) {
                sol__hline_solid_clipped(surf, solid_x0, solid_x1, row, color);
            }

            /* Left corner AA band */
            if (left_r > 0) {
                int64_t cy2 = left_top ? ((int64_t)(rect.y + left_r) * 2)
                                       : ((int64_t)(rect.y + rect.h - left_r) * 2);
                int64_t cx2 = (int64_t)(rect.x + left_r) * 2;
                int64_t py2 = (int64_t)row * 2 + 1;
                int64_t dy = (py2 - cy2) * 128;
                int64_t r_fp8 = (int64_t)left_r * 256;
                int32_t x0 = (int32_t)SOL_MAX((int64_t)rect.x, (int64_t)r.x);
                int32_t x1 = (int32_t)SOL_MIN((int64_t)solid_x0 - 1, (int64_t)r.x + r.w - 1);

                for (col = x0; col <= x1; ++col) {
                    int64_t dx = (cx2 - ((int64_t)col * 2 + 1)) * 128;
                    int64_t dist = (int64_t)sol__isqrt64((uint64_t)(dx * dx) + (uint64_t)(dy * dy));
                    int64_t cov = r_fp8 + 128 - dist;
                    if (cov > 0) {
                        sol__plot_cov(surf, col, row, color, (cov >= 255) ? 255u : (uint8_t)cov);
                    }
                }
            }

            /* Right corner AA band */
            if (right_r > 0) {
                int64_t cy2 = right_top ? ((int64_t)(rect.y + right_r) * 2)
                                        : ((int64_t)(rect.y + rect.h - right_r) * 2);
                int64_t cx2 = (int64_t)(rect.x + rect.w - right_r) * 2;
                int64_t py2 = (int64_t)row * 2 + 1;
                int64_t dy = (py2 - cy2) * 128;
                int64_t r_fp8 = (int64_t)right_r * 256;
                int32_t x0 = (int32_t)SOL_MAX((int64_t)solid_x1 + 1, (int64_t)r.x);
                int32_t x1 = (int32_t)SOL_MIN((int64_t)rect.x + rect.w - 1, (int64_t)r.x + r.w - 1);

                for (col = x0; col <= x1; ++col) {
                    int64_t dx = (((int64_t)col * 2 + 1) - cx2) * 128;
                    int64_t dist = (int64_t)sol__isqrt64((uint64_t)(dx * dx) + (uint64_t)(dy * dy));
                    int64_t cov = r_fp8 + 128 - dist;
                    if (cov > 0) {
                        sol__plot_cov(surf, col, row, color, (cov >= 255) ? 255u : (uint8_t)cov);
                    }
                }
            }
        }
    }
}

void sol_fill_rounded_rect_aa(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                              sol_color_t color) {
    sol_fill_rounded_rect_ex_aa(surf, rect, SOL_CORNERS(radius, radius, radius, radius), color);
}

void sol_draw_rounded_rect_aa(sol_surface_t *surf, sol_rect_t rect, int32_t radius,
                              int32_t thickness, sol_color_t color) {
    sol_rect_t r;
    int32_t rad;
    int32_t row;
    int32_t col;
    int32_t bx0;
    int32_t by0;
    int32_t bx1;
    int32_t by1;
    int32_t t_fp8;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0 || thickness <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    rad = SOL_MIN(SOL_MAX(radius, 0), SOL_MIN(rect.w, rect.h) / 2);
    t_fp8 = thickness * 256;
    bx0 = rect.x;
    by0 = rect.y;
    bx1 = rect.x + rect.w;
    by1 = rect.y + rect.h;

    r = sol_rect_intersect(SOL_RECT(rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2), surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        int32_t ly = row - rect.y;
        bool in_flat_mid = (ly >= rad + thickness && ly < rect.h - rad - thickness);

        for (col = r.x; col < r.x + r.w; ++col) {
            if (in_flat_mid && col >= rect.x + thickness + 1 &&
                col < rect.x + rect.w - thickness - 1) {
                /* Skip completely hollow interior */
                col = rect.x + rect.w - thickness - 2;
                continue;
            }
            {
                int32_t sd = sol__sdf_rounded_box_fp8(col, row, bx0, by0, bx1, by1, rad);
                int32_t cov_out = 128 - sd;
                int32_t cov_in = sd + t_fp8 + 128;
                int32_t cov = SOL_MIN(cov_out, cov_in);

                if (cov > 0) {
                    sol__plot_cov(surf, col, row, color, (cov >= 255) ? 255u : (uint8_t)cov);
                }
            }
        }
    }
}

SOL_PRIVATE int64_t sol__edge_fn(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t px,
                                 int64_t py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void sol_fill_triangle_aa(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                          int32_t x2, int32_t y2, sol_color_t color) {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
    int32_t y;
    int32_t x;
    int64_t area2;

    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }

    area2 = sol__edge_fn(x0, y0, x1, y1, x2, y2);
    if (area2 == 0) {
        return;
    }
    if (area2 < 0) {
        /* Ensure consistent winding */
        int32_t tx = x1;
        int32_t ty = y1;
        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }

    min_x = (int32_t)SOL_MAX(SOL_MIN((int64_t)x0, SOL_MIN((int64_t)x1, (int64_t)x2)) - 1,
                             (int64_t)surf->clip.x);
    min_y = (int32_t)SOL_MAX(SOL_MIN((int64_t)y0, SOL_MIN((int64_t)y1, (int64_t)y2)) - 1,
                             (int64_t)surf->clip.y);
    max_x = (int32_t)SOL_MIN(SOL_MAX((int64_t)x0, SOL_MAX((int64_t)x1, (int64_t)x2)) + 1,
                             (int64_t)surf->clip.x + (int64_t)surf->clip.w - 1);
    max_y = (int32_t)SOL_MIN(SOL_MAX((int64_t)y0, SOL_MAX((int64_t)y1, (int64_t)y2)) + 1,
                             (int64_t)surf->clip.y + (int64_t)surf->clip.h - 1);

    if (min_x > max_x || min_y > max_y) {
        return;
    }

    {
        /* Precompute bounding-box edge tolerance for fast interior check (coordinates scaled by 4)
         */
        int64_t sx0 = (int64_t)x0 * 4;
        int64_t sy0 = (int64_t)y0 * 4;
        int64_t sx1 = (int64_t)x1 * 4;
        int64_t sy1 = (int64_t)y1 * 4;
        int64_t sx2 = (int64_t)x2 * 4;
        int64_t sy2 = (int64_t)y2 * 4;

        for (y = min_y; y <= max_y; ++y) {
            for (x = min_x; x <= max_x; ++x) {
                static const int32_t k_off[4] = {0, 1, 2, 3};
                int32_t hits = 0;
                int sy;
                int sx;

                for (sy = 0; sy < 4; ++sy) {
                    int64_t py = (int64_t)y * 4 + k_off[sy];
                    for (sx = 0; sx < 4; ++sx) {
                        int64_t px = (int64_t)x * 4 + k_off[sx];
                        if (sol__edge_fn(sx0, sy0, sx1, sy1, px, py) >= 0 &&
                            sol__edge_fn(sx1, sy1, sx2, sy2, px, py) >= 0 &&
                            sol__edge_fn(sx2, sy2, sx0, sy0, px, py) >= 0) {
                            hits++;
                        }
                    }
                }
                if (hits > 0) {
                    sol__plot_cov(surf, x, y, color, (uint8_t)((hits * 255) / 16));
                }
            }
        }
    }
}
