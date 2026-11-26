# SolsticeSFG — Master Engineering Roadmap

> **Philosophy:** Build one subsystem at a time to **100% completeness**—covering all edge cases, freestanding OS-dev compatibility, strict C99 compliance, fuzz/unit tests, and visual verification—before moving to the next. Never leave half-baked stubs behind.

Status legend:
- `[x]` Implemented, tested (modular + amalgamated), and verified
- `[ ]` Planned / In queue

---

## Phase 0: Repository, Build Infrastructure & Quality Gates
- [x] Dual license (`Zlib OR MIT`) with SPDX headers
- [x] Strict `.clang-format`, `.editorconfig`, `.gitattributes` (LF/CRLF discipline), `.gitignore`
- [x] `Documents/Contributing.md` (DCO `-s` sign-off, `dev`/`master` flow, AI accountability)
- [x] `Documents/PROMPT.md` (strict C99, zero-dependency, amalgamation invariants)
- [x] GitHub Issue Forms (`.yml`) & Pull Request checklist template
- [x] Cross-platform GNU `Makefile` (Linux/Unix `sh` + Windows `cmd.exe` shell probing, `-MMD -MP` auto-deps, `CROSS_COMPILE=`, `debug`/`release`/`sanitize`/`freestanding` modes)
- [x] Pure C99 amalgamation tool (`tools/amalgamate.c`) -> `dist/solsticesfg.h`
- [x] Dual test runner (every `tests/test_*.c` built & executed against BOTH `libsolsticesfg.a` and `dist/solsticesfg.h`)
- [x] GitHub Actions CI for Linux (GCC/Clang), Windows (MinGW), ASan/UBSan, Freestanding (`-ffreestanding -nostdlib`), and `.clang-format`
- [x] Extend `tools/amalgamate.c` to verify all `.c`/`.h` files in `src/` are accounted for (fail build if a file is accidentally omitted)
- [x] Expand CI matrix to include **MSVC (`cl.exe`)**, **macOS (`macos-latest` Clang)**, **FreeBSD (`vmactions/freebsd-vm`)**, and **Valgrind / Cppcheck**
- [x] Add `SOL_NO_STDIO` / freestanding-safe test harness mode and fuzzing harness (`tests/fuzz/`) for binary parsers (UTF-8, PSF2, TTF/OTF, PNG/QOI)

---

## Phase 1: Core Surface, Memory & Pixel Formats (`sol_core`, `sol_alloc`, `sol_format`)
### 1.1 Surface & Geometry Foundation
- [x] `sol_surface_t` (`pixels`, `width`, `height`, `stride`, active `clip`)
- [x] `sol_surface_init` with overflow-safe validation (`width > 0`, `height > 0`, `stride >= width`)
- [x] Scissor/clipping rectangle (`sol_surface_set_clip`, `sol_surface_reset_clip`, `sol_rect_intersect`)
- [x] **Clip stack** (`sol_push_clip`, `sol_pop_clip`, `sol_clip_intersect`) using a fixed-depth stack inside `sol_ctx_t` / surface helper (essential for nested UI containers)
- [x] **Sub-surface views** (`sol_surface_subview`) creating zero-copy child surfaces pointing into a parent's pixel memory with clamped bounds and stride
- [x] **Dirty rectangle tracker** (`sol_dirty_tracker_t`) for tracking modified screen regions and merging overlapping dirty rects (critical for 60+ FPS software DE rendering without redrawing static areas)
- [x] Integer geometry primitives: `sol_point_t`, `sol_size_t`, `sol_rect_contains_point`, `sol_rect_union`, `sol_rect_inset`/`pad`

### 1.2 Freestanding Memory & Allocators
- [x] Overridable memory macros (`SOL_MALLOC`, `SOL_REALLOC`, `SOL_FREE`, `SOL_MEMCPY`, `SOL_MEMSET`, `SOL_MEMMOVE`)
- [x] Pure C99 zero-libc fallbacks (`sol__memcpy`, `sol__memset`, `sol__memmove`, `sol__memcmp`) enabled automatically under `SOL_FREESTANDING`
- [x] **Arena / Bump Allocator** (`sol_arena_t`) working over caller-provided static buffers or OS pages (zero fragmentation, instant reset for per-frame UI/path scratch memory)
- [x] Overflow-safe math helpers (`sol__add_overflow_i32`, `sol__mul_overflow_sz`, `sol__isqrt64`, fixed-point 16.16 / 24.8 math helpers)

