#include "sol_internal.h"

sol_mat2x3_t sol_mat2x3_identity(void) {
    sol_mat2x3_t mat = {{SOL_FX16_ONE, 0, 0, 0, SOL_FX16_ONE, 0}};
    return mat;
}

sol_mat2x3_t sol_mat2x3_translate(int32_t tx, int32_t ty) {
    sol_mat2x3_t mat = {
        {SOL_FX16_ONE, 0, SOL_FX16_FROM_INT(tx), 0, SOL_FX16_ONE, SOL_FX16_FROM_INT(ty)}};
    return mat;
}

sol_mat2x3_t sol_mat2x3_scale_fp16(int32_t sx_fp16, int32_t sy_fp16) {
    sol_mat2x3_t mat = {{sx_fp16, 0, 0, 0, sy_fp16, 0}};
    return mat;
}

sol_mat2x3_t sol_mat2x3_rotate_deg(int32_t deg) {
    int32_t c = sol__cos_deg_fp16(deg);
    int32_t s = sol__sin_deg_fp16(deg);
    sol_mat2x3_t mat = {{c, -s, 0, s, c, 0}};
    return mat;
}

sol_mat2x3_t sol_mat2x3_mul(sol_mat2x3_t a, sol_mat2x3_t b) {
    sol_mat2x3_t res;
    res.m[0] = sol__fx16_mul(a.m[0], b.m[0]) + sol__fx16_mul(a.m[1], b.m[3]);
    res.m[1] = sol__fx16_mul(a.m[0], b.m[1]) + sol__fx16_mul(a.m[1], b.m[4]);
    res.m[2] = sol__fx16_mul(a.m[0], b.m[2]) + sol__fx16_mul(a.m[1], b.m[5]) + a.m[2];
    res.m[3] = sol__fx16_mul(a.m[3], b.m[0]) + sol__fx16_mul(a.m[4], b.m[3]);
    res.m[4] = sol__fx16_mul(a.m[3], b.m[1]) + sol__fx16_mul(a.m[4], b.m[4]);
    res.m[5] = sol__fx16_mul(a.m[3], b.m[2]) + sol__fx16_mul(a.m[4], b.m[5]) + a.m[5];
    return res;
}

SOL_PRIVATE bool sol__clip_blit_rects(const sol_surface_t *dst, int32_t *dx, int32_t *dy,
                                      const sol_surface_t *src, sol_rect_t *sr) {
    sol_rect_t src_bounds;
    sol_rect_t dst_bounds;

    if (dst == NULL || dst->pixels == NULL || src == NULL || src->pixels == NULL) {
        return false;
    }
    if (dst->clip.w <= 0 || dst->clip.h <= 0 || src->width <= 0 || src->height <= 0) {
        return false;
    }

    src_bounds = SOL_RECT(0, 0, src->width, src->height);
    {
        sol_rect_t clipped_src = sol_rect_intersect(src_bounds, *sr);
        if (clipped_src.w <= 0 || clipped_src.h <= 0) {
            return false;
        }
        *dx += clipped_src.x - sr->x;
        *dy += clipped_src.y - sr->y;
        *sr = clipped_src;
    }

    dst_bounds = SOL_RECT(*dx, *dy, sr->w, sr->h);
    {
        sol_rect_t clipped_dst = sol_rect_intersect(dst->clip, dst_bounds);
        if (clipped_dst.w <= 0 || clipped_dst.h <= 0) {
            return false;
        }
        sr->x += clipped_dst.x - *dx;
        sr->y += clipped_dst.y - *dy;
        sr->w = clipped_dst.w;
        sr->h = clipped_dst.h;
        *dx = clipped_dst.x;
        *dy = clipped_dst.y;
    }

    return true;
}

void sol_blit(sol_surface_t *dst, int32_t dx, int32_t dy, const sol_surface_t *src) {
    if (src == NULL) {
        return;
    }
    sol_blit_rect(dst, dx, dy, src, SOL_RECT(0, 0, src->width, src->height), SOL_BLEND_SRC_OVER);
}

