#include <stdio.h>
#include <stdlib.h>

#include "solsticesfg.h"

static int save_ppm(const char *path, const sol_surface_t *surf) {
    FILE *fp = fopen(path, "wb");
    int32_t y;
    int32_t x;

    if (fp == NULL) {
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", surf->width, surf->height);
    for (y = 0; y < surf->height; ++y) {
        for (x = 0; x < surf->width; ++x) {
            sol_color_t c = sol_get_pixel(surf, x, y);
            uint8_t rgb[3];
            rgb[0] = SOL_COLOR_R(c);
            rgb[1] = SOL_COLOR_G(c);
            rgb[2] = SOL_COLOR_B(c);
            fwrite(rgb, 1, 3, fp);
        }
    }

    fclose(fp);
    return 0;
}

int main(void) {
    enum { W = 640, H = 400 };
    uint32_t *pixels = (uint32_t *)malloc((size_t)W * (size_t)H * sizeof(uint32_t));
    sol_surface_t surf;

    if (pixels == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    sol_surface_init(&surf, pixels, W, H, W);

    /* Background vertical gradient (Equinox night sky vibe) */
    sol_fill_rect_gradient_v(&surf, SOL_RECT(0, 0, W, H), SOL_RGB(18, 20, 38), SOL_RGB(42, 24, 58));

    /* Rounded DE-style floating panel with border */
    sol_fill_rounded_rect(&surf, SOL_RECT(30, 30, 260, 160), 14, SOL_ARGB(200, 28, 34, 54));
    sol_draw_rounded_rect(&surf, SOL_RECT(30, 30, 260, 160), 14, SOL_RGB(110, 140, 255));

    /* Horizontal gradient bar inside panel */
    sol_fill_rect_gradient_h(&surf, SOL_RECT(50, 55, 220, 24), SOL_RGB(255, 95, 109),
                             SOL_RGB(255, 195, 113));

    /* Squares & thick border */
    sol_fill_rect(&surf, SOL_SQUARE(55, 105, 55), SOL_RGB(70, 200, 160));
    sol_draw_rect_thick(&surf, SOL_SQUARE(130, 105, 55), 4, SOL_COLOR_WHITE);

    /* Overlapping translucent circles (RGB Venn test) */
    sol_fill_circle(&surf, 430, 130, 65, SOL_ARGB(170, 255, 70, 90));
    sol_fill_circle(&surf, 495, 130, 65, SOL_ARGB(170, 70, 230, 140));
    sol_fill_circle(&surf, 462, 185, 65, SOL_ARGB(170, 70, 140, 255));
    sol_draw_circle(&surf, 462, 150, 95, SOL_ARGB(180, 255, 255, 255));

    /* Triangles (filled + outline) */
    sol_fill_triangle(&surf, 110, 355, 200, 230, 280, 355, SOL_ARGB(220, 255, 165, 40));
    sol_draw_triangle(&surf, 110, 355, 200, 230, 280, 355, SOL_COLOR_WHITE);

    /* Ellipse & lines */
    sol_fill_ellipse(&surf, 460, 315, 110, 45, SOL_ARGB(180, 40, 190, 230));
    sol_draw_ellipse(&surf, 460, 315, 110, 45, SOL_COLOR_CYAN);

    sol_draw_line(&surf, 20, 380, 620, 380, SOL_ARGB(120, 255, 255, 255));
    sol_draw_line(&surf, -20, 210, 330, 390, SOL_COLOR_YELLOW);

    if (save_ppm("showcase.ppm", &surf) != 0) {
        fprintf(stderr, "Failed to write showcase.ppm\n");
        free(pixels);
        return 1;
    }

    printf("Saved showcase.ppm (%dx%d)\n", W, H);
    free(pixels);
    return 0;
}