### 1.3 Multi-Format Framebuffer Conversion
- [x] Pixel format enum (`sol_pixel_format_t`):
  - `SOL_FMT_ARGB8888` (canonical internal format: `0xAARRGGBB`)
  - `SOL_FMT_XRGB8888` (Linux DRM/KMS, X11, Windows DIB)
  - `SOL_FMT_RGBA8888` (OpenGL / Wayland / macOS)
  - `SOL_FMT_BGRA8888` (little-endian byte order on DRM/Wayland/Win32)
  - `SOL_FMT_ABGR8888`
  - `SOL_FMT_RGB888` / `SOL_FMT_BGR888` (24-bit packed framebuffers)
  - `SOL_FMT_RGB565` / `SOL_FMT_BGR565` (16-bit embedded / legacy VESA BIOS)
  - `SOL_FMT_A8` / `SOL_FMT_GRAY8` (8-bit alpha masks & grayscale glyph buffers)
- [x] Fast row/surface blitting converters (`sol_convert_row`, `sol_present_to_buffer`) to push canonical `ARGB8888` surfaces into any OS hardware framebuffer
- [x] Ordered Bayer 4x4 / 8x8 dithering for clean output on `RGB565` / low-bit-depth displays

---

## Phase 2: Color Science & Compositing (`sol_blend`, `sol_color`)
- [x] ARGB8888 pack/unpack macros (`SOL_ARGB`, `SOL_RGB`, `SOL_COLOR_A/R/G/B`) and base constants
- [x] Exact integer `div255` Porter-Duff `SRC_OVER` blending (`sol_color_blend`, `sol_blend_pixel`)
- [x] Linear color interpolation (`sol_color_lerp`) and alpha modulation (`sol_color_with_alpha`)
- [x] **Full Porter-Duff compositing modes** (`sol_blend_mode_t`):
  - `SOL_BLEND_NONE` (direct copy), `SOL_BLEND_SRC_OVER`, `SOL_BLEND_SRC_IN`, `SOL_BLEND_SRC_OUT`, `SOL_BLEND_SRC_ATOP`
  - `SOL_BLEND_DST_OVER`, `SOL_BLEND_ADD` (glow/lights), `SOL_BLEND_MULTIPLY`, `SOL_BLEND_SCREEN`, `SOL_BLEND_OVERLAY`
- [x] **Premultiplied Alpha pipeline** (`sol_color_premul`, `sol_color_unpremul`, fast premultiplied `SRC_OVER` without dark-halo artifacts on anti-aliased edges and scaled textures)
- [x] **Color space conversions:** RGB <-> HSV, RGB <-> HSL, RGB <-> Hex (`#RRGGBB` / `#AARRGGBB` parser & formatter), Contrast/Luminance helpers (WCAG contrast ratio for readable DE themes)
- [x] **Gamma-correct sRGB <-> Linear LUT** (compile-time 256-entry integer LUT for physically accurate anti-aliasing and gradients without float `powf`)
- [x] Multi-stop gradients (`sol_gradient_stop_t[]`): linear (arbitrary angle), radial, and 4-corner bilinear gradients

---

## Phase 3: Complete 2D Rasterization, Anti-Aliasing & Vector Paths (`sol_draw`, `sol_aa`, `sol_fx`)
### 3.1 Hard-Edged Integer Primitives (Done + Extensions)
- [x] Clipped spans (`sol_draw_hline`, `sol_draw_vline`), Cohen-Sutherland + Bresenham lines (`sol_draw_line`)
- [x] Rectangles & squares (`sol_fill_rect`, `sol_draw_rect`, `sol_draw_rect_thick`, vertical/horizontal gradients)
- [x] Rounded rectangles (`sol_fill_rounded_rect`, `sol_draw_rounded_rect`)
- [x] Zero-overdraw circles & ellipses (`sol_fill_circle`, `sol_draw_circle`, `sol_fill_ellipse`, `sol_draw_ellipse`)
- [x] Zero-overdraw scanline triangles (`sol_draw_triangle`, `sol_fill_triangle`)
- [ ] **Per-corner rounded rectangles** (`sol_rounded_rect_ex` with independent `[tl, tr, br, bl]` radii — e.g. top-rounded window titlebars, pill tabs)
- [ ] **Dashed / dotted lines** (`sol_draw_line_dashed` with pattern bitmask or dash/gap array)
- [ ] **Arcs, rings & pie sectors** (`sol_draw_arc`, `sol_fill_pie`, `sol_fill_ring` — gauges, loading spinners, charts)