void sol_blit_rect(sol_surface_t *dst, int32_t dx, int32_t dy, const sol_surface_t *src,
                   sol_rect_t src_rect, sol_blend_mode_t mode) {
    int32_t row;
    int32_t col;
    bool reverse_y;
    bool same_buf;

    if (!sol__clip_blit_rects(dst, &dx, &dy, src, &src_rect)) {
        return;
    }

    same_buf = ((const void *)dst->pixels == (const void *)src->pixels);
    reverse_y = same_buf && (dy > src_rect.y);

    for (row = 0; row < src_rect.h; ++row) {
        int32_t r = reverse_y ? (src_rect.h - 1 - row) : row;
        uint32_t *d_row = dst->pixels + (size_t)(dy + r) * (size_t)dst->stride + (size_t)dx;
        const uint32_t *s_row =
            src->pixels + (size_t)(src_rect.y + r) * (size_t)src->stride + (size_t)src_rect.x;

        if (mode == SOL_BLEND_NONE) {
            if (same_buf) {
                SOL_MEMMOVE(d_row, s_row, (size_t)src_rect.w * sizeof(uint32_t));
            } else {
                SOL_MEMCPY(d_row, s_row, (size_t)src_rect.w * sizeof(uint32_t));
            }
        } else if (mode == SOL_BLEND_SRC_OVER) {
            if (same_buf && dy == src_rect.y && dx > src_rect.x) {
                for (col = src_rect.w - 1; col >= 0; --col) {
                    d_row[col] = sol__blend_argb_fast(d_row[col], s_row[col]);
                }
            } else {
                for (col = 0; col < src_rect.w; ++col) {
                    d_row[col] = sol__blend_argb_fast(d_row[col], s_row[col]);
                }
            }
        } else {
            if (same_buf && dy == src_rect.y && dx > src_rect.x) {
                for (col = src_rect.w - 1; col >= 0; --col) {
                    d_row[col] = sol_color_blend_mode(d_row[col], s_row[col], mode);
                }
            } else {
                for (col = 0; col < src_rect.w; ++col) {
                    d_row[col] = sol_color_blend_mode(d_row[col], s_row[col], mode);
                }
            }
        }
    }
}

void sol_blit_tinted(sol_surface_t *dst, int32_t dx, int32_t dy, const sol_surface_t *src,
                     sol_rect_t src_rect, sol_color_t tint) {
    uint32_t ta = SOL_COLOR_A(tint);
    uint32_t tr = SOL_COLOR_R(tint);
    uint32_t tg = SOL_COLOR_G(tint);
    uint32_t tb = SOL_COLOR_B(tint);
    int32_t row;
    int32_t col;

    if (ta == 0u) {
        return;
    }
    if (tint == SOL_COLOR_WHITE) {
        sol_blit_rect(dst, dx, dy, src, src_rect, SOL_BLEND_SRC_OVER);
        return;
    }
    if (!sol__clip_blit_rects(dst, &dx, &dy, src, &src_rect)) {
        return;
    }

    for (row = 0; row < src_rect.h; ++row) {
        uint32_t *d_row = dst->pixels + (size_t)(dy + row) * (size_t)dst->stride + (size_t)dx;
        const uint32_t *s_row =
            src->pixels + (size_t)(src_rect.y + row) * (size_t)src->stride + (size_t)src_rect.x;

        for (col = 0; col < src_rect.w; ++col) {
            uint32_t c = s_row[col];
            uint32_t a = sol__div255((uint32_t)SOL_COLOR_A(c) * ta);
            if (a == 0u) {
                continue;
            }
            uint32_t r = sol__div255((uint32_t)SOL_COLOR_R(c) * tr);
            uint32_t g = sol__div255((uint32_t)SOL_COLOR_G(c) * tg);
            uint32_t b = sol__div255((uint32_t)SOL_COLOR_B(c) * tb);
            d_row[col] = sol__blend_argb_fast(d_row[col], SOL_ARGB(a, r, g, b));
        }
    }
}

