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

uint16_t sol_srgb_to_linear(uint8_t srgb) {
    /* Pure integer Chilliant sRGB polynomial: 0.012523*x + 0.682171*x^2 + 0.305306*x^3 */
    uint64_t c = (uint64_t)srgb;
    uint64_t t1 = (821ull * c) / 255ull;
    uint64_t t2 = (44706ull * c * c) / (255ull * 255ull);
    uint64_t t3 = (20008ull * c * c * c) / (255ull * 255ull * 255ull);
    uint64_t sum = t1 + t2 + t3;
    return (sum > 65535ull) ? 65535u : (uint16_t)sum;
}

uint8_t sol_linear_to_srgb(uint16_t linear) {
    uint32_t lo = 0u;
    uint32_t hi = 255u;
    uint32_t best = 0u;
    uint32_t best_diff = 0xFFFFFFFFu;

    while (lo <= hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        uint16_t val = sol_srgb_to_linear((uint8_t)mid);
        uint32_t diff = (val > linear) ? (uint32_t)(val - linear) : (uint32_t)(linear - val);

        if (diff < best_diff) {
            best_diff = diff;
            best = mid;
        }
        if (val == linear) {
            return (uint8_t)mid;
        }
        if (val < linear) {
            lo = mid + 1u;
        } else {
            if (mid == 0u) {
                break;
            }
            hi = mid - 1u;
        }
    }
    return (uint8_t)best;
}

sol_color_t sol_color_lerp_linear(sol_color_t a, sol_color_t b, uint8_t t) {
    uint32_t wt = (uint32_t)t;
    uint32_t inv_t = 255u - wt;
    uint32_t out_a = sol__div255((uint32_t)SOL_COLOR_A(a) * inv_t + (uint32_t)SOL_COLOR_A(b) * wt);

    uint32_t lr = (uint32_t)(((uint64_t)sol_srgb_to_linear(SOL_COLOR_R(a)) * inv_t +
                              (uint64_t)sol_srgb_to_linear(SOL_COLOR_R(b)) * wt + 127ull) /
                             255ull);
    uint32_t lg = (uint32_t)(((uint64_t)sol_srgb_to_linear(SOL_COLOR_G(a)) * inv_t +
                              (uint64_t)sol_srgb_to_linear(SOL_COLOR_G(b)) * wt + 127ull) /
                             255ull);
    uint32_t lb = (uint32_t)(((uint64_t)sol_srgb_to_linear(SOL_COLOR_B(a)) * inv_t +
                              (uint64_t)sol_srgb_to_linear(SOL_COLOR_B(b)) * wt + 127ull) /
                             255ull);

    return SOL_ARGB(out_a, sol_linear_to_srgb((uint16_t)lr), sol_linear_to_srgb((uint16_t)lg),
                    sol_linear_to_srgb((uint16_t)lb));
}

sol_color_t sol_color_blend(sol_color_t dst, sol_color_t src) {
    return sol__blend_argb_fast(dst, src);
}

SOL_PRIVATE uint32_t sol__blend_ch_factor(uint32_t s_ch, uint32_t d_ch, uint32_t factor_s,
                                          uint32_t factor_d) {
    uint32_t v = sol__div255(s_ch * factor_s + d_ch * factor_d);
    return (v > 255u) ? 255u : v;
}

SOL_PRIVATE uint32_t sol__overlay_ch(uint32_t s_ch, uint32_t d_ch) {
    if (d_ch < 128u) {
        return sol__div255(2u * s_ch * d_ch);
    }
    return 255u - sol__div255(2u * (255u - s_ch) * (255u - d_ch));
}