### 3.2 Anti-Aliased (AA) & Subpixel Primitives (Essential for Modern DE)
- [ ] **Xiaolin Wu anti-aliased lines** (`sol_draw_line_aa`) with fractional coverage blending
- [ ] **Thick anti-aliased lines** (`sol_draw_line_thick_aa`) with selectable end caps (`BUTT`, `ROUND`, `SQUARE`)
- [ ] **Anti-aliased circles, rings & ellipses** (`sol_fill_circle_aa`, `sol_draw_circle_aa`, `sol_fill_ellipse_aa`) via exact distance/supersampled coverage
- [ ] **Anti-aliased rounded rectangles** (`sol_fill_rounded_rect_aa`, `sol_draw_rounded_rect_aa`, borders with arbitrary thickness) via integer SDF coverage (zero ugly jagged corners!)
- [ ] **Anti-aliased triangles** (`sol_fill_triangle_aa`) via edge-function supersampling (4x4 / 8x8 RGSS pattern or analytical edge coverage)

### 3.3 Vector Paths, Curves & Arbitrary Polygons
- [ ] **Bezier curves:** Quadratic (`sol_draw_quad_bezier`) & Cubic (`sol_draw_cubic_bezier`) with adaptive flatness subdivision (zero floats or fixed-point)
- [ ] **Path builder API** (`sol_path_t` backed by `sol_arena_t`): `move_to`, `line_to`, `quad_to`, `cubic_to`, `arc_to`, `close_path`
- [ ] **Polygon scanline rasterizer** (`sol_fill_polygon`) supporting concave & self-intersecting polygons with **Even-Odd** and **Non-Zero Winding** fill rules + optional 4x/8x vertical supersampled anti-aliasing
- [ ] **Stroke expander:** convert open/closed polylines into thick strokes with **Miter**, **Bevel**, and **Round** line joins

### 3.4 UI Visual Effects (DE / Compositor Grade)
- [ ] **Fast Blur (`sol_blur_rect`):** O(1) per-pixel StackBlur / 3-pass Box Blur approximation inside a clipped surface region (glassmorphism / frosted backdrop blur for DE panels & menus!)
- [ ] **Soft Drop Shadows (`sol_draw_box_shadow`):** fast analytic rounded-box shadow with configurable `offset_x`, `offset_y`, `blur_radius`, `spread`, and `color` (no full blur pass needed!)
- [ ] **Noise / Grain overlay** (subtle deterministic PRNG dither to eliminate gradient banding on dark DE wallpapers)

---

## Phase 4: Surface Blitting, Transforms & Image Codecs (`sol_blit`, `sol_image`)
### 4.1 Blitting & Transformations
- [ ] `sol_blit` / `sol_blit_rect`: fast clipped 1:1 copy and `SRC_OVER` alpha-blended blit between surfaces
- [ ] `sol_blit_tinted`: multiply source texture pixels by a tint/opacity color (essential for monochrome SVG/alpha UI icons!)
- [ ] `sol_blit_9slice`: 9-patch / 9-slice stretching for crisp scalable UI panels, window borders, and buttons without corner distortion
- [ ] `sol_blit_scaled`: nearest-neighbor AND bilinear-filtered scaling (`sol_blit_scaled_bilinear`)
- [ ] `sol_blit_transformed`: 2x3 affine matrix blit (rotation, scaling, shear) with clipping and bilinear filtering
- [ ] `sol_surface_flip_h`, `sol_surface_flip_v`, `sol_surface_rotate_90`

### 4.2 Zero-Dependency Image Decoders & Encoders (Optional compile-time modules)
- [ ] **PPM / PGM / PBM** (`P3`, `P5`, `P6`) loader & writer (built-in for tests and instant debugging)
- [ ] **BMP** (24-bit & 32-bit uncompressed, top-down/bottom-up) loader & writer
- [ ] **TGA** (uncompressed & RLE, 24/32-bit) loader & writer
- [ ] **QOI** (Quite OK Image — ultra-fast lossless RGBA encoder/decoder in ~200 lines of C)
- [ ] **PNG** (pure C99 RFC 1950/1951 DEFLATE/INFLATE + PNG chunk parser + CRC32 + filter reconstruction `None`/`Sub`/`Up`/`Avg`/`Paeth` + optional zlib encoder) — rigorously fuzzed against malformed chunks!

