/*
 * SolsticeSFG - Freestanding-Safe Test Harness
 * SPDX-License-Identifier: Zlib OR MIT
 */

#ifndef SOL_TEST_H
#define SOL_TEST_H

#ifdef SOL_FREESTANDING
    #ifndef SOL_NO_STDIO
        #define SOL_NO_STDIO
    #endif
#endif

#ifndef SOL_NO_STDIO
    #include <stdio.h>
#endif

#ifdef SOL_BUILD_AMALGAMATED
    #define SOL_IMPLEMENTATION
#endif
#include "solsticesfg.h"

static int g_failures = 0;

#define TEST_MEMZERO(ptr, size)                                                                    \
    do {                                                                                           \
        uint8_t *sol__mz_p = (uint8_t *)(ptr);                                                     \
        size_t sol__mz_i;                                                                          \
        for (sol__mz_i = 0; sol__mz_i < (size); ++sol__mz_i) {                                     \
            sol__mz_p[sol__mz_i] = 0;                                                              \
        }                                                                                          \
    } while (0)

#ifdef SOL_NO_STDIO
    #define TEST_ASSERT(cond)                                                                      \
        do {                                                                                       \
            if (!(cond)) {                                                                         \
                g_failures++;                                                                      \
            }                                                                                      \
        } while (0)

    #define TEST_REPORT(name)                                                                      \
        do {                                                                                       \
            (void)(name);                                                                          \
            if (g_failures > 0) {                                                                  \
                return 1;                                                                          \
            }                                                                                      \
        } while (0)
#else
    #define TEST_ASSERT(cond)                                                                      \
        do {                                                                                       \
            if (!(cond)) {                                                                         \
                fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
                g_failures++;                                                                      \
            }                                                                                      \
        } while (0)

    #define TEST_REPORT(name)                                                                      \
        do {                                                                                       \
            if (g_failures > 0) {                                                                  \
                fprintf(stderr, "Total failures: %d\n", g_failures);                               \
                return 1;                                                                          \
            }                                                                                      \
            printf("%s passed (SolsticeSFG v%s)\n", (name), sol_version_string());                 \
        } while (0)
#endif

#endif /* SOL_TEST_H */
