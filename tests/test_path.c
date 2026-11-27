#include "sol_test.h"

static void test_bezier_curves(void) {
    uint32_t buf[32 * 32];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(&surf, buf, 32, 32, 32) == SOL_OK);
    sol_clear(&surf, SOL_COLOR_BLACK);

    sol_draw_quad_bezier(&surf, 2, 28, 16, 2, 29, 28, SOL_COLOR_WHITE, true);
    TEST_ASSERT(SOL_COLOR_R(sol_get_pixel(&surf, 2, 28)) > 200u);
    TEST_ASSERT(SOL_COLOR_R(sol_get_pixel(&surf, 29, 28)) > 200u);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == SOL_COLOR_BLACK);

    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_draw_cubic_bezier(&surf, 2, 16, 10, 2, 22, 30, 29, 16, SOL_COLOR_CYAN, false);
    TEST_ASSERT(sol_get_pixel(&surf, 2, 16) == SOL_COLOR_CYAN);
    TEST_ASSERT(sol_get_pixel(&surf, 29, 16) == SOL_COLOR_CYAN);
}

static void test_polygon_winding_rules(void) {
    uint8_t arena_mem[16384];
    sol_arena_t arena;
    uint32_t buf[40 * 40];
    sol_surface_t surf;
    /* 5-pointed star whose center winds twice (even-odd = hollow, non-zero = filled) */
    sol_point_t star[5];

    star[0] = SOL_POINT(20, 2);
    star[1] = SOL_POINT(31, 36);
    star[2] = SOL_POINT(3, 14);
    star[3] = SOL_POINT(37, 14);
    star[4] = SOL_POINT(9, 36);

    TEST_ASSERT(sol_arena_init(&arena, arena_mem, sizeof(arena_mem)) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&surf, buf, 40, 40, 40) == SOL_OK);

    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_fill_polygon(&surf, star, 5u, SOL_FILL_EVEN_ODD, SOL_COLOR_YELLOW, &arena, true);
    TEST_ASSERT(sol_get_pixel(&surf, 20, 20) == SOL_COLOR_BLACK); /* Star center hollow */
    TEST_ASSERT(sol_get_pixel(&surf, 20, 9) == SOL_COLOR_YELLOW); /* Top tip filled */

    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_fill_polygon(&surf, star, 5u, SOL_FILL_NON_ZERO, SOL_COLOR_YELLOW, &arena, true);
    TEST_ASSERT(sol_get_pixel(&surf, 20, 20) == SOL_COLOR_YELLOW); /* Star center filled */
    TEST_ASSERT(sol_arena_remaining(&arena) == sizeof(arena_mem)); /* Scratch cleanly rewound */
}

static void test_path_builder_and_stroke_joins(void) {
    uint8_t arena_mem[32768];
    sol_arena_t arena;
    uint32_t buf[48 * 48];
    sol_surface_t surf;
    sol_path_t path;
    sol_color_t semi = SOL_ARGB(128, 0, 255, 128);
    sol_color_t expected;

    TEST_ASSERT(sol_arena_init(&arena, arena_mem, sizeof(arena_mem)) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&surf, buf, 48, 48, 48) == SOL_OK);
    TEST_ASSERT(sol_path_init(&path, &arena) == SOL_OK);

    TEST_ASSERT(sol_path_move_to(&path, 8, 8) == SOL_OK);
    TEST_ASSERT(sol_path_line_to(&path, 40, 8) == SOL_OK);
    TEST_ASSERT(sol_path_line_to(&path, 40, 40) == SOL_OK);
    TEST_ASSERT(sol_path_line_to(&path, 8, 40) == SOL_OK);
    TEST_ASSERT(sol_path_close(&path) == SOL_OK);

    /* Closed translucent stroke must not double-blend corners */
    sol_clear(&surf, SOL_COLOR_BLACK);
    expected = sol_color_blend(SOL_COLOR_BLACK, semi);
    sol_stroke_path(&surf, &path, 6, SOL_CAP_BUTT, SOL_JOIN_MITER, semi, &arena);
    TEST_ASSERT(sol_get_pixel(&surf, 24, 8) == expected);
    TEST_ASSERT(sol_get_pixel(&surf, 40, 8) == expected);         /* Corner matches edge alpha! */
    TEST_ASSERT(sol_get_pixel(&surf, 24, 24) == SOL_COLOR_BLACK); /* Center stays hollow */

    /* Curved path with quad + cubic + arc */
    sol_arena_reset(&arena);
    TEST_ASSERT(sol_path_init(&path, &arena) == SOL_OK);
    TEST_ASSERT(sol_path_move_to(&path, 6, 24) == SOL_OK);
    TEST_ASSERT(sol_path_quad_to(&path, 24, 2, 42, 24) == SOL_OK);
    TEST_ASSERT(sol_path_arc_to(&path, 24, 24, 14, 14, 0, 180) == SOL_OK);
    TEST_ASSERT(sol_path_close(&path) == SOL_OK);

    sol_clear(&surf, SOL_COLOR_BLACK);
    sol_fill_path(&surf, &path, SOL_FILL_EVEN_ODD, SOL_COLOR_RED, &arena, true);
    TEST_ASSERT(sol_get_pixel(&surf, 24, 14) == SOL_COLOR_RED);
}

int main(void) {
    test_bezier_curves();
    test_polygon_winding_rules();
    test_path_builder_and_stroke_joins();

    TEST_REPORT("test_path");
    return 0;
}