---

## Phase 5: Complete Text & Typography Engine (`sol_utf8`, `sol_font`, `sol_ttf`)
> *Goal: Complete once-and-for-all text stack covering terminal/OS boot fonts, Linux console fonts, and full vector TrueType/OpenType typography with rich styling and full Unicode.*

### 5.1 Unicode, UTF-8 & Wide-Character Core (`sol_utf8.c`)
- [ ] Strict **UTF-8 decoder** (`sol_utf8_next`): 1-to-4 byte decoding into `uint32_t` codepoints, rejecting overlong encodings, surrogate halves (`U+D800..U+DFFF`), and values `> U+10FFFF` (emitting `U+FFFD` replacement character safely)
- [ ] **UTF-8 encoder** (`sol_utf8_encode`) & codepoint counting (`sol_utf8_strlen`)
- [ ] **UTF-16 / Wide-character (`uint16_t` / `wchar_t`) converters** (`sol_utf16_to_utf8`, `sol_utf8_to_utf16`) for seamless Windows Win32 API interoperability
- [ ] **Codepoint classification helpers:** ASCII, whitespace, breaking spaces, newline (`\n`, `\r\n`), tab expansion, zero-width/combining awareness, basic CJK wide-glyph advance handling

### 5.2 Bitmap & Console Fonts (`sol_font_bitmap.c`, `sol_psf.c`)
- [ ] **External/User-provided 8x8 & 8x16 bitmap font interface** (`sol_bitmap_font_t`): clean struct & registration hook so the caller/repo drops in raw glyph arrays without bloating core source files
- [ ] **BDF / HEX bitmap font parser** (optional simple text-based bitmap font loader for crisp pixel UI/CJK fallback fonts)
- [ ] **PC Screen Font (`.psf1` & `.psf2`) full parser** (`sol_psf_load_mem`):
  - Magic validation (`0x36 0x04` for PSF1, `0x72 0xB5 0x4A 0x86` for PSF2)
  - Arbitrary glyph width/height (8x16, 12x24, 16x32, etc.)
  - Full PSF Unicode translation table parsing (`0xFFFF` / `0xFE` multi-codepoint sequences -> glyph index hash/sorted lookup table) — native Linux & EquantOS console font support!

### 5.3 Pure C99 TrueType & OpenType Vector Font Engine (`.ttf` / `.otf`) (`sol_ttf.c`)
- [ ] Safe big-endian binary cursor reader (bounds-checked on every read — zero out-of-bounds crashes on corrupt fonts)
- [ ] Font container & table directory parser (`head`, `hhea`, `maxp`, `hmtx`, `loca`, `glyf`, `cmap`, `kern`, `name`, `OS/2`, `CFF `)
- [ ] `cmap` subtable support: Format 0 (byte encoding), Format 4 (BMP Unicode), Format 6, Format 12 (full Unicode including Emoji/CJK/math symbols `> U+FFFF`!)
- [ ] `glyf` outline loader: simple glyphs + recursive compound/composite glyphs (with transform matrices & component offsets, depth-limited against cyclic loops!)
- [ ] `CFF` (Compact Font Format) Type 2 charstring parser for `.otf` cubic Bezier outlines
- [ ] **Rasterizer:**
  - Fractional scale factor (`pixel_height` -> `unitsPerEm` scaling) for **any arbitrary font size** (6px to 500px+)
  - Analytical scanline coverage / signed-edge anti-aliased rasterizer (sharp at small sizes, smooth at large sizes, optional 3x1 LCD subpixel RGB/BGR rendering!)
  - Horizontal metrics (`advance_width`, `left_side_bearing`) + `kern` pair kerning lookup
- [ ] **Glyph Atlas Cache (`sol_font_atlas_t`):**
  - Dynamic or arena-backed LRU glyph cache (`codepoint + pixel_size + style_flags -> atlas_rect`)
  - Rasterizes each glyph once, then blits via fast alpha-tinted `sol_blit_a8`!
- [ ] **Font Fallback Chain:** attach fallback fonts (e.g., UI Sans `.ttf` -> Symbols `.ttf` -> CJK `.ttf` -> `.psf2` -> 8x16 missing-box fallback) so mixed English/Cyrillic/Japanese/Symbols render seamlessly in one string!