void sol_blit_alpha_mask(sol_surface_t *dst, int32_t dx, int32_t dy, const uint8_t *mask,
                         int32_t mask_w, int32_t mask_h, int32_t mask_stride, sol_rect_t src_rect,
                         sol_color_t color) {
    sol_rect_t sr;
    sol_rect_t dr;
    uint32_t sa = SOL_COLOR_A(color);
    uint32_t rgb = color & 0x00FFFFFFu;
    int32_t row;
    int32_t col;

    if (dst == NULL || dst->pixels == NULL || mask == NULL || mask_w <= 0 || mask_h <= 0 ||
        mask_stride < mask_w || sa == 0u || dst->clip.w <= 0 || dst->clip.h <= 0) {
        return;
    }

    sr = sol_rect_intersect(SOL_RECT(0, 0, mask_w, mask_h), src_rect);
    if (sr.w <= 0 || sr.h <= 0) {
        return;
    }
    dx += sr.x - src_rect.x;
    dy += sr.y - src_rect.y;

    dr = sol_rect_intersect(dst->clip, SOL_RECT(dx, dy, sr.w, sr.h));
    if (dr.w <= 0 || dr.h <= 0) {
        return;
    }
    sr.x += dr.x - dx;
    sr.y += dr.y - dy;

    for (row = 0; row < dr.h; ++row) {
        uint32_t *d_row = dst->pixels + (size_t)(dr.y + row) * (size_t)dst->stride + (size_t)dr.x;
        const uint8_t *m_row = mask + (size_t)(sr.y + row) * (size_t)mask_stride + (size_t)sr.x;

        for (col = 0; col < dr.w; ++col) {
            uint8_t cov = m_row[col];
            if (cov == 0u) {
                continue;
            }
            if (cov == 255u) {
                d_row[col] = sol__blend_argb_fast(d_row[col], color);
            } else {
                uint32_t a = sol__div255(sa * (uint32_t)cov);
                if (a > 0u) {
                    d_row[col] = sol__blend_argb_fast(d_row[col], rgb | (a << 24));
                }
            }
        }
    }
}

SOL_PRIVATE sol_color_t sol__sample_bilinear(const sol_surface_t *src, sol_rect_t sr,
                                             int64_t sx_fp16, int64_t sy_fp16) {
    int32_t x0 = (int32_t)(sx_fp16 >> 16);
    int32_t y0 = (int32_t)(sy_fp16 >> 16);
    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;
    int32_t max_x = sr.x + sr.w - 1;
    int32_t max_y = sr.y + sr.h - 1;

    if (x0 < sr.x) {
        x0 = sr.x;
    }
    if (x1 > max_x) {
        x1 = max_x;
    }
    if (y0 < sr.y) {
        y0 = sr.y;
    }
    if (y1 > max_y) {
        y1 = max_y;
    }

    {
        sol_color_t p00 =
            sol_color_premul(src->pixels[(size_t)y0 * (size_t)src->stride + (size_t)x0]);
        sol_color_t p10 =
            sol_color_premul(src->pixels[(size_t)y0 * (size_t)src->stride + (size_t)x1]);
        sol_color_t p01 =
            sol_color_premul(src->pixels[(size_t)y1 * (size_t)src->stride + (size_t)x0]);
        sol_color_t p11 =
            sol_color_premul(src->pixels[(size_t)y1 * (size_t)src->stride + (size_t)x1]);
        uint32_t fx = (uint32_t)((sx_fp16 >> 8) & 0xFF);
        uint32_t fy = (uint32_t)((sy_fp16 >> 8) & 0xFF);
        uint32_t ifx = 256u - fx;
        uint32_t ify = 256u - fy;
        uint32_t w00 = ifx * ify;
        uint32_t w10 = fx * ify;
        uint32_t w01 = ifx * fy;
        uint32_t w11 = fx * fy;

        uint32_t a =
            ((uint32_t)SOL_COLOR_A(p00) * w00 + (uint32_t)SOL_COLOR_A(p10) * w10 +
             (uint32_t)SOL_COLOR_A(p01) * w01 + (uint32_t)SOL_COLOR_A(p11) * w11 + 32768u) >>
            16;
        uint32_t r =
            ((uint32_t)SOL_COLOR_R(p00) * w00 + (uint32_t)SOL_COLOR_R(p10) * w10 +
             (uint32_t)SOL_COLOR_R(p01) * w01 + (uint32_t)SOL_COLOR_R(p11) * w11 + 32768u) >>
            16;
        uint32_t g =
            ((uint32_t)SOL_COLOR_G(p00) * w00 + (uint32_t)SOL_COLOR_G(p10) * w10 +
             (uint32_t)SOL_COLOR_G(p01) * w01 + (uint32_t)SOL_COLOR_G(p11) * w11 + 32768u) >>
            16;
        uint32_t b =
            ((uint32_t)SOL_COLOR_B(p00) * w00 + (uint32_t)SOL_COLOR_B(p10) * w10 +
             (uint32_t)SOL_COLOR_B(p01) * w01 + (uint32_t)SOL_COLOR_B(p11) * w11 + 32768u) >>
            16;

        return sol_color_unpremul(SOL_ARGB((a > 255u) ? 255u : a, (r > 255u) ? 255u : r,
                                           (g > 255u) ? 255u : g, (b > 255u) ? 255u : b));
    }
}

