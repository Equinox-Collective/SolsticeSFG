#include "sol_internal.h"

SOL_PRIVATE int64_t sol__abs64(int64_t v) {
    return (v < 0) ? -v : v;
}

SOL_PRIVATE int32_t sol__round_fp8_to_i32(int64_t v) {
    return (v >= 0) ? (int32_t)((v + 128) >> 8) : (int32_t)(-(((-v) + 128) >> 8));
}

/* ============================================================================
 * Iterative Bezier Subdivision (24.8 Fixed-Point, zero recursion, zero floats)
 * ============================================================================ */

typedef struct sol__quad_seg {
    int64_t x0, y0, cx, cy, x1, y1;
    int depth;
} sol__quad_seg_t;

typedef struct sol__cubic_seg {
    int64_t x0, y0, c1x, c1y, c2x, c2y, x1, y1;
    int depth;
} sol__cubic_seg_t;

SOL_PRIVATE void sol__emit_line_cb(sol_surface_t *surf, sol_path_t *path, int32_t x0, int32_t y0,
                                   int32_t x1, int32_t y1, sol_color_t color, bool aa) {
    if (path != NULL) {
        sol_path_line_to(path, x1, y1);
    } else if (surf != NULL) {
        if (aa) {
            sol_draw_line_aa(surf, x0, y0, x1, y1, color);
        } else {
            sol_draw_line(surf, x0, y0, x1, y1, color);
        }
    }
}

SOL_PRIVATE void sol__flatten_quad(sol_surface_t *surf, sol_path_t *path, int32_t ix0, int32_t iy0,
                                   int32_t icx, int32_t icy, int32_t ix1, int32_t iy1,
                                   sol_color_t color, bool aa) {
    sol__quad_seg_t stack[16];
    int sp = 0;

    stack[sp].x0 = (int64_t)ix0 << 8;
    stack[sp].y0 = (int64_t)iy0 << 8;
    stack[sp].cx = (int64_t)icx << 8;
    stack[sp].cy = (int64_t)icy << 8;
    stack[sp].x1 = (int64_t)ix1 << 8;
    stack[sp].y1 = (int64_t)iy1 << 8;
    stack[sp].depth = 0;
    sp++;

    while (sp > 0) {
        sol__quad_seg_t seg = stack[--sp];
        /* Midpoint deviation from chord: (x0 + x1)/2 - (x0 + 2*cx + x1)/4 = (x0 - 2*cx + x1)/4 */
        int64_t dev_x = sol__abs64(seg.x0 - 2 * seg.cx + seg.x1);
        int64_t dev_y = sol__abs64(seg.y0 - 2 * seg.cy + seg.y1);

        if ((dev_x + dev_y) <= 256 || seg.depth >= 10 || sp >= 14) {
            sol__emit_line_cb(surf, path, sol__round_fp8_to_i32(seg.x0),
                              sol__round_fp8_to_i32(seg.y0), sol__round_fp8_to_i32(seg.x1),
                              sol__round_fp8_to_i32(seg.y1), color, aa);
        } else {
            int64_t q0x = (seg.x0 + seg.cx) >> 1;
            int64_t q0y = (seg.y0 + seg.cy) >> 1;
            int64_t q1x = (seg.cx + seg.x1) >> 1;
            int64_t q1y = (seg.cy + seg.y1) >> 1;
            int64_t mx = (q0x + q1x) >> 1;
            int64_t my = (q0y + q1y) >> 1;

            /* Push right half first so left half pops and emits in order */
            stack[sp].x0 = mx;
            stack[sp].y0 = my;
            stack[sp].cx = q1x;
            stack[sp].cy = q1y;
            stack[sp].x1 = seg.x1;
            stack[sp].y1 = seg.y1;
            stack[sp].depth = seg.depth + 1;
            sp++;

            stack[sp].x0 = seg.x0;
            stack[sp].y0 = seg.y0;
            stack[sp].cx = q0x;
            stack[sp].cy = q0y;
            stack[sp].x1 = mx;
            stack[sp].y1 = my;
            stack[sp].depth = seg.depth + 1;
            sp++;
        }
    }
}