sol_color_t sol_color_blend_mode(sol_color_t dst, sol_color_t src, sol_blend_mode_t mode) {
    uint32_t sa = SOL_COLOR_A(src);
    uint32_t sr = SOL_COLOR_R(src);
    uint32_t sg = SOL_COLOR_G(src);
    uint32_t sb = SOL_COLOR_B(src);

    uint32_t da = SOL_COLOR_A(dst);
    uint32_t dr = SOL_COLOR_R(dst);
    uint32_t dg = SOL_COLOR_G(dst);
    uint32_t db = SOL_COLOR_B(dst);

    uint32_t inv_sa = 255u - sa;
    uint32_t inv_da = 255u - da;

    switch (mode) {
    case SOL_BLEND_NONE:
        return src;

    case SOL_BLEND_SRC_OVER:
        return sol__blend_argb_fast(dst, src);

    case SOL_BLEND_SRC_IN: {
        uint32_t a = sol__div255(sa * da);
        uint32_t r = sol__div255(sr * a);
        uint32_t g = sol__div255(sg * a);
        uint32_t b = sol__div255(sb * a);
        return SOL_ARGB(a, r, g, b);
    }

    case SOL_BLEND_SRC_OUT: {
        uint32_t a = sol__div255(sa * inv_da);
        return SOL_ARGB(a, sr, sg, sb);
    }

    case SOL_BLEND_SRC_ATOP: {
        uint32_t r = sol__blend_ch_factor(sr, dr, sa, inv_sa);
        uint32_t g = sol__blend_ch_factor(sg, dg, sa, inv_sa);
        uint32_t b = sol__blend_ch_factor(sb, db, sa, inv_sa);
        return SOL_ARGB(da, r, g, b);
    }

    case SOL_BLEND_DST_OVER: {
        if (da == 255u) {
            return dst;
        }
        uint32_t a = da + sol__div255(sa * inv_da);
        uint32_t r = sol__blend_ch_factor(dr, sr, da, inv_da);
        uint32_t g = sol__blend_ch_factor(dg, sg, da, inv_da);
        uint32_t b = sol__blend_ch_factor(db, sb, da, inv_da);
        return SOL_ARGB(a, r, g, b);
    }

    case SOL_BLEND_ADD: {
        uint32_t r = dr + sol__div255(sr * sa);
        uint32_t g = dg + sol__div255(sg * sa);
        uint32_t b = db + sol__div255(sb * sa);
        uint32_t a = da + sa;
        return SOL_ARGB((a > 255u) ? 255u : a, (r > 255u) ? 255u : r, (g > 255u) ? 255u : g,
                        (b > 255u) ? 255u : b);
    }

    case SOL_BLEND_MULTIPLY: {
        uint32_t mr = sol__div255(sr * dr);
        uint32_t mg = sol__div255(sg * dg);
        uint32_t mb = sol__div255(sb * db);
        uint32_t r = sol__blend_ch_factor(mr, dr, sa, inv_sa);
        uint32_t g = sol__blend_ch_factor(mg, dg, sa, inv_sa);
        uint32_t b = sol__blend_ch_factor(mb, db, sa, inv_sa);
        uint32_t a = sa + sol__div255(da * inv_sa);
        return SOL_ARGB(a, r, g, b);
    }

    case SOL_BLEND_SCREEN: {
        uint32_t scr_r = 255u - sol__div255((255u - sr) * (255u - dr));
        uint32_t scr_g = 255u - sol__div255((255u - sg) * (255u - dg));
        uint32_t scr_b = 255u - sol__div255((255u - sb) * (255u - db));
        uint32_t r = sol__blend_ch_factor(scr_r, dr, sa, inv_sa);
        uint32_t g = sol__blend_ch_factor(scr_g, dg, sa, inv_sa);
        uint32_t b = sol__blend_ch_factor(scr_b, db, sa, inv_sa);
        uint32_t a = sa + sol__div255(da * inv_sa);
        return SOL_ARGB(a, r, g, b);
    }

    case SOL_BLEND_OVERLAY: {
        uint32_t ov_r = sol__overlay_ch(sr, dr);
        uint32_t ov_g = sol__overlay_ch(sg, dg);
        uint32_t ov_b = sol__overlay_ch(sb, db);
        uint32_t r = sol__blend_ch_factor(ov_r, dr, sa, inv_sa);
        uint32_t g = sol__blend_ch_factor(ov_g, dg, sa, inv_sa);
        uint32_t b = sol__blend_ch_factor(ov_b, db, sa, inv_sa);
        uint32_t a = sa + sol__div255(da * inv_sa);
        return SOL_ARGB(a, r, g, b);
    }

    default:
        return sol__blend_argb_fast(dst, src);
    }
}

sol_color_t sol_color_premul(sol_color_t color) {
    uint32_t a = SOL_COLOR_A(color);
    if (a == 0u) {
        return 0u;
    }
    if (a == 255u) {
        return color;
    }
    return SOL_ARGB(a, sol__div255((uint32_t)SOL_COLOR_R(color) * a),
                    sol__div255((uint32_t)SOL_COLOR_G(color) * a),
                    sol__div255((uint32_t)SOL_COLOR_B(color) * a));
}