void sol_blit_scaled(sol_surface_t *dst, sol_rect_t dst_rect, const sol_surface_t *src,
                     sol_rect_t src_rect, bool bilinear, sol_blend_mode_t mode) {
    sol_rect_t sr;
    sol_rect_t dr;
    int32_t row;
    int32_t col;

    if (dst == NULL || dst->pixels == NULL || src == NULL || src->pixels == NULL) {
        return;
    }
    if (dst_rect.w <= 0 || dst_rect.h <= 0 || src->width <= 0 || src->height <= 0) {
        return;
    }

    sr = sol_rect_intersect(SOL_RECT(0, 0, src->width, src->height), src_rect);
    if (sr.w <= 0 || sr.h <= 0) {
        return;
    }

    dr = sol_rect_intersect(dst->clip, dst_rect);
    if (dr.w <= 0 || dr.h <= 0) {
        return;
    }

    if (dst_rect.w == sr.w && dst_rect.h == sr.h) {
        sol_blit_rect(dst, dst_rect.x, dst_rect.y, src, sr, mode);
        return;
    }

    for (row = dr.y; row < dr.y + dr.h; ++row) {
        uint32_t *d_row = dst->pixels + (size_t)row * (size_t)dst->stride;

        if (!bilinear || sr.w == 1 || sr.h == 1) {
            int32_t sy = sr.y + (int32_t)(((int64_t)(row - dst_rect.y) * (int64_t)sr.h) /
                                          (int64_t)dst_rect.h);
            const uint32_t *s_row = src->pixels + (size_t)sy * (size_t)src->stride;

            for (col = dr.x; col < dr.x + dr.w; ++col) {
                int32_t sx = sr.x + (int32_t)(((int64_t)(col - dst_rect.x) * (int64_t)sr.w) /
                                              (int64_t)dst_rect.w);
                d_row[col] = sol_color_blend_mode(d_row[col], s_row[sx], mode);
            }
        } else {
            int64_t sy_fp16 = ((int64_t)sr.y << 16) +
                              (((int64_t)(row - dst_rect.y) * (int64_t)(sr.h - 1)) << 16) /
                                  (int64_t)(dst_rect.h > 1 ? (dst_rect.h - 1) : 1);

            for (col = dr.x; col < dr.x + dr.w; ++col) {
                int64_t sx_fp16 = ((int64_t)sr.x << 16) +
                                  (((int64_t)(col - dst_rect.x) * (int64_t)(sr.w - 1)) << 16) /
                                      (int64_t)(dst_rect.w > 1 ? (dst_rect.w - 1) : 1);
                sol_color_t c = sol__sample_bilinear(src, sr, sx_fp16, sy_fp16);
                d_row[col] = sol_color_blend_mode(d_row[col], c, mode);
            }
        }
    }
}

