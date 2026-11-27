#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dirent.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const k_public_headers[] = {"solsticesfg.h"};

static const char *const k_internal_headers[] = {"sol_internal.h"};

static const char *const k_source_files[] = {"sol_core.c",  "sol_blend.c",  "sol_draw.c",
                                             "sol_alloc.c", "sol_format.c", "sol_aa.c",
                                             "sol_fx.c",    "sol_path.c",   "sol_blit.c"};

static int has_c_or_h_ext(const char *name) {
    size_t len = strlen(name);
    if (len < 3) {
        return 0;
    }
    return (strcmp(name + len - 2, ".c") == 0) || (strcmp(name + len - 2, ".h") == 0);
}

static int in_list(const char *name, const char *const *list, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(name, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int verify_dir_files(const char *dir, const char *const *list_a, size_t count_a,
                            const char *const *list_b, size_t count_b) {
    int ok = 0;

#ifdef _WIN32
    char pattern[1024];
    WIN32_FIND_DATAA ffd;
    HANDLE hfind;

    if (snprintf(pattern, sizeof(pattern), "%s\\*", dir) >= (int)sizeof(pattern)) {
        fprintf(stderr, "amalgamate: directory path too long: %s\n", dir);
        return -1;
    }
    hfind = FindFirstFileA(pattern, &ffd);
    if (hfind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "amalgamate: failed to scan directory '%s'\n", dir);
        return -1;
    }
    do {
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (has_c_or_h_ext(ffd.cFileName)) {
            if (!in_list(ffd.cFileName, list_a, count_a) &&
                !in_list(ffd.cFileName, list_b, count_b)) {
                fprintf(stderr,
                        "amalgamate: error: '%s/%s' is not registered in tools/amalgamate.c\n", dir,
                        ffd.cFileName);
                ok = -1;
            }
        }
    } while (FindNextFileA(hfind, &ffd) != 0);
    FindClose(hfind);
#else
    DIR *dp = opendir(dir);
    const struct dirent *ent;

    if (dp == NULL) {
        fprintf(stderr, "amalgamate: failed to open directory '%s'\n", dir);
        return -1;
    }
    while ((ent = readdir(dp)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (has_c_or_h_ext(name)) {
            if (!in_list(name, list_a, count_a) && !in_list(name, list_b, count_b)) {
                fprintf(stderr,
                        "amalgamate: error: '%s/%s' is not registered in tools/amalgamate.c\n", dir,
                        name);
                ok = -1;
            }
        }
    }
    closedir(dp);
#endif

    return ok;
}

static int is_local_sol_include(const char *line) {
    const char *p = line;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (strncmp(p, "#include", 8) != 0) {
        return 0;
    }
    p += 8;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return strncmp(p, "\"sol", 4) == 0 || strncmp(p, "\"../include/sol", 15) == 0;
}

static int append_file(FILE *out, const char *dir, const char *filename, int strip_local_inc) {
    char path[1024];
    char line[4096];
    FILE *in;

    if (snprintf(path, sizeof(path), "%s/%s", dir, filename) >= (int)sizeof(path)) {
        fprintf(stderr, "amalgamate: path too long: %s/%s\n", dir, filename);
        return -1;
    }

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "amalgamate: failed to open '%s'\n", path);
        return -1;
    }

    fprintf(out, "\n/* === BEGIN %s === */\n", filename);
    while (fgets(line, (int)sizeof(line), in) != NULL) {
        if (strip_local_inc && is_local_sol_include(line)) {
            continue;
        }
        /* Strip trailing CR if any so output always uses clean LF */
        size_t len = strlen(line);
        if (len >= 2 && line[len - 2] == '\r' && line[len - 1] == '\n') {
            line[len - 2] = '\n';
            line[len - 1] = '\0';
        }
        fputs(line, out);
    }
    fprintf(out, "/* === END %s === */\n", filename);

    fclose(in);
    return 0;
}

int main(int argc, char **argv) {
    const char *inc_dir;
    const char *src_dir;
    const char *out_path;
    FILE *out;
    size_t i;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <include_dir> <src_dir> <output_header>\n",
                argc > 0 ? argv[0] : "amalgamate");
        return 1;
    }

    inc_dir = argv[1];
    src_dir = argv[2];
    out_path = argv[3];

    if (verify_dir_files(inc_dir, k_public_headers,
                         sizeof(k_public_headers) / sizeof(k_public_headers[0]), NULL, 0) != 0) {
        return 1;
    }

    if (verify_dir_files(src_dir, k_internal_headers,
                         sizeof(k_internal_headers) / sizeof(k_internal_headers[0]), k_source_files,
                         sizeof(k_source_files) / sizeof(k_source_files[0])) != 0) {
        return 1;
    }

    out = fopen(out_path, "wb");
    if (out == NULL) {
        fprintf(stderr, "amalgamate: failed to open output '%s'\n", out_path);
        return 1;
    }

    fprintf(out, "/*\n"
                 " * ==========================================================================\n"
                 " *  SolsticeSFG — Amalgamated Single-Header Distribution\n"
                 " *  DO NOT EDIT THIS FILE DIRECTLY. Generated by tools/amalgamate.c\n"
                 " *\n"
                 " *  Copyright (c) 2026 Equinox-Collective (ewasion137)\n"
                 " *  SPDX-License-Identifier: Zlib OR MIT\n"
                 " *\n"
                 " *  USAGE:\n"
                 " *    In ONE .c file, define SOL_IMPLEMENTATION before including:\n"
                 " *      #define SOL_IMPLEMENTATION\n"
                 " *      #include \"solsticesfg.h\"\n"
                 " * ==========================================================================\n"
                 " */\n"
                 "#define SOL_AMALGAMATED 1\n");

    for (i = 0; i < sizeof(k_public_headers) / sizeof(k_public_headers[0]); ++i) {
        if (append_file(out, inc_dir, k_public_headers[i], 0) != 0) {
            fclose(out);
            return 1;
        }
    }

    fprintf(out, "\n#ifdef SOL_IMPLEMENTATION\n"
                 "#ifndef SOLSTICESFG_IMPLEMENTATION_INCLUDED\n"
                 "#define SOLSTICESFG_IMPLEMENTATION_INCLUDED\n");

    for (i = 0; i < sizeof(k_internal_headers) / sizeof(k_internal_headers[0]); ++i) {
        if (append_file(out, src_dir, k_internal_headers[i], 1) != 0) {
            fclose(out);
            return 1;
        }
    }

    for (i = 0; i < sizeof(k_source_files) / sizeof(k_source_files[0]); ++i) {
        if (append_file(out, src_dir, k_source_files[i], 1) != 0) {
            fclose(out);
            return 1;
        }
    }

    fprintf(out, "\n#endif /* SOLSTICESFG_IMPLEMENTATION_INCLUDED */\n"
                 "#endif /* SOL_IMPLEMENTATION */\n");

    fclose(out);
    return 0;
}