sol_color_t sol_color_unpremul(sol_color_t premul_color) {
    uint32_t a = SOL_COLOR_A(premul_color);
    if (a == 0u) {
        return 0u;
    }
    if (a == 255u) {
        return premul_color;
    }

    uint32_t r = ((uint32_t)SOL_COLOR_R(premul_color) * 255u + (a >> 1)) / a;
    uint32_t g = ((uint32_t)SOL_COLOR_G(premul_color) * 255u + (a >> 1)) / a;
    uint32_t b = ((uint32_t)SOL_COLOR_B(premul_color) * 255u + (a >> 1)) / a;

    return SOL_ARGB(a, (r > 255u) ? 255u : r, (g > 255u) ? 255u : g, (b > 255u) ? 255u : b);
}

sol_color_t sol_color_blend_premul(sol_color_t dst_pm, sol_color_t src_pm) {
    uint32_t sa = SOL_COLOR_A(src_pm);
    uint32_t inv_sa;

    if (sa == 0u) {
        return dst_pm;
    }
    if (sa == 255u) {
        return src_pm;
    }

    inv_sa = 255u - sa;
    uint32_t a = sa + sol__div255((uint32_t)SOL_COLOR_A(dst_pm) * inv_sa);
    uint32_t r =
        (uint32_t)SOL_COLOR_R(src_pm) + sol__div255((uint32_t)SOL_COLOR_R(dst_pm) * inv_sa);
    uint32_t g =
        (uint32_t)SOL_COLOR_G(src_pm) + sol__div255((uint32_t)SOL_COLOR_G(dst_pm) * inv_sa);
    uint32_t b =
        (uint32_t)SOL_COLOR_B(src_pm) + sol__div255((uint32_t)SOL_COLOR_B(dst_pm) * inv_sa);

    return SOL_ARGB((a > 255u) ? 255u : a, (r > 255u) ? 255u : r, (g > 255u) ? 255u : g,
                    (b > 255u) ? 255u : b);
}

sol_color_t sol_color_from_hsv(uint16_t hue_deg, uint8_t sat, uint8_t val, uint8_t alpha) {
    uint32_t h = (uint32_t)(hue_deg % 360u);
    uint32_t s = (uint32_t)sat;
    uint32_t v = (uint32_t)val;
    uint32_t reg;
    uint32_t f_num;
    uint32_t p;
    uint32_t q;
    uint32_t t;

    if (s == 0u) {
        return SOL_ARGB(alpha, v, v, v);
    }

    reg = h / 60u;
    f_num = h % 60u;

    p = (v * (255u - s) + 127u) / 255u;
    q = (v * (255u * 60u - s * f_num) + 255u * 30u) / (255u * 60u);
    t = (v * (255u * 60u - s * (60u - f_num)) + 255u * 30u) / (255u * 60u);

    switch (reg) {
    case 0:
        return SOL_ARGB(alpha, v, t, p);
    case 1:
        return SOL_ARGB(alpha, q, v, p);
    case 2:
        return SOL_ARGB(alpha, p, v, t);
    case 3:
        return SOL_ARGB(alpha, p, q, v);
    case 4:
        return SOL_ARGB(alpha, t, p, v);
    default:
        return SOL_ARGB(alpha, v, p, q);
    }
}

void sol_color_to_hsv(sol_color_t color, uint16_t *out_hue, uint8_t *out_sat, uint8_t *out_val) {
    uint32_t r = SOL_COLOR_R(color);
    uint32_t g = SOL_COLOR_G(color);
    uint32_t b = SOL_COLOR_B(color);
    uint32_t max_c = SOL_MAX(r, SOL_MAX(g, b));
    uint32_t min_c = SOL_MIN(r, SOL_MIN(g, b));
    uint32_t delta = max_c - min_c;
    int32_t h = 0;
    uint8_t s = 0u;

    if (delta > 0u) {
        s = (uint8_t)((delta * 255u + (max_c >> 1)) / max_c);
        if (max_c == r) {
            h = (60 * ((int32_t)g - (int32_t)b)) / (int32_t)delta;
            if (h < 0) {
                h += 360;
            }
        } else if (max_c == g) {
            h = 120 + (60 * ((int32_t)b - (int32_t)r)) / (int32_t)delta;
        } else {
            h = 240 + (60 * ((int32_t)r - (int32_t)g)) / (int32_t)delta;
        }
    }

    if (out_hue != NULL) {
        *out_hue = (uint16_t)h;
    }
    if (out_sat != NULL) {
        *out_sat = s;
    }
    if (out_val != NULL) {
        *out_val = (uint8_t)max_c;
    }
}

