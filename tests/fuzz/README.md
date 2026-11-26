# SolsticeSFG Fuzzing Harness (`tests/fuzz/`)

All fuzz targets implement `LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)` and can run either:
1. **With Clang libFuzzer + ASan/UBSan** (continuous fuzzing):
   ```sh
   clang -std=c99 -Wall -Wextra -Werror -pedantic -g -O1 \
         -fsanitize=fuzzer,address,undefined \
         -Iinclude -Isrc tests/fuzz/fuzz_surface.c src/*.c -lm \
         -o build/fuzz_surface
   ./build/fuzz_surface -max_total_time=30
   ```
2. **In standalone smoke-test mode** (CI / `make fuzz-smoke`, no libFuzzer required):
   ```sh
   make fuzz-smoke
   ```

## Targets
- `fuzz_surface.c` — Surface initialization, clipping, and 2D rasterizer bounds/overflow fuzzing.

### Planned Binary Parser Targets (Phases 4 & 5)
- `fuzz_qoi.c` / `fuzz_png.c` — Image decoder fuzzers (Phase 4)
- `fuzz_utf8.c` / `fuzz_psf2.c` / `fuzz_ttf.c` — Unicode & font parser fuzzers (Phase 5)