SOL_PRIVATE void sol__flatten_cubic(sol_surface_t *surf, sol_path_t *path, int32_t ix0, int32_t iy0,
                                    int32_t ic1x, int32_t ic1y, int32_t ic2x, int32_t ic2y,
                                    int32_t ix1, int32_t iy1, sol_color_t color, bool aa) {
    sol__cubic_seg_t stack[16];
    int sp = 0;

    stack[sp].x0 = (int64_t)ix0 << 8;
    stack[sp].y0 = (int64_t)iy0 << 8;
    stack[sp].c1x = (int64_t)ic1x << 8;
    stack[sp].c1y = (int64_t)ic1y << 8;
    stack[sp].c2x = (int64_t)ic2x << 8;
    stack[sp].c2y = (int64_t)ic2y << 8;
    stack[sp].x1 = (int64_t)ix1 << 8;
    stack[sp].y1 = (int64_t)iy1 << 8;
    stack[sp].depth = 0;
    sp++;

    while (sp > 0) {
        sol__cubic_seg_t seg = stack[--sp];
        /* Wang / cubic flatness bound: max(|c1 - (2*x0+x1)/3|, |c2 - (x0+2*x1)/3|) */
        int64_t d1x = sol__abs64(3 * seg.c1x - 2 * seg.x0 - seg.x1);
        int64_t d1y = sol__abs64(3 * seg.c1y - 2 * seg.y0 - seg.y1);
        int64_t d2x = sol__abs64(3 * seg.c2x - seg.x0 - 2 * seg.x1);
        int64_t d2y = sol__abs64(3 * seg.c2y - seg.y0 - 2 * seg.y1);
        int64_t err = SOL_MAX(d1x + d1y, d2x + d2y);

        if (err <= 384 || seg.depth >= 10 || sp >= 14) {
            sol__emit_line_cb(surf, path, sol__round_fp8_to_i32(seg.x0),
                              sol__round_fp8_to_i32(seg.y0), sol__round_fp8_to_i32(seg.x1),
                              sol__round_fp8_to_i32(seg.y1), color, aa);
        } else {
            int64_t x01 = (seg.x0 + seg.c1x) >> 1;
            int64_t y01 = (seg.y0 + seg.c1y) >> 1;
            int64_t x12 = (seg.c1x + seg.c2x) >> 1;
            int64_t y12 = (seg.c1y + seg.c2y) >> 1;
            int64_t x23 = (seg.c2x + seg.x1) >> 1;
            int64_t y23 = (seg.c2y + seg.y1) >> 1;
            int64_t x012 = (x01 + x12) >> 1;
            int64_t y012 = (y01 + y12) >> 1;
            int64_t x123 = (x12 + x23) >> 1;
            int64_t y123 = (y12 + y23) >> 1;
            int64_t mx = (x012 + x123) >> 1;
            int64_t my = (y012 + y123) >> 1;

            stack[sp].x0 = mx;
            stack[sp].y0 = my;
            stack[sp].c1x = x123;
            stack[sp].c1y = y123;
            stack[sp].c2x = x23;
            stack[sp].c2y = y23;
            stack[sp].x1 = seg.x1;
            stack[sp].y1 = seg.y1;
            stack[sp].depth = seg.depth + 1;
            sp++;

            stack[sp].x0 = seg.x0;
            stack[sp].y0 = seg.y0;
            stack[sp].c1x = x01;
            stack[sp].c1y = y01;
            stack[sp].c2x = x012;
            stack[sp].c2y = y012;
            stack[sp].x1 = mx;
            stack[sp].y1 = my;
            stack[sp].depth = seg.depth + 1;
            sp++;
        }
    }
}

void sol_draw_quad_bezier(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t cx, int32_t cy,
                          int32_t x1, int32_t y1, sol_color_t color, bool aa) {
    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }
    sol__flatten_quad(surf, NULL, x0, y0, cx, cy, x1, y1, color, aa);
}

void sol_draw_cubic_bezier(sol_surface_t *surf, int32_t x0, int32_t y0, int32_t c1x, int32_t c1y,
                           int32_t c2x, int32_t c2y, int32_t x1, int32_t y1, sol_color_t color,
                           bool aa) {
    if (surf == NULL || surf->pixels == NULL || SOL_COLOR_A(color) == 0u) {
        return;
    }
    sol__flatten_cubic(surf, NULL, x0, y0, c1x, c1y, c2x, c2y, x1, y1, color, aa);
}

/* ============================================================================
 * Arena-Backed Path Builder
 * ============================================================================ */

SOL_PRIVATE sol_status_t sol__path_append_pt(sol_path_t *path, int32_t x, int32_t y) {
    sol_path_subpath_t *sub = path->curr;
    sol_path_chunk_t *chunk;

    if (sub == NULL) {
        return SOL_ERR_INVALID_ARG;
    }

    if (sub->tail != NULL && sub->tail->count > 0u) {
        sol_point_t last = sub->tail->pts[sub->tail->count - 1u];
        if (last.x == x && last.y == y) {
            return SOL_OK;
        }
    }

    if (sub->tail == NULL || sub->tail->count >= SOL_PATH_CHUNK_CAP) {
        chunk = (sol_path_chunk_t *)sol_arena_alloc(path->arena, sizeof(sol_path_chunk_t));
        if (chunk == NULL) {
            path->oom = true;
            return SOL_ERR_OUT_OF_BOUNDS;
        }
        chunk->count = 0u;
        chunk->next = NULL;

        if (sub->tail == NULL) {
            sub->head = chunk;
            sub->tail = chunk;
        } else {
            sub->tail->next = chunk;
            sub->tail = chunk;
        }
    }

    sub->tail->pts[sub->tail->count++] = SOL_POINT(x, y);
    sub->point_count++;
    path->curr_pt = SOL_POINT(x, y);
    path->has_curr = true;
    return SOL_OK;
}