void sol_blit_9slice(sol_surface_t *dst, sol_rect_t dst_rect, const sol_surface_t *src,
                     sol_rect_t src_rect, sol_9slice_t margins, bool bilinear,
                     sol_blend_mode_t mode) {
    sol_rect_t sr;
    int32_t sl;
    int32_t st;
    int32_t sr_m;
    int32_t sb;
    int32_t dl;
    int32_t dt;
    int32_t dr_m;
    int32_t db;
    int32_t sx[4];
    int32_t sy[4];
    int32_t dx[4];
    int32_t dy[4];
    int row_idx;
    int col_idx;

    if (dst == NULL || src == NULL || dst_rect.w <= 0 || dst_rect.h <= 0) {
        return;
    }

    sr = sol_rect_intersect(SOL_RECT(0, 0, src->width, src->height), src_rect);
    if (sr.w <= 0 || sr.h <= 0) {
        return;
    }

    sl = SOL_MAX(0, margins.left);
    st = SOL_MAX(0, margins.top);
    sr_m = SOL_MAX(0, margins.right);
    sb = SOL_MAX(0, margins.bottom);

    if (sl + sr_m > sr.w) {
        int32_t sum = sl + sr_m;
        sl = (int32_t)(((int64_t)sl * sr.w) / sum);
        sr_m = sr.w - sl;
    }
    if (st + sb > sr.h) {
        int32_t sum = st + sb;
        st = (int32_t)(((int64_t)st * sr.h) / sum);
        sb = sr.h - st;
    }

    dl = sl;
    dr_m = sr_m;
    dt = st;
    db = sb;
    if (dl + dr_m > dst_rect.w) {
        int32_t sum = dl + dr_m;
        dl = (int32_t)(((int64_t)dl * dst_rect.w) / sum);
        dr_m = dst_rect.w - dl;
    }
    if (dt + db > dst_rect.h) {
        int32_t sum = dt + db;
        dt = (int32_t)(((int64_t)dt * dst_rect.h) / sum);
        db = dst_rect.h - dt;
    }

    sx[0] = sr.x;
    sx[1] = sr.x + sl;
    sx[2] = sr.x + sr.w - sr_m;
    sx[3] = sr.x + sr.w;

    sy[0] = sr.y;
    sy[1] = sr.y + st;
    sy[2] = sr.y + sr.h - sb;
    sy[3] = sr.y + sr.h;

    dx[0] = dst_rect.x;
    dx[1] = dst_rect.x + dl;
    dx[2] = dst_rect.x + dst_rect.w - dr_m;
    dx[3] = dst_rect.x + dst_rect.w;

    dy[0] = dst_rect.y;
    dy[1] = dst_rect.y + dt;
    dy[2] = dst_rect.y + dst_rect.h - db;
    dy[3] = dst_rect.y + dst_rect.h;

    for (row_idx = 0; row_idx < 3; ++row_idx) {
        int32_t sh = sy[row_idx + 1] - sy[row_idx];
        int32_t dh = dy[row_idx + 1] - dy[row_idx];
        if (sh <= 0 || dh <= 0) {
            continue;
        }
        for (col_idx = 0; col_idx < 3; ++col_idx) {
            int32_t sw = sx[col_idx + 1] - sx[col_idx];
            int32_t dw = dx[col_idx + 1] - dx[col_idx];
            if (sw <= 0 || dw <= 0) {
                continue;
            }
            sol_blit_scaled(dst, SOL_RECT(dx[col_idx], dy[row_idx], dw, dh), src,
                            SOL_RECT(sx[col_idx], sy[row_idx], sw, sh), bilinear, mode);
        }
    }
}

void sol_blit_transformed(sol_surface_t *dst, const sol_surface_t *src, sol_rect_t src_rect,
                          sol_mat2x3_t transform, bool bilinear, sol_blend_mode_t mode) {
    sol_rect_t sr;
    int64_t det_fp16;
    int64_t min_x = INT64_MAX;
    int64_t min_y = INT64_MAX;
    int64_t max_x = INT64_MIN;
    int64_t max_y = INT64_MIN;
    sol_rect_t bbox;

    if (dst == NULL || dst->pixels == NULL || src == NULL || src->pixels == NULL) {
        return;
    }

    sr = sol_rect_intersect(SOL_RECT(0, 0, src->width, src->height), src_rect);
    if (sr.w <= 0 || sr.h <= 0 || dst->clip.w <= 0 || dst->clip.h <= 0) {
        return;
    }

    /* Determinant of 2x2 part scaled by 2^16: (a*d - b*c) >> 16 */
    det_fp16 = ((int64_t)transform.m[0] * (int64_t)transform.m[4] -
                (int64_t)transform.m[1] * (int64_t)transform.m[3]) >>
               16;
    if (det_fp16 == 0) {
        return;
    }

    /* Transform 4 source corners [0..sr.w, 0..sr.h] in local space */
    {
        int32_t cx[4];
        int32_t cy[4];
        int i;

        cx[0] = 0;
        cy[0] = 0;
        cx[1] = sr.w;
        cy[1] = 0;
        cx[2] = sr.w;
        cy[2] = sr.h;
        cx[3] = 0;
        cy[3] = sr.h;

        for (i = 0; i < 4; ++i) {
            int64_t tx = ((int64_t)transform.m[0] * cx[i] + (int64_t)transform.m[1] * cy[i] +
                          transform.m[2]) >>
                         16;
            int64_t ty = ((int64_t)transform.m[3] * cx[i] + (int64_t)transform.m[4] * cy[i] +
                          transform.m[5]) >>
                         16;
            if (tx < min_x) {
                min_x = tx;
            }
            if (ty < min_y) {
                min_y = ty;
            }
            if (tx > max_x) {
                max_x = tx;
            }
            if (ty > max_y) {
                max_y = ty;
            }
        }
    }

    bbox = sol_rect_intersect(dst->clip,
                              SOL_RECT((int32_t)(min_x - 1), (int32_t)(min_y - 1),
                                       (int32_t)(max_x - min_x + 3), (int32_t)(max_y - min_y + 3)));
    if (bbox.w <= 0 || bbox.h <= 0) {
        return;
    }

    {
        int64_t a = transform.m[0];
        int64_t b = transform.m[1];
        int64_t tx = transform.m[2];
        int64_t c = transform.m[3];
        int64_t d = transform.m[4];
        int64_t ty = transform.m[5];
        int32_t py;
        int32_t px;

        for (py = bbox.y; py < bbox.y + bbox.h; ++py) {
            uint32_t *d_row = dst->pixels + (size_t)py * (size_t)dst->stride;
            int64_t dy_fp16 = ((int64_t)py << 16) - ty;

            for (px = bbox.x; px < bbox.x + bbox.w; ++px) {
                int64_t dx_fp16 = ((int64_t)px << 16) - tx;
                int64_t sx_fp16 = (d * dx_fp16 - b * dy_fp16) / det_fp16;
                int64_t sy_fp16 = (a * dy_fp16 - c * dx_fp16) / det_fp16;

                if (sx_fp16 < 0 || sy_fp16 < 0 || sx_fp16 >= ((int64_t)sr.w << 16) ||
                    sy_fp16 >= ((int64_t)sr.h << 16)) {
                    continue;
                }

                if (!bilinear) {
                    int32_t ix = sr.x + (int32_t)(sx_fp16 >> 16);
                    int32_t iy = sr.y + (int32_t)(sy_fp16 >> 16);
                    sol_color_t sc = src->pixels[(size_t)iy * (size_t)src->stride + (size_t)ix];
                    d_row[px] = sol_color_blend_mode(d_row[px], sc, mode);
                } else {
                    sol_color_t sc = sol__sample_bilinear(src, sr, ((int64_t)sr.x << 16) + sx_fp16,
                                                          ((int64_t)sr.y << 16) + sy_fp16);
                    d_row[px] = sol_color_blend_mode(d_row[px], sc, mode);
                }
            }
        }
    }
}