### 5.4 Rich Text Rendering, Effects & Layout (`sol_text.c`)
- [ ] **Arbitrary scaling** (integer pixel-crisp scaling for bitmap/PSF fonts + true vector scaling for TTF/OTF)
- [ ] **Text coloring & gradients:**
  - Solid color, per-glyph or span-wide **horizontal/vertical/diagonal color gradients** (rainbow/sunset/theme gradients across text!)
  - **Drop shadow** & **outline/stroke** text effects (readable HUD/OS overlay labels over any wallpaper)
  - Synthetic **Bold** (dilation/double-strike), **Italic** (shear transform), **Underline**, **Strikethrough**, **Background highlight** (for text selection!)
- [ ] **Layout & Measurement:**
  - `sol_text_measure` (exact bounding box width, height, ascent, descent, line count)
  - Alignment (`LEFT`, `CENTER`, `RIGHT`, `TOP`, `MIDDLE`, `BOTTOM`)
  - Word wrapping (`sol_text_wrap` within max width) & ellipsis truncation (`"Long File Name..."`)
  - Caret / hit-testing API: `sol_text_hit_test(x, y) -> byte_offset` and `sol_text_caret_x(byte_offset) -> x` (required for text editors, terminals, and UI input boxes!)
- [ ] **Comprehensive Text Test Suite (`tests/test_text.c` + `examples/text_showcase.c`):**
  - Test UTF-8 edge cases (ASCII, Cyrillic, Japanese/CJK, math/box-drawing/arrow Unicode symbols, malformed byte sequences)
  - Render visual PPM showcase proving: multiple sizes, gradients across text, rotated/scaled text, shadows/outlines, word wrap, `.psf2` + `.ttf` + `.otf` + bitmap fonts side by side!

---

## Phase 6: Layout Engine & UI Toolkit (`sol_layout`, `sol_ui`)
> *Goal: Power full Desktop Environments (panels, taskbars, launchers, file managers, settings apps) on both Windows and EquantOS.*

### 6.1 Layout Solver (`sol_layout.c`)
- [ ] Hybrid Immediate/Retained layout pass (Arena-backed per frame, zero heap churn)
- [ ] Flex/Box containers: horizontal & vertical flow, `gap`, `padding`, `margin`
- [ ] Sizing rules: fixed `px`, percentage `%`, `fit_content`, `grow` (flex weight), `min`/`max` constraints
- [ ] Alignment (`START`, `CENTER`, `END`, `STRETCH`) and overflow clipping + scroll offset calculation

### 6.2 Input & Event Abstraction (`sol_input.h`)
- [ ] Platform-independent event structs: mouse move/button/wheel, key down/up/repeat (clean keycode enum + modifiers `SHIFT`/`CTRL`/`ALT`/`SUPER`), UTF-8 text input events, window resize/focus/DPI change
- [ ] Hit-testing, hover/active/hot-ID state, drag tracking, keyboard focus & Tab navigation, double/triple-click timing

### 6.3 Core UI Widgets & DE Primitives (`sol_ui.c`)
- [ ] **Theme struct (`sol_theme_t`):** colors, corner radii, borders, shadows, font handles, spacing scale (Dark/Light presets + live hot-swap)
- [ ] **Primitives:** Label, Separator, Icon, Image, Panel/Card (with optional blur & shadow), Tooltip
- [ ] **Interactive Controls:** Button (primary/ghost/icon), Checkbox, Toggle Switch, Radio Group, Slider, Progress Bar
- [ ] **Text Input / Line Edit:** single-line & multi-line text box with cursor navigation, selection, scroll, clipboard callback hooks
- [ ] **Containers & Chrome:** ScrollView (mouse wheel + draggable scrollbar), Tabs, Context Menu / Popup Dropdown, Modal Dialog
- [ ] **DE-Specific Components:** Taskbar/Dock container, Workspace switcher pill, System Tray icon slot, Window Titlebar / Resize handles / Snap preview overlay

---

## Phase 7: Cross-OS Platform Backends (`backends/`)
> *Goal: Core library stays 100% OS-agnostic. Backends live strictly in `backends/` and compile cleanly on every target OS.*