SOL_PRIVATE uint8_t sol__hue_to_rgb_ch(int32_t p255, int32_t q255, int32_t t360) {
    int32_t t = t360;
    int32_t res;

    if (t < 0) {
        t += 360;
    }
    if (t >= 360) {
        t -= 360;
    }

    if (t < 60) {
        res = p255 + ((q255 - p255) * t + 30) / 60;
    } else if (t < 180) {
        res = q255;
    } else if (t < 240) {
        res = p255 + ((q255 - p255) * (240 - t) + 30) / 60;
    } else {
        res = p255;
    }

    if (res < 0) {
        res = 0;
    } else if (res > 255) {
        res = 255;
    }
    return (uint8_t)res;
}

sol_color_t sol_color_from_hsl(uint16_t hue_deg, uint8_t sat, uint8_t light, uint8_t alpha) {
    int32_t h = (int32_t)(hue_deg % 360u);
    int32_t s = (int32_t)sat;
    int32_t l = (int32_t)light;
    int32_t q;
    int32_t p;

    if (s == 0) {
        return SOL_ARGB(alpha, l, l, l);
    }

    if (l < 128) {
        q = (l * (255 + s) + 127) / 255;
    } else {
        q = l + s - (l * s + 127) / 255;
    }
    p = 2 * l - q;

    return SOL_ARGB(alpha, sol__hue_to_rgb_ch(p, q, h + 120), sol__hue_to_rgb_ch(p, q, h),
                    sol__hue_to_rgb_ch(p, q, h - 120));
}

void sol_color_to_hsl(sol_color_t color, uint16_t *out_hue, uint8_t *out_sat, uint8_t *out_light) {
    uint32_t r = SOL_COLOR_R(color);
    uint32_t g = SOL_COLOR_G(color);
    uint32_t b = SOL_COLOR_B(color);
    uint32_t max_c = SOL_MAX(r, SOL_MAX(g, b));
    uint32_t min_c = SOL_MIN(r, SOL_MIN(g, b));
    uint32_t delta = max_c - min_c;
    uint32_t l = (max_c + min_c + 1u) >> 1;
    uint32_t s = 0u;
    int32_t h = 0;

    if (delta > 0u) {
        uint32_t denom = (l < 128u) ? (max_c + min_c) : (510u - max_c - min_c);
        if (denom > 0u) {
            s = (delta * 255u + (denom >> 1)) / denom;
            if (s > 255u) {
                s = 255u;
            }
        }

        if (max_c == r) {
            h = (60 * ((int32_t)g - (int32_t)b)) / (int32_t)delta;
            if (h < 0) {
                h += 360;
            }
        } else if (max_c == g) {
            h = 120 + (60 * ((int32_t)b - (int32_t)r)) / (int32_t)delta;
        } else {
            h = 240 + (60 * ((int32_t)r - (int32_t)g)) / (int32_t)delta;
        }
    }

    if (out_hue != NULL) {
        *out_hue = (uint16_t)h;
    }
    if (out_sat != NULL) {
        *out_sat = (uint8_t)s;
    }
    if (out_light != NULL) {
        *out_light = (uint8_t)l;
    }
}

SOL_PRIVATE int sol__hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

sol_status_t sol_color_from_hex(const char *hex, sol_color_t *out_color) {
    size_t len = 0u;
    uint32_t vals[8];

    if (hex == NULL || out_color == NULL) {
        return SOL_ERR_INVALID_ARG;
    }

    if (hex[0] == '#') {
        hex++;
    } else if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }

    while (hex[len] != '\0') {
        if (len >= 8u) {
            return SOL_ERR_INVALID_ARG;
        }
        int d = sol__hex_digit(hex[len]);
        if (d < 0) {
            return SOL_ERR_INVALID_ARG;
        }
        vals[len++] = (uint32_t)d;
    }

    if (len == 3u) {
        *out_color = SOL_RGB(vals[0] * 17u, vals[1] * 17u, vals[2] * 17u);
        return SOL_OK;
    }
    if (len == 6u) {
        *out_color =
            SOL_RGB((vals[0] << 4) | vals[1], (vals[2] << 4) | vals[3], (vals[4] << 4) | vals[5]);
        return SOL_OK;
    }
    if (len == 8u) {
        size_t i;
        uint32_t acc = 0u;
        for (i = 0u; i < 8u; ++i) {
            acc = (acc << 4) | vals[i];
        }
        *out_color = (sol_color_t)acc;
        return SOL_OK;
    }

    return SOL_ERR_INVALID_ARG;
}

