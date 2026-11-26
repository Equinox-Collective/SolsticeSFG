#include "sol_test.h"

static void test_surface_init_validation(void) {
    uint32_t buf[16];
    sol_surface_t surf;

    TEST_ASSERT(sol_surface_init(NULL, buf, 4, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, NULL, 4, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 0, 4, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, -1, 4) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 3) == SOL_ERR_INVALID_ARG);

    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 4) == SOL_OK);
    TEST_ASSERT(surf.clip.x == 0 && surf.clip.y == 0);
    TEST_ASSERT(surf.clip.w == 4 && surf.clip.h == 4);
}

static void test_clip_and_clear(void) {
    uint32_t buf[4 * 4];
    sol_surface_t surf;
    sol_rect_t clip = {1, 1, 10, 10};
    sol_color_t black = SOL_RGB(0, 0, 0);
    sol_color_t red = SOL_RGB(255, 0, 0);

    TEST_MEMZERO(buf, sizeof(buf));
    TEST_ASSERT(sol_surface_init(&surf, buf, 4, 4, 4) == SOL_OK);

    sol_clear(&surf, black);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);

    sol_surface_set_clip(&surf, clip);
    TEST_ASSERT(surf.clip.x == 1 && surf.clip.y == 1);
    TEST_ASSERT(surf.clip.w == 3 && surf.clip.h == 3);

    sol_clear(&surf, red);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 1, 0) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 1) == black);
    TEST_ASSERT(sol_get_pixel(&surf, 1, 1) == red);
    TEST_ASSERT(sol_get_pixel(&surf, 3, 3) == red);

    /* Out-of-clip and out-of-bounds put_pixel must be safely ignored */
    sol_put_pixel(&surf, 0, 0, red);
    sol_put_pixel(&surf, -5, -5, red);
    sol_put_pixel(&surf, 99, 99, red);
    TEST_ASSERT(sol_get_pixel(&surf, 0, 0) == black);
}

static void test_rect_intersect(void) {
    sol_rect_t a = {-5, -5, 15, 15};
    sol_rect_t b = {0, 0, 8, 8};
    sol_rect_t c = {20, 20, 5, 5};
    sol_rect_t r1 = sol_rect_intersect(a, b);
    sol_rect_t r2 = sol_rect_intersect(b, c);

    TEST_ASSERT(r1.x == 0 && r1.y == 0 && r1.w == 8 && r1.h == 8);
    TEST_ASSERT(r2.w == 0 && r2.h == 0);
}

static void test_geometry_helpers(void) {
    sol_rect_t a = SOL_RECT(10, 10, 20, 20);
    sol_rect_t b = SOL_RECT(20, 15, 30, 10);
    sol_rect_t u = sol_rect_union(a, b);
    sol_rect_t ins = sol_rect_inset(a, 3, 5);
    sol_rect_t collapsed = sol_rect_inset(a, 15, 0);

    TEST_ASSERT(sol_rect_equals(u, SOL_RECT(10, 10, 40, 20)));
    TEST_ASSERT(sol_rect_equals(ins, SOL_RECT(13, 15, 14, 10)));
    TEST_ASSERT(collapsed.w == 0 && collapsed.h == 0);

    TEST_ASSERT(sol_rect_contains_point(a, 10, 10));
    TEST_ASSERT(sol_rect_contains_point(a, 29, 29));
    TEST_ASSERT(!sol_rect_contains_point(a, 30, 10));
    TEST_ASSERT(!sol_rect_contains_point(a, 9, 10));
}