### 7.1 Unified Backend Contract (`backends/sol_backend.h`)
- [ ] Optional thin window/display wrapper (`sol_app_t` / `sol_window_t`) providing: create surface/window, poll events into `sol_input`, present `sol_surface_t` (full or dirty-rect partial update), set cursor shape, clipboard get/set, high-res monotonic timer (`sol_time_ns`)

### 7.2 Freestanding & Hobby OS (`EquantOS`)
- [ ] **`sol_backend_fb_raw.c`:** Zero-dependency backend targeting raw linear framebuffer (Limine / Multiboot2 / UEFI GOP / VESA), dirty-rect copy + pixel format conversion (`XRGB8888`/`BGRA8888`/`RGB565`), ringbuffer input hooks
- [ ] **`sol_backend_equantos.c`:** Custom display-server IPC / KMS hooks for EquantOS Christmas DE

### 7.3 Linux (`backends/linux/`)
- [ ] **`sol_backend_drm.c`:** Direct Linux **KMS/DRM** (`drmModeSetCrtc`, dumb buffers + double-buffered page flipping `drmModePageFlip`) + **`evdev`** keyboard/mouse input (zero X11, zero Wayland!)
- [ ] **`sol_backend_fbdev.c`:** Legacy `/dev/fb0` + `/dev/input/event*`
- [ ] **`sol_backend_x11.c`:** Xlib + MIT-SHM (`XShmPutImage`) for fast windowed testing on existing Linux desktops
- [ ] **`sol_backend_wayland.c`:** Pure Wayland client (`wl_shm`, `xdg-shell` via `wayland-client`)

### 7.4 Windows (`backends/windows/`)
- [ ] **`sol_backend_win32.c`:** Native Win32 `HWND` + `CreateDIBSection` (zero-copy `uint32_t *` shared with GDI!), DPI awareness (`SetProcessDpiAwarenessContext`), raw/WM mouse + `WM_CHAR` UTF-16->UTF-8 input, `QueryPerformanceCounter`
- [ ] **Windows DE / Shell Extensions:** Support for `WS_EX_LAYERED` per-pixel alpha windows (`UpdateLayeredWindow`), borderless always-on-top taskbars (`SHAppBarMessage`), global hotkeys, and window enumeration hooks (`SetWinEventHook`)

### 7.5 macOS (`backends/macos/`)
- [ ] **`sol_backend_cocoa.m`:** Minimal Objective-C/C bridge (`NSApplication`, `NSWindow`, `NSView` / `CALayer` or `CGContextDrawImage` from `CGDataProvider`), Retina (`backingScaleFactor`) DPI support, Cocoa mouse/key/UTF-8 text events

### 7.6 BSD — FreeBSD / OpenBSD / NetBSD (`backends/bsd/`)
- [ ] Shared POSIX/DRM/X11 compatibility layer (`sol_backend_drm.c` + `sol_backend_x11.c` + `wscons` / `syscons` framebuffer adaptation with BSD endianness & `<sys/types.h>` guards)
- [ ] Dedicated CI verification on FreeBSD

---

## Phase 8: Performance, SIMD & Benchmarking (`sol_simd`, `benchmarks/`)
- [ ] Pure C99 scalar reference path remains canonical ground truth (every SIMD path must match bit-for-bit or within ±1 LSB)
- [ ] Compile-time + runtime CPUID feature detection (`SSE2`, `AVX2` on x86_64; `NEON` on ARM64)
- [ ] Vectorized hot loops: solid span fill, `SRC_OVER` alpha blending (4/8 pixels at once), `sol_blit`, `StackBlur`, `ARGB <-> BGRA/XRGB` swizzle
- [ ] Automated benchmark suite (`make bench`) measuring MPixels/sec for fills, AA rounded rects, StackBlur, TTF glyph blitting, and full 1080p/4K DE frame compositing

---

## Phase 9: Release Engineering & Documentation (`v1.0.0`)
- [ ] Complete `README.md` with visual showcase, architecture diagram, quickstart (single-header & modular), and backend table
- [ ] Full Doxygen-style / Markdown API reference in `Documents/API.md`
- [ ] Automated GitHub Releases generating `dist/solsticesfg.h`, prebuilt static libraries, and checksums
- [ ] **Milestone Validation Projects (Dogfooding):**
  1. Interactive Windows DE / Taskbar prototype powered 100% by `SolsticeSFG`
  2. EquantOS Christmas Special native Display Server + DE (zero X11 / zero IceWM!)