sol_status_t sol_color_to_hex(sol_color_t color, char *out_buf, size_t buf_size,
                              bool include_alpha) {
    static const char k_hex[] = "0123456789ABCDEF";
    size_t needed = include_alpha ? 10u : 8u;
    size_t pos = 0u;

    if (out_buf == NULL || buf_size < needed) {
        return SOL_ERR_INVALID_ARG;
    }

    out_buf[pos++] = '#';
    if (include_alpha) {
        uint8_t a = SOL_COLOR_A(color);
        out_buf[pos++] = k_hex[(a >> 4) & 0x0Fu];
        out_buf[pos++] = k_hex[a & 0x0Fu];
    }

    {
        uint8_t r = SOL_COLOR_R(color);
        uint8_t g = SOL_COLOR_G(color);
        uint8_t b = SOL_COLOR_B(color);

        out_buf[pos++] = k_hex[(r >> 4) & 0x0Fu];
        out_buf[pos++] = k_hex[r & 0x0Fu];
        out_buf[pos++] = k_hex[(g >> 4) & 0x0Fu];
        out_buf[pos++] = k_hex[g & 0x0Fu];
        out_buf[pos++] = k_hex[(b >> 4) & 0x0Fu];
        out_buf[pos++] = k_hex[b & 0x0Fu];
        out_buf[pos] = '\0';
    }
    return SOL_OK;
}

uint8_t sol_color_luminance(sol_color_t color) {
    uint64_t lr = (uint64_t)sol_srgb_to_linear(SOL_COLOR_R(color));
    uint64_t lg = (uint64_t)sol_srgb_to_linear(SOL_COLOR_G(color));
    uint64_t lb = (uint64_t)sol_srgb_to_linear(SOL_COLOR_B(color));
    uint64_t lin_lum = (2126ull * lr + 7152ull * lg + 722ull * lb + 5000ull) / 10000ull;

    return sol_linear_to_srgb((lin_lum > 65535ull) ? 65535u : (uint16_t)lin_lum);
}

uint32_t sol_color_contrast_x100(sol_color_t a, sol_color_t b) {
    uint64_t la = (2126ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_R(a)) +
                   7152ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_G(a)) +
                   722ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_B(a)) + 5000ull) /
                  10000ull;
    uint64_t lb = (2126ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_R(b)) +
                   7152ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_G(b)) +
                   722ull * (uint64_t)sol_srgb_to_linear(SOL_COLOR_B(b)) + 5000ull) /
                  10000ull;

    uint64_t l1 = (la > lb) ? la : lb;
    uint64_t l2 = (la > lb) ? lb : la;
    uint64_t num = 100ull * (l1 + 3277ull);
    uint64_t den = l2 + 3277ull;

    return (uint32_t)((num + (den >> 1)) / den);
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

void sol_blend_pixel_mode(sol_surface_t *surf, int32_t x, int32_t y, sol_color_t color,
                          sol_blend_mode_t mode) {
    size_t idx;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }
    if (x < surf->clip.x || y < surf->clip.y || x >= surf->clip.x + surf->clip.w ||
        y >= surf->clip.y + surf->clip.h) {
        return;
    }

    idx = (size_t)y * (size_t)surf->stride + (size_t)x;
    surf->pixels[idx] = sol_color_blend_mode(surf->pixels[idx], color, mode);
}

void sol_fill_rect_blend(sol_surface_t *surf, sol_rect_t rect, sol_color_t color,
                         sol_blend_mode_t mode) {
    sol_rect_t r;
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    for (row = 0; row < r.h; ++row) {
        uint32_t *dst = surf->pixels + (size_t)(r.y + row) * (size_t)surf->stride + (size_t)r.x;
        for (col = 0; col < r.w; ++col) {
            dst[col] = sol_color_blend_mode(dst[col], color, mode);
        }
    }
}

sol_color_t sol_gradient_eval(const sol_gradient_stop_t *stops, size_t count, uint8_t t,
                              bool linear_gamma) {
    size_t i;

    if (stops == NULL || count == 0u) {
        return SOL_COLOR_TRANSPARENT;
    }
    if (count == 1u || t <= stops[0].pos) {
        return stops[0].color;
    }
    if (t >= stops[count - 1u].pos) {
        return stops[count - 1u].color;
    }

    for (i = 0u; i + 1u < count; ++i) {
        uint8_t p0 = stops[i].pos;
        uint8_t p1 = stops[i + 1u].pos;
        if (t >= p0 && t <= p1) {
            if (p1 <= p0) {
                return stops[i + 1u].color;
            }
            uint8_t local_t = (uint8_t)(((uint32_t)(t - p0) * 255u) / (uint32_t)(p1 - p0));
            return linear_gamma
                       ? sol_color_lerp_linear(stops[i].color, stops[i + 1u].color, local_t)
                       : sol_color_lerp(stops[i].color, stops[i + 1u].color, local_t);
        }
    }

    return stops[count - 1u].color;
}