sol_status_t sol_path_init(sol_path_t *path, sol_arena_t *arena) {
    if (path == NULL || arena == NULL) {
        return SOL_ERR_INVALID_ARG;
    }

    path->arena = arena;
    path->subpaths = NULL;
    path->curr = NULL;
    path->curr_pt = SOL_POINT(0, 0);
    path->has_curr = false;
    path->oom = false;
    return SOL_OK;
}

void sol_path_reset(sol_path_t *path) {
    if (path == NULL) {
        return;
    }
    path->subpaths = NULL;
    path->curr = NULL;
    path->curr_pt = SOL_POINT(0, 0);
    path->has_curr = false;
    path->oom = false;
}

sol_status_t sol_path_move_to(sol_path_t *path, int32_t x, int32_t y) {
    sol_path_subpath_t *sub;

    if (path == NULL || path->arena == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (path->oom) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    sub = (sol_path_subpath_t *)sol_arena_alloc(path->arena, sizeof(sol_path_subpath_t));
    if (sub == NULL) {
        path->oom = true;
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    sub->head = NULL;
    sub->tail = NULL;
    sub->point_count = 0u;
    sub->closed = false;
    sub->next = NULL;

    if (path->subpaths == NULL) {
        path->subpaths = sub;
        path->curr = sub;
    } else {
        path->curr->next = sub;
        path->curr = sub;
    }

    return sol__path_append_pt(path, x, y);
}

sol_status_t sol_path_line_to(sol_path_t *path, int32_t x, int32_t y) {
    if (path == NULL || path->arena == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (path->oom) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    if (path->curr == NULL || !path->has_curr) {
        return sol_path_move_to(path, x, y);
    }
    return sol__path_append_pt(path, x, y);
}

sol_status_t sol_path_quad_to(sol_path_t *path, int32_t cx, int32_t cy, int32_t x, int32_t y) {
    if (path == NULL || path->arena == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (path->oom) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    if (!path->has_curr) {
        return sol_path_move_to(path, x, y);
    }
    sol__flatten_quad(NULL, path, path->curr_pt.x, path->curr_pt.y, cx, cy, x, y, 0u, false);
    return path->oom ? SOL_ERR_OUT_OF_BOUNDS : SOL_OK;
}

sol_status_t sol_path_cubic_to(sol_path_t *path, int32_t c1x, int32_t c1y, int32_t c2x, int32_t c2y,
                               int32_t x, int32_t y) {
    if (path == NULL || path->arena == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    if (path->oom) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }
    if (!path->has_curr) {
        return sol_path_move_to(path, x, y);
    }
    sol__flatten_cubic(NULL, path, path->curr_pt.x, path->curr_pt.y, c1x, c1y, c2x, c2y, x, y, 0u,
                       false);
    return path->oom ? SOL_ERR_OUT_OF_BOUNDS : SOL_OK;
}

sol_status_t sol_path_arc_to(sol_path_t *path, int32_t cx, int32_t cy, int32_t rx, int32_t ry,
                             int32_t start_deg, int32_t end_deg) {
    int32_t span;
    int32_t steps;
    int32_t i;

    if (path == NULL || path->arena == NULL || rx < 0 || ry < 0) {
        return SOL_ERR_INVALID_ARG;
    }
    if (path->oom) {
        return SOL_ERR_OUT_OF_BOUNDS;
    }

    span = end_deg - start_deg;
    {
        int32_t abs_span = (span < 0) ? -span : span;
        int32_t max_r = SOL_MAX(rx, ry);
        steps = SOL_MAX(4, (abs_span * SOL_MAX(max_r, 8)) / 240);
        if (steps > 128) {
            steps = 128;
        }
    }

    for (i = 0; i <= steps; ++i) {
        int32_t deg = start_deg + (span * i) / steps;
        int32_t px = cx + (int32_t)(((int64_t)rx * (int64_t)sol__cos_deg_fp16(deg)) >> 16);
        int32_t py = cy + (int32_t)(((int64_t)ry * (int64_t)sol__sin_deg_fp16(deg)) >> 16);
        sol_status_t st = (i == 0 && !path->has_curr) ? sol_path_move_to(path, px, py)
                                                      : sol_path_line_to(path, px, py);
        if (st != SOL_OK) {
            return st;
        }
    }
    return SOL_OK;
}

sol_status_t sol_path_close(sol_path_t *path) {
    if (path == NULL || path->curr == NULL) {
        return SOL_ERR_INVALID_ARG;
    }
    path->curr->closed = true;
    if (path->curr->head != NULL && path->curr->head->count > 0u) {
        path->curr_pt = path->curr->head->pts[0];
    }
    return SOL_OK;
}

/* ============================================================================
 * Scanline Polygon & Path Rasterizer (Even-Odd & Non-Zero + 4x Subpixel AA)
 * ============================================================================ */

typedef struct sol__poly_edge {
    int32_t x0_fp8;
    int32_t y0_sub;
    int32_t x1_fp8;
    int32_t y1_sub;
    int32_t dir;
} sol__poly_edge_t;

typedef struct sol__poly_hit {
    int32_t x_fp8;
    int32_t dir;
} sol__poly_hit_t;

SOL_PRIVATE void sol__add_edge(sol__poly_edge_t *edges, size_t *edge_count, int32_t x0, int32_t y0,
                               int32_t x1, int32_t y1, int32_t sub_scale, int32_t *min_y,
                               int32_t *max_y) {
    int32_t sy0 = y0 * sub_scale;
    int32_t sy1 = y1 * sub_scale;
    int32_t sx0 = x0 * 256 + 128;
    int32_t sx1 = x1 * 256 + 128;
    int32_t dir = 1;

    if (sy0 == sy1) {
        return;
    }
    if (sy0 > sy1) {
        int32_t tx = sx0;
        int32_t ty = sy0;
        sx0 = sx1;
        sy0 = sy1;
        sx1 = tx;
        sy1 = ty;
        dir = -1;
    }

    if (y0 < *min_y) {
        *min_y = y0;
    }
    if (y1 < *min_y) {
        *min_y = y1;
    }
    if (y0 > *max_y) {
        *max_y = y0;
    }
    if (y1 > *max_y) {
        *max_y = y1;
    }

    edges[*edge_count].x0_fp8 = sx0;
    edges[*edge_count].y0_sub = sy0;
    edges[*edge_count].x1_fp8 = sx1;
    edges[*edge_count].y1_sub = sy1;
    edges[*edge_count].dir = dir;
    (*edge_count)++;
}

SOL_PRIVATE void sol__sort_hits(sol__poly_hit_t *hits, size_t count) {
    size_t i;
    for (i = 1u; i < count; ++i) {
        sol__poly_hit_t key = hits[i];
        size_t j = i;
        while (j > 0u && hits[j - 1u].x_fp8 > key.x_fp8) {
            hits[j] = hits[j - 1u];
            j--;
        }
        hits[j] = key;
    }
}

SOL_PRIVATE void sol__rasterize_edges(sol_surface_t *surf, const sol__poly_edge_t *edges,
                                      size_t edge_count, int32_t min_y, int32_t max_y,
                                      int32_t sub_scale, sol_fill_rule_t rule, sol_color_t color,
                                      sol__poly_hit_t *hits, uint8_t *cov_row, int32_t cov_cap,
                                      bool aa) {
    int32_t clip_x0 = surf->clip.x;
    int32_t clip_x1 = surf->clip.x + surf->clip.w - 1;
    int32_t clip_y0 = surf->clip.y;
    int32_t clip_y1 = surf->clip.y + surf->clip.h - 1;
    int32_t y_start = SOL_MAX(min_y, clip_y0);
    int32_t y_end = SOL_MIN(max_y, clip_y1);
    int32_t row_w = clip_x1 - clip_x0 + 1;
    int32_t y;

    if (edge_count == 0u || y_start > y_end || row_w <= 0) {
        return;
    }
    if (row_w > cov_cap) {
        row_w = cov_cap;
    }

    for (y = y_start; y <= y_end; ++y) {
        int32_t sy;
        int32_t col;

        SOL_MEMSET(cov_row, 0, (size_t)row_w);

        for (sy = 0; sy < sub_scale; ++sy) {
            int32_t sub_y = y * sub_scale + sy;
            size_t hit_count = 0u;
            size_t e;
            size_t h;
            int32_t winding = 0;
            bool inside = false;
            int32_t span_start_fp8 = 0;

            for (e = 0u; e < edge_count; ++e) {
                if (sub_y >= edges[e].y0_sub && sub_y < edges[e].y1_sub) {
                    int64_t dy = (int64_t)edges[e].y1_sub - (int64_t)edges[e].y0_sub;
                    int64_t dx = (int64_t)edges[e].x1_fp8 - (int64_t)edges[e].x0_fp8;
                    int64_t x_fp8 = (int64_t)edges[e].x0_fp8 +
                                    (dx * ((int64_t)sub_y - (int64_t)edges[e].y0_sub)) / dy;
                    hits[hit_count].x_fp8 = (int32_t)x_fp8;
                    hits[hit_count].dir = edges[e].dir;
                    hit_count++;
                }
            }

            if (hit_count < 2u) {
                continue;
            }

            sol__sort_hits(hits, hit_count);

            for (h = 0u; h < hit_count; ++h) {
                bool next_inside;
                if (rule == SOL_FILL_EVEN_ODD) {
                    winding ^= 1;
                    next_inside = (winding != 0);
                } else {
                    winding += hits[h].dir;
                    next_inside = (winding != 0);
                }

                if (!inside && next_inside) {
                    span_start_fp8 = hits[h].x_fp8;
                    inside = true;
                } else if (inside && !next_inside) {
                    int32_t x0_fp8 = span_start_fp8;
                    int32_t x1_fp8 = hits[h].x_fp8;
                    inside = false;

                    if (x1_fp8 > x0_fp8) {
                        if (!aa) {
                            int32_t px0 = (x0_fp8 >> 8);
                            int32_t px1 = (x1_fp8 >> 8);
                            if (px0 < clip_x0) {
                                px0 = clip_x0;
                            }
                            if (px1 > clip_x0 + row_w - 1) {
                                px1 = clip_x0 + row_w - 1;
                            }
                            for (col = px0; col <= px1; ++col) {
                                cov_row[col - clip_x0] = 255u;
                            }
                        } else {
                            int32_t px0 = (x0_fp8 >= 0) ? (x0_fp8 >> 8) : -(((-x0_fp8) + 255) >> 8);
                            int32_t px1 = (x1_fp8 >= 0) ? (x1_fp8 >> 8) : -(((-x1_fp8) + 255) >> 8);
                            uint8_t step_cov = (uint8_t)(255 / sub_scale);

                            if (px0 == px1) {
                                if (px0 >= clip_x0 && px0 < clip_x0 + row_w) {
                                    uint32_t add = ((uint32_t)(x1_fp8 - x0_fp8) * step_cov) >> 8;
                                    uint32_t sum = (uint32_t)cov_row[px0 - clip_x0] + add;
                                    cov_row[px0 - clip_x0] = (sum > 255u) ? 255u : (uint8_t)sum;
                                }
                            } else {
                                int32_t solid_start = px0 + 1;
                                int32_t solid_end = px1 - 1;

                                if (px0 >= clip_x0 && px0 < clip_x0 + row_w) {
                                    uint32_t frac0 = (uint32_t)(256 - (x0_fp8 & 0xFF));
                                    uint32_t add0 = (frac0 * step_cov) >> 8;
                                    uint32_t sum0 = (uint32_t)cov_row[px0 - clip_x0] + add0;
                                    cov_row[px0 - clip_x0] = (sum0 > 255u) ? 255u : (uint8_t)sum0;
                                }
                                if (px1 >= clip_x0 && px1 < clip_x0 + row_w) {
                                    uint32_t frac1 = (uint32_t)(x1_fp8 & 0xFF);
                                    uint32_t add1 = (frac1 * step_cov) >> 8;
                                    uint32_t sum1 = (uint32_t)cov_row[px1 - clip_x0] + add1;
                                    cov_row[px1 - clip_x0] = (sum1 > 255u) ? 255u : (uint8_t)sum1;
                                }

                                if (solid_start < clip_x0) {
                                    solid_start = clip_x0;
                                }
                                if (solid_end > clip_x0 + row_w - 1) {
                                    solid_end = clip_x0 + row_w - 1;
                                }
                                for (col = solid_start; col <= solid_end; ++col) {
                                    uint32_t sum = (uint32_t)cov_row[col - clip_x0] + step_cov;
                                    cov_row[col - clip_x0] = (sum > 255u) ? 255u : (uint8_t)sum;
                                }
                            }
                        }
                    }
                }
            }
        }

        for (col = 0; col < row_w; ++col) {
            uint8_t c = cov_row[col];
            if (c > 0u) {
                /* Snap near-full 4x coverage (e.g. 4*63=252) cleanly to 255 */
                if (c >= 252u) {
                    c = 255u;
                }
                sol__plot_cov(surf, clip_x0 + col, y, color, c);
            }
        }
    }
}

void sol_fill_polygon(sol_surface_t *surf, const sol_point_t *pts, size_t count,
                      sol_fill_rule_t rule, sol_color_t color, sol_arena_t *scratch, bool aa) {
    sol__poly_edge_t stack_edges[64];
    sol__poly_hit_t stack_hits[64];
    uint8_t stack_cov[512];
    sol__poly_edge_t *edges = stack_edges;
    sol__poly_hit_t *hits = stack_hits;
    uint8_t *cov_row = stack_cov;
    int32_t cov_cap = (int32_t)sizeof(stack_cov);
    size_t mark = 0u;
    size_t edge_count = 0u;
    int32_t min_y = INT32_MAX;
    int32_t max_y = INT32_MIN;
    int32_t sub_scale = aa ? 4 : 1;
    size_t i;

    if (surf == NULL || surf->pixels == NULL || pts == NULL || count < 3u ||
        SOL_COLOR_A(color) == 0u || surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    if (scratch != NULL) {
        mark = sol_arena_mark(scratch);
        edges = (sol__poly_edge_t *)sol_arena_calloc(scratch, count, sizeof(sol__poly_edge_t));
        hits = (sol__poly_hit_t *)sol_arena_calloc(scratch, count, sizeof(sol__poly_hit_t));
        cov_row = (uint8_t *)sol_arena_alloc(scratch, (size_t)surf->clip.w);
        if (edges == NULL || hits == NULL || cov_row == NULL) {
            sol_arena_rewind(scratch, mark);
            return;
        }
        cov_cap = surf->clip.w;
    } else if (count > 64u) {
        return;
    }

    for (i = 0u; i < count; ++i) {
        size_t next = (i + 1u == count) ? 0u : (i + 1u);
        sol__add_edge(edges, &edge_count, pts[i].x, pts[i].y, pts[next].x, pts[next].y, sub_scale,
                      &min_y, &max_y);
    }

    sol__rasterize_edges(surf, edges, edge_count, min_y, max_y, sub_scale, rule, color, hits,
                         cov_row, cov_cap, aa);

    if (scratch != NULL) {
        sol_arena_rewind(scratch, mark);
    }
}

void sol_fill_path(sol_surface_t *surf, const sol_path_t *path, sol_fill_rule_t rule,
                   sol_color_t color, sol_arena_t *scratch, bool aa) {
    size_t total_edges = 0u;
    const sol_path_subpath_t *sub;
    size_t mark;
    sol__poly_edge_t *edges;
    sol__poly_hit_t *hits;
    uint8_t *cov_row;
    size_t edge_count = 0u;
    int32_t min_y = INT32_MAX;
    int32_t max_y = INT32_MIN;
    int32_t sub_scale = aa ? 4 : 1;

    if (surf == NULL || surf->pixels == NULL || path == NULL || scratch == NULL ||
        SOL_COLOR_A(color) == 0u || surf->clip.w <= 0 || surf->clip.h <= 0) {
        return;
    }

    for (sub = path->subpaths; sub != NULL; sub = sub->next) {
        if (sub->point_count >= 3u) {
            total_edges += sub->point_count;
        }
    }
    if (total_edges < 3u) {
        return;
    }

    mark = sol_arena_mark(scratch);
    edges = (sol__poly_edge_t *)sol_arena_calloc(scratch, total_edges, sizeof(sol__poly_edge_t));
    hits = (sol__poly_hit_t *)sol_arena_calloc(scratch, total_edges, sizeof(sol__poly_hit_t));
    cov_row = (uint8_t *)sol_arena_alloc(scratch, (size_t)surf->clip.w);
    if (edges == NULL || hits == NULL || cov_row == NULL) {
        sol_arena_rewind(scratch, mark);
        return;
    }

    for (sub = path->subpaths; sub != NULL; sub = sub->next) {
        const sol_path_chunk_t *ch;
        sol_point_t first_pt = SOL_POINT(0, 0);
        sol_point_t prev_pt = SOL_POINT(0, 0);
        bool has_first = false;
        size_t i;

        if (sub->point_count < 3u) {
            continue;
        }
        for (ch = sub->head; ch != NULL; ch = ch->next) {
            for (i = 0u; i < ch->count; ++i) {
                sol_point_t pt = ch->pts[i];
                if (!has_first) {
                    first_pt = pt;
                    prev_pt = pt;
                    has_first = true;
                } else {
                    sol__add_edge(edges, &edge_count, prev_pt.x, prev_pt.y, pt.x, pt.y, sub_scale,
                                  &min_y, &max_y);
                    prev_pt = pt;
                }
            }
        }
        if (has_first) {
            sol__add_edge(edges, &edge_count, prev_pt.x, prev_pt.y, first_pt.x, first_pt.y,
                          sub_scale, &min_y, &max_y);
        }
    }

    sol__rasterize_edges(surf, edges, edge_count, min_y, max_y, sub_scale, rule, color, hits,
                         cov_row, surf->clip.w, aa);
    sol_arena_rewind(scratch, mark);
}

/* ============================================================================
 * Stroke Expander (Miter / Bevel / Round Joins + Butt / Round / Square Caps)
 * ============================================================================ */

SOL_PRIVATE sol_point_t sol__unit_normal_fp12(sol_point_t a, sol_point_t b, int64_t half_w_fp8) {
    int64_t dx = (int64_t)b.x - (int64_t)a.x;
    int64_t dy = (int64_t)b.y - (int64_t)a.y;
    int64_t len2 = dx * dx + dy * dy;
    uint32_t len_fp8;

    if (len2 == 0) {
        return SOL_POINT(0, 0);
    }
    len_fp8 = sol__isqrt64((uint64_t)len2 << 16);
    if (len_fp8 == 0u) {
        return SOL_POINT(0, 0);
    }

    /* Left-hand normal (-dy, dx) scaled by half_w */
    {
        int64_t nx = (-dy * half_w_fp8) / (int64_t)len_fp8;
        int64_t ny = (dx * half_w_fp8) / (int64_t)len_fp8;
        return SOL_POINT((int32_t)nx, (int32_t)ny);
    }
}

SOL_PRIVATE void sol__emit_round_arc(sol_path_t *dst, sol_point_t center, sol_point_t n_from,
                                     sol_point_t n_to, int32_t half_w, bool ccw) {
    int64_t cross = (int64_t)n_from.x * (int64_t)n_to.y - (int64_t)n_from.y * (int64_t)n_to.x;
    int32_t steps = SOL_MIN(12, SOL_MAX(4, half_w / 2));
    int32_t i;

    if ((ccw && cross >= 0) || (!ccw && cross <= 0)) {
        sol_path_line_to(dst, center.x + n_to.x, center.y + n_to.y);
        return;
    }

    for (i = 1; i < steps; ++i) {
        /* Interpolate vector and re-normalize to half_w */
        int64_t vx = (int64_t)n_from.x * (steps - i) + (int64_t)n_to.x * i;
        int64_t vy = (int64_t)n_from.y * (steps - i) + (int64_t)n_to.y * i;
        uint32_t vlen = sol__isqrt64((uint64_t)(vx * vx + vy * vy));
        if (vlen > 0u) {
            int32_t px = center.x + (int32_t)((vx * half_w) / (int64_t)vlen);
            int32_t py = center.y + (int32_t)((vy * half_w) / (int64_t)vlen);
            sol_path_line_to(dst, px, py);
        }
    }
    sol_path_line_to(dst, center.x + n_to.x, center.y + n_to.y);
}

SOL_PRIVATE void sol__emit_join_side(sol_path_t *dst, sol_point_t pt, sol_point_t n_in,
                                     sol_point_t n_out, int32_t half_w, sol_line_join_t join,
                                     bool outer_side) {
    if (!outer_side || join == SOL_JOIN_BEVEL) {
        sol_path_line_to(dst, pt.x + n_in.x, pt.y + n_in.y);
        sol_path_line_to(dst, pt.x + n_out.x, pt.y + n_out.y);
        return;
    }

    if (join == SOL_JOIN_ROUND) {
        sol_path_line_to(dst, pt.x + n_in.x, pt.y + n_in.y);
        {
            int64_t cross = (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
            sol__emit_round_arc(dst, pt, n_in, n_out, half_w, cross < 0);
        }
        return;
    }

    /* SOL_JOIN_MITER: intersect offset edges with 4x half_w miter limit */
    {
        int64_t denom = (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
        int64_t hw2 = (int64_t)half_w * (int64_t)half_w;
        int64_t dot = (int64_t)n_in.x * (int64_t)n_out.x + (int64_t)n_in.y * (int64_t)n_out.y;

        if (denom != 0 && (hw2 + dot) > 0) {
            int64_t mx = (int64_t)n_in.x + (int64_t)n_out.x;
            int64_t my = (int64_t)n_in.y + (int64_t)n_out.y;
            int64_t m2 = mx * mx + my * my;
            if (m2 > 0) {
                /* Miter vector = ((2 * hw^2) / (hw^2 + dot)) * ((n_in + n_out)/2) */
                int64_t scale_den = hw2 + dot;
                int64_t vx = (hw2 * mx) / scale_den;
                int64_t vy = (hw2 * my) / scale_den;
                if ((vx * vx + vy * vy) <= (16 * hw2)) {
                    sol_path_line_to(dst, pt.x + (int32_t)vx, pt.y + (int32_t)vy);
                    return;
                }
            }
        }
        sol_path_line_to(dst, pt.x + n_in.x, pt.y + n_in.y);
        sol_path_line_to(dst, pt.x + n_out.x, pt.y + n_out.y);
    }
}

void sol_stroke_path(sol_surface_t *surf, const sol_path_t *path, int32_t thickness,
                     sol_line_cap_t cap, sol_line_join_t join, sol_color_t color,
                     sol_arena_t *scratch) {
    const sol_path_subpath_t *sub;
    size_t mark;
    sol_path_t stroke_poly;
    int32_t half_w;

    if (surf == NULL || surf->pixels == NULL || path == NULL || scratch == NULL || thickness <= 0 ||
        SOL_COLOR_A(color) == 0u) {
        return;
    }

    if (thickness == 1) {
        for (sub = path->subpaths; sub != NULL; sub = sub->next) {
            const sol_path_chunk_t *ch;
            sol_point_t first_pt = SOL_POINT(0, 0);
            sol_point_t prev_pt = SOL_POINT(0, 0);
            bool has_first = false;
            size_t i;

            for (ch = sub->head; ch != NULL; ch = ch->next) {
                for (i = 0u; i < ch->count; ++i) {
                    sol_point_t pt = ch->pts[i];
                    if (!has_first) {
                        first_pt = pt;
                        prev_pt = pt;
                        has_first = true;
                    } else {
                        sol_draw_line_aa(surf, prev_pt.x, prev_pt.y, pt.x, pt.y, color);
                        prev_pt = pt;
                    }
                }
            }
            if (sub->closed && has_first && sub->point_count >= 2u) {
                sol_draw_line_aa(surf, prev_pt.x, prev_pt.y, first_pt.x, first_pt.y, color);
            }
        }
        return;
    }

    half_w = SOL_MAX(1, thickness / 2);
    mark = sol_arena_mark(scratch);

    for (sub = path->subpaths; sub != NULL; sub = sub->next) {
        sol_point_t *pts;
        sol_point_t *norms;
        size_t n = 0u;
        size_t seg_count;
        size_t i;
        const sol_path_chunk_t *ch;

        if (sub->point_count < 2u) {
            continue;
        }

        pts = (sol_point_t *)sol_arena_alloc(scratch, sub->point_count * sizeof(sol_point_t));
        norms = (sol_point_t *)sol_arena_alloc(scratch, sub->point_count * sizeof(sol_point_t));
        if (pts == NULL || norms == NULL) {
            sol_arena_rewind(scratch, mark);
            return;
        }

        for (ch = sub->head; ch != NULL; ch = ch->next) {
            for (i = 0u; i < ch->count; ++i) {
                pts[n++] = ch->pts[i];
            }
        }

        seg_count = sub->closed ? n : (n - 1u);
        for (i = 0u; i < seg_count; ++i) {
            size_t nxt = (i + 1u == n) ? 0u : (i + 1u);
            norms[i] = sol__unit_normal_fp12(pts[i], pts[nxt], (int64_t)half_w * 256);
        }

        sol_path_init(&stroke_poly, scratch);

        if (sub->closed && n >= 3u) {
            /* Outer/Left loop forward */
            for (i = 0u; i < n; ++i) {
                size_t prev_seg = (i == 0u) ? (n - 1u) : (i - 1u);
                sol_point_t n_in = norms[prev_seg];
                sol_point_t n_out = norms[i];
                int64_t turn =
                    (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
                if (i == 0u) {
                    sol_path_move_to(&stroke_poly, pts[0].x + n_in.x, pts[0].y + n_in.y);
                }
                sol__emit_join_side(&stroke_poly, pts[i], n_in, n_out, half_w, join, turn < 0);
            }
            sol_path_close(&stroke_poly);

            /* Inner/Right loop backward */
            for (i = n; i > 0u; --i) {
                size_t idx = i - 1u;
                size_t prev_seg = (idx == 0u) ? (n - 1u) : (idx - 1u);
                sol_point_t n_in = SOL_POINT(-norms[idx].x, -norms[idx].y);
                sol_point_t n_out = SOL_POINT(-norms[prev_seg].x, -norms[prev_seg].y);
                int64_t turn =
                    (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
                if (i == n) {
                    sol_path_move_to(&stroke_poly, pts[idx].x + n_in.x, pts[idx].y + n_in.y);
                }
                sol__emit_join_side(&stroke_poly, pts[idx], n_in, n_out, half_w, join, turn < 0);
            }
            sol_path_close(&stroke_poly);
        } else {
            /* Open polyline: start cap -> forward left -> end cap -> backward right */
            sol_point_t n0 = norms[0];
            sol_point_t n_last = norms[seg_count - 1u];

            if (cap == SOL_CAP_SQUARE) {
                sol_point_t back = SOL_POINT(n0.y, -n0.x);
                sol_path_move_to(&stroke_poly, pts[0].x + n0.x + back.x, pts[0].y + n0.y + back.y);
            } else {
                sol_path_move_to(&stroke_poly, pts[0].x + n0.x, pts[0].y + n0.y);
            }

            for (i = 1u; i + 1u < n; ++i) {
                sol_point_t n_in = norms[i - 1u];
                sol_point_t n_out = norms[i];
                int64_t turn =
                    (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
                sol__emit_join_side(&stroke_poly, pts[i], n_in, n_out, half_w, join, turn < 0);
            }

            if (cap == SOL_CAP_SQUARE) {
                sol_point_t fwd = SOL_POINT(-n_last.y, n_last.x);
                sol_path_line_to(&stroke_poly, pts[n - 1u].x + n_last.x + fwd.x,
                                 pts[n - 1u].y + n_last.y + fwd.y);
                sol_path_line_to(&stroke_poly, pts[n - 1u].x - n_last.x + fwd.x,
                                 pts[n - 1u].y - n_last.y + fwd.y);
            } else if (cap == SOL_CAP_ROUND) {
                sol_path_line_to(&stroke_poly, pts[n - 1u].x + n_last.x, pts[n - 1u].y + n_last.y);
                sol__emit_round_arc(&stroke_poly, pts[n - 1u], n_last,
                                    SOL_POINT(-n_last.x, -n_last.y), half_w, false);
            } else {
                sol_path_line_to(&stroke_poly, pts[n - 1u].x + n_last.x, pts[n - 1u].y + n_last.y);
                sol_path_line_to(&stroke_poly, pts[n - 1u].x - n_last.x, pts[n - 1u].y - n_last.y);
            }

            for (i = n - 1u; i > 1u; --i) {
                size_t idx = i - 1u;
                sol_point_t n_in = SOL_POINT(-norms[idx].x, -norms[idx].y);
                sol_point_t n_out = SOL_POINT(-norms[idx - 1u].x, -norms[idx - 1u].y);
                int64_t turn =
                    (int64_t)n_in.x * (int64_t)n_out.y - (int64_t)n_in.y * (int64_t)n_out.x;
                sol__emit_join_side(&stroke_poly, pts[idx], n_in, n_out, half_w, join, turn < 0);
            }

            if (cap == SOL_CAP_SQUARE) {
                sol_point_t back = SOL_POINT(n0.y, -n0.x);
                sol_path_line_to(&stroke_poly, pts[0].x - n0.x + back.x, pts[0].y - n0.y + back.y);
            } else if (cap == SOL_CAP_ROUND) {
                sol_path_line_to(&stroke_poly, pts[0].x - n0.x, pts[0].y - n0.y);
                sol__emit_round_arc(&stroke_poly, pts[0], SOL_POINT(-n0.x, -n0.y), n0, half_w,
                                    false);
            } else {
                sol_path_line_to(&stroke_poly, pts[0].x - n0.x, pts[0].y - n0.y);
            }
            sol_path_close(&stroke_poly);
        }

        sol_fill_path(surf, &stroke_poly, SOL_FILL_NON_ZERO, color, scratch, true);
        sol_arena_rewind(scratch, mark);
    }
}