void sol_surface_flip_h(sol_surface_t *surf) {
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 1 || surf->height <= 0) {
        return;
    }

    for (row = 0; row < surf->height; ++row) {
        uint32_t *line = surf->pixels + (size_t)row * (size_t)surf->stride;
        for (col = 0; col < surf->width / 2; ++col) {
            int32_t opp = surf->width - 1 - col;
            uint32_t tmp = line[col];
            line[col] = line[opp];
            line[opp] = tmp;
        }
    }
}

void sol_surface_flip_v(sol_surface_t *surf) {
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL || surf->width <= 0 || surf->height <= 1) {
        return;
    }

    for (row = 0; row < surf->height / 2; ++row) {
        int32_t opp = surf->height - 1 - row;
        uint32_t *top = surf->pixels + (size_t)row * (size_t)surf->stride;
        uint32_t *bot = surf->pixels + (size_t)opp * (size_t)surf->stride;
        for (col = 0; col < surf->width; ++col) {
            uint32_t tmp = top[col];
            top[col] = bot[col];
            bot[col] = tmp;
        }
    }
}

sol_status_t sol_surface_rotate_90(sol_surface_t *dst, const sol_surface_t *src, int32_t cw_turns) {
    int32_t turns;
    int32_t y;
    int32_t x;

    if (dst == NULL || dst->pixels == NULL || src == NULL || src->pixels == NULL || dst == src) {
        return SOL_ERR_INVALID_ARG;
    }

    turns = cw_turns & 3;
    if (turns == 0 || turns == 2) {
        if (dst->width != src->width || dst->height != src->height) {
            return SOL_ERR_INVALID_ARG;
        }
    } else {
        if (dst->width != src->height || dst->height != src->width) {
            return SOL_ERR_INVALID_ARG;
        }
    }

    for (y = 0; y < src->height; ++y) {
        for (x = 0; x < src->width; ++x) {
            uint32_t px = src->pixels[(size_t)y * (size_t)src->stride + (size_t)x];
            int32_t dx = x;
            int32_t dy = y;

            if (turns == 1) {
                dx = src->height - 1 - y;
                dy = x;
            } else if (turns == 2) {
                dx = src->width - 1 - x;
                dy = src->height - 1 - y;
            } else if (turns == 3) {
                dx = y;
                dy = src->width - 1 - x;
            }
            dst->pixels[(size_t)dy * (size_t)dst->stride + (size_t)dx] = px;
        }
    }

    return SOL_OK;
}