void sol_fill_rect_gradient_linear(sol_surface_t *surf, sol_rect_t rect, int32_t x0, int32_t y0,
                                   int32_t x1, int32_t y1, const sol_gradient_stop_t *stops,
                                   size_t stop_count, bool linear_gamma) {
    sol_rect_t r;
    int64_t dx;
    int64_t dy;
    int64_t len2;
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL || stops == NULL || stop_count == 0u) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    dx = (int64_t)x1 - (int64_t)x0;
    dy = (int64_t)y1 - (int64_t)y0;
    len2 = dx * dx + dy * dy;

    if (len2 == 0) {
        sol_fill_rect(surf, r, stops[0].color);
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        uint32_t *dst = surf->pixels + (size_t)row * (size_t)surf->stride;
        int64_t py_term = ((int64_t)row - (int64_t)y0) * dy;

        for (col = r.x; col < r.x + r.w; ++col) {
            int64_t proj = ((int64_t)col - (int64_t)x0) * dx + py_term;
            uint8_t t;
            if (proj <= 0) {
                t = 0u;
            } else if (proj >= len2) {
                t = 255u;
            } else {
                t = (uint8_t)((proj * 255) / len2);
            }
            sol_color_t c = sol_gradient_eval(stops, stop_count, t, linear_gamma);
            dst[col] = sol__blend_argb_fast(dst[col], c);
        }
    }
}

void sol_fill_rect_gradient_radial(sol_surface_t *surf, sol_rect_t rect, int32_t cx, int32_t cy,
                                   int32_t radius, const sol_gradient_stop_t *stops,
                                   size_t stop_count, bool linear_gamma) {
    sol_rect_t r;
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL || stops == NULL || stop_count == 0u) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    if (radius <= 0) {
        sol_fill_rect(surf, r, stops[stop_count - 1u].color);
        return;
    }

    for (row = r.y; row < r.y + r.h; ++row) {
        uint32_t *dst = surf->pixels + (size_t)row * (size_t)surf->stride;
        int64_t dy = (int64_t)row - (int64_t)cy;
        uint64_t dy2 = (uint64_t)(dy * dy);

        for (col = r.x; col < r.x + r.w; ++col) {
            int64_t dx = (int64_t)col - (int64_t)cx;
            uint64_t dist = (uint64_t)sol__isqrt64((uint64_t)(dx * dx) + dy2);
            uint8_t t =
                (dist >= (uint64_t)radius) ? 255u : (uint8_t)((dist * 255ull) / (uint64_t)radius);
            sol_color_t c = sol_gradient_eval(stops, stop_count, t, linear_gamma);
            dst[col] = sol__blend_argb_fast(dst[col], c);
        }
    }
}

void sol_fill_rect_gradient_4(sol_surface_t *surf, sol_rect_t rect, sol_color_t tl, sol_color_t tr,
                              sol_color_t br, sol_color_t bl) {
    sol_rect_t r;
    int32_t denom_w;
    int32_t denom_h;
    int32_t row;
    int32_t col;

    if (surf == NULL || surf->pixels == NULL || rect.w <= 0 || rect.h <= 0) {
        return;
    }

    r = sol_rect_intersect(rect, surf->clip);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    denom_w = (rect.w > 1) ? (rect.w - 1) : 1;
    denom_h = (rect.h > 1) ? (rect.h - 1) : 1;

    for (row = r.y; row < r.y + r.h; ++row) {
        uint8_t ty = (uint8_t)(((int64_t)(row - rect.y) * 255) / denom_h);
        sol_color_t left_c = sol_color_lerp(tl, bl, ty);
        sol_color_t right_c = sol_color_lerp(tr, br, ty);
        uint32_t *dst = surf->pixels + (size_t)row * (size_t)surf->stride;

        for (col = r.x; col < r.x + r.w; ++col) {
            uint8_t tx = (uint8_t)(((int64_t)(col - rect.x) * 255) / denom_w);
            sol_color_t c = sol_color_lerp(left_c, right_c, tx);
            dst[col] = sol__blend_argb_fast(dst[col], c);
        }
    }
}