static void test_subview_and_clip_stack(void) {
    uint32_t buf[8 * 8];
    sol_surface_t parent;
    sol_surface_t sub;
    sol_clip_stack_t stack;
    int32_t i;

    TEST_MEMZERO(buf, sizeof(buf));
    TEST_ASSERT(sol_surface_init(&parent, buf, 8, 8, 8) == SOL_OK);

    /* Subview partially clipped against parent boundary */
    TEST_ASSERT(sol_surface_subview(&sub, &parent, SOL_RECT(4, 4, 10, 10)) == SOL_OK);
    TEST_ASSERT(sub.width == 4 && sub.height == 4 && sub.stride == 8);

    /* Out-of-bounds subview fails cleanly */
    TEST_ASSERT(sol_surface_subview(&sub, &parent, SOL_RECT(20, 20, 4, 4)) ==
                SOL_ERR_OUT_OF_BOUNDS);
    TEST_ASSERT(sol_surface_subview(&sub, &parent, SOL_RECT(4, 4, 4, 4)) == SOL_OK);

    sol_clip_stack_init(&stack);
    TEST_ASSERT(sol_pop_clip(&sub, &stack) == SOL_ERR_OUT_OF_BOUNDS);

    TEST_ASSERT(sol_push_clip(&sub, &stack, SOL_RECT(1, 1, 2, 2)) == SOL_OK);
    TEST_ASSERT(sol_rect_equals(sub.clip, SOL_RECT(1, 1, 2, 2)));

    /* Writing inside subview updates parent buffer at (4+1, 4+1) = (5, 5) */
    sol_clear(&sub, SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&parent, 5, 5) == SOL_COLOR_RED);
    TEST_ASSERT(sol_get_pixel(&parent, 4, 4) == 0u);

    /* Nested push narrows clip further */
    TEST_ASSERT(sol_push_clip(&sub, &stack, SOL_RECT(0, 0, 2, 2)) == SOL_OK);
    TEST_ASSERT(sol_rect_equals(sub.clip, SOL_RECT(1, 1, 1, 1)));

    TEST_ASSERT(sol_pop_clip(&sub, &stack) == SOL_OK);
    TEST_ASSERT(sol_rect_equals(sub.clip, SOL_RECT(1, 1, 2, 2)));
    TEST_ASSERT(sol_pop_clip(&sub, &stack) == SOL_OK);
    TEST_ASSERT(sol_rect_equals(sub.clip, SOL_RECT(0, 0, 4, 4)));

    /* Overflow check on fixed stack */
    for (i = 0; i < SOL_CLIP_STACK_MAX; ++i) {
        TEST_ASSERT(sol_push_clip(&sub, &stack, SOL_RECT(0, 0, 2, 2)) == SOL_OK);
    }
    TEST_ASSERT(sol_push_clip(&sub, &stack, SOL_RECT(0, 0, 2, 2)) == SOL_ERR_OUT_OF_BOUNDS);
}

static void test_dirty_tracker(void) {
    sol_dirty_tracker_t dt;
    int32_t i;

    TEST_ASSERT(sol_dirty_init(&dt, 800, 600) == SOL_OK);
    TEST_ASSERT(sol_dirty_is_empty(&dt));

    /* Adjacent touching rects merge automatically */
    sol_dirty_add(&dt, SOL_RECT(10, 10, 20, 20));
    sol_dirty_add(&dt, SOL_RECT(30, 10, 20, 20));
    TEST_ASSERT(dt.count == 1);
    TEST_ASSERT(sol_rect_equals(dt.rects[0], SOL_RECT(10, 10, 40, 20)));

    /* Disjoint rect stays separate */
    sol_dirty_add(&dt, SOL_RECT(200, 200, 50, 50));
    TEST_ASSERT(dt.count == 2);
    TEST_ASSERT(sol_rect_equals(sol_dirty_bounding_box(&dt), SOL_RECT(10, 10, 240, 240)));

    /* Overflowing SOL_DIRTY_MAX_RECTS stays bounded and lossless */
    sol_dirty_reset(&dt);
    for (i = 0; i < SOL_DIRTY_MAX_RECTS + 10; ++i) {
        sol_dirty_add(&dt, SOL_RECT(i * 10, 0, 5, 5));
    }
    TEST_ASSERT(dt.count <= SOL_DIRTY_MAX_RECTS);
    TEST_ASSERT(!sol_dirty_is_empty(&dt));

    sol_dirty_mark_all(&dt);
    TEST_ASSERT(dt.full_redraw);
    TEST_ASSERT(sol_rect_equals(sol_dirty_bounding_box(&dt), SOL_RECT(0, 0, 800, 600)));
}

int main(void) {
    test_surface_init_validation();
    test_clip_and_clear();
    test_rect_intersect();
    test_geometry_helpers();
    test_subview_and_clip_stack();
    test_dirty_tracker();

    TEST_REPORT("test_core");
    return 0;
}
