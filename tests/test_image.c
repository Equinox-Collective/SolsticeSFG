#include "sol_test.h"

static void fill_pattern(sol_surface_t *surf) {
    int32_t y;
    int32_t x;
    for (y = 0; y < surf->height; ++y) {
        for (x = 0; x < surf->width; ++x) {
            uint8_t a = ((x + y) % 3 == 0) ? 160u : 255u;
            sol_put_pixel(
                surf, x, y,
                SOL_ARGB(a, (uint8_t)(x * 31), (uint8_t)(y * 47), (uint8_t)((x ^ y) * 19)));
        }
    }
}

static void test_ppm_bmp_tga_qoi_roundtrip(void) {
    uint8_t arena_mem[16384];
    sol_arena_t arena;
    uint32_t src_px[8 * 6];
    sol_surface_t src;
    uint8_t enc[4096];
    size_t written = 0u;
    sol_image_t img;

    TEST_ASSERT(sol_arena_init(&arena, arena_mem, sizeof(arena_mem)) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&src, src_px, 8, 6, 8) == SOL_OK);
    fill_pattern(&src);

    /* PPM P6 roundtrip (RGB channels match) */
    TEST_ASSERT(sol_image_encode_ppm(&src, enc, sizeof(enc), &written) == SOL_OK);
    TEST_ASSERT(sol_image_detect(enc, written) == SOL_IMG_PPM);
    TEST_ASSERT(sol_image_load_mem(&img, enc, written, &arena) == SOL_OK);
    TEST_ASSERT(img.surface.width == 8 && img.surface.height == 6);
    TEST_ASSERT((sol_get_pixel(&img.surface, 3, 2) & 0x00FFFFFFu) ==
                (sol_get_pixel(&src, 3, 2) & 0x00FFFFFFu));

    /* BMP 32-bit exact ARGB roundtrip */
    sol_arena_reset(&arena);
    TEST_ASSERT(sol_image_encode_bmp(&src, enc, sizeof(enc), &written) == SOL_OK);
    TEST_ASSERT(sol_image_detect(enc, written) == SOL_IMG_BMP);
    TEST_ASSERT(sol_image_load_mem(&img, enc, written, &arena) == SOL_OK);
    TEST_ASSERT(sol__memcmp(img.pixels, src_px, sizeof(src_px)) == 0);

    /* TGA 32-bit exact ARGB roundtrip */
    sol_arena_reset(&arena);
    TEST_ASSERT(sol_image_encode_tga(&src, enc, sizeof(enc), &written) == SOL_OK);
    TEST_ASSERT(sol_image_load_tga_mem(&img, enc, written, &arena) == SOL_OK);
    TEST_ASSERT(sol__memcmp(img.pixels, src_px, sizeof(src_px)) == 0);

    /* QOI exact ARGB roundtrip */
    sol_arena_reset(&arena);
    TEST_ASSERT(sol_image_encode_qoi(&src, enc, sizeof(enc), &written) == SOL_OK);
    TEST_ASSERT(sol_image_detect(enc, written) == SOL_IMG_QOI);
    TEST_ASSERT(sol_image_load_mem(&img, enc, written, &arena) == SOL_OK);
    TEST_ASSERT(sol__memcmp(img.pixels, src_px, sizeof(src_px)) == 0);
}

static void test_png_and_pnm_variants(void) {
    static const uint8_t k_p3_ascii[] =
        "P3\n# SolsticeSFG test comment\n2 1\n255\n255 0 0  0 255 128\n";
    static const uint8_t k_p5_pgm[] = {'P', '5', '\n', '2',  ' ',   '1',  '\n',
                                       '2', '5', '5',  '\n', 0x00u, 0xFFu};
    uint8_t arena_mem[16384];
    sol_arena_t arena;
    uint32_t src_px[8 * 6];
    sol_surface_t src;
    uint8_t enc[4096];
    size_t queried = 0u;
    size_t written = 0u;
    sol_image_t img;

    TEST_ASSERT(sol_arena_init(&arena, arena_mem, sizeof(arena_mem)) == SOL_OK);
    TEST_ASSERT(sol_surface_init(&src, src_px, 8, 6, 8) == SOL_OK);
    fill_pattern(&src);

    /* Query size + Stored-DEFLATE PNG roundtrip (arena mode) */
    TEST_ASSERT(sol_image_encode_png(&src, NULL, 0u, &queried) == SOL_OK);
    TEST_ASSERT(queried > 0u && queried <= sizeof(enc));
    TEST_ASSERT(sol_image_encode_png(&src, enc, sizeof(enc), &written) == SOL_OK);
    TEST_ASSERT(written == queried);
    TEST_ASSERT(sol_image_detect(enc, written) == SOL_IMG_PNG);
    TEST_ASSERT(sol_image_load_mem(&img, enc, written, &arena) == SOL_OK);
    TEST_ASSERT(img.surface.width == 8 && img.surface.height == 6);
    TEST_ASSERT(sol__memcmp(img.pixels, src_px, sizeof(src_px)) == 0);

    /* Corrupt CRC rejection cleanly rewinds arena */
    enc[15] ^= 0x01u;
    sol_arena_reset(&arena);
    TEST_ASSERT(sol_image_load_png_mem(&img, enc, written, &arena) == SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_arena_remaining(&arena) == sizeof(arena_mem));

    /* ASCII P3 with # comment and binary P5 PGM */
    TEST_ASSERT(sol_image_load_mem(&img, k_p3_ascii, sizeof(k_p3_ascii) - 1u, &arena) == SOL_OK);
    TEST_ASSERT(sol_get_pixel(&img.surface, 0, 0) == SOL_RGB(255, 0, 0));
    TEST_ASSERT(sol_get_pixel(&img.surface, 1, 0) == SOL_RGB(0, 255, 128));

    TEST_ASSERT(sol_image_load_mem(&img, k_p5_pgm, sizeof(k_p5_pgm), &arena) == SOL_OK);
    TEST_ASSERT(sol_get_pixel(&img.surface, 0, 0) == SOL_COLOR_BLACK);
    TEST_ASSERT(sol_get_pixel(&img.surface, 1, 0) == SOL_COLOR_WHITE);

#ifndef SOL_FREESTANDING
    /* Hosted heap allocation + destroy */
    TEST_ASSERT(sol_image_load_mem(&img, k_p3_ascii, sizeof(k_p3_ascii) - 1u, NULL) == SOL_OK);
    TEST_ASSERT(img.owns_memory);
    sol_image_destroy(&img);
    TEST_ASSERT(img.pixels == NULL);
#endif
}

static void test_malformed_inputs(void) {
    uint8_t arena_mem[4096];
    sol_arena_t arena;
    sol_image_t img;
    static const uint8_t k_bad_pnm[] = "P6\n999999 999999\n255\n";
    static const uint8_t k_bad_tga[18] = {0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 0, 32, 0};

    TEST_ASSERT(sol_arena_init(&arena, arena_mem, sizeof(arena_mem)) == SOL_OK);
    TEST_ASSERT(sol_image_load_ppm_mem(&img, k_bad_pnm, sizeof(k_bad_pnm) - 1u, &arena) ==
                SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_image_load_tga_mem(&img, k_bad_tga, sizeof(k_bad_tga), &arena) ==
                SOL_ERR_INVALID_ARG);
    TEST_ASSERT(sol_arena_remaining(&arena) == sizeof(arena_mem));
}

int main(void) {
    test_ppm_bmp_tga_qoi_roundtrip();
    test_png_and_pnm_variants();
    test_malformed_inputs();

    TEST_REPORT("test_image");
    return 0;
}
