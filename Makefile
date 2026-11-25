# ==============================================================================
#  SolsticeSFG — Master Build System
#  Equinox-Collective (Zlib OR MIT)
# ==============================================================================

PROJECT      := solsticesfg
HEADER_NAME  := solsticesfg.h
VERSION      := 0.1.0

# ------------------------------------------------------------------------------
# 1. OS, Architecture & Shell Detection
# ------------------------------------------------------------------------------
IS_WINDOWS := 0
IS_MACOS   := 0
IS_LINUX   := 0
IS_CMD     := 0

ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
    EXE        := .exe
    SO_EXT     := .dll
    LIB_PREFIX :=
    # Detect if Make is running under cmd.exe instead of sh.exe (Git Bash/MSYS2)
    ifneq (,$(findstring cmd,$(SHELL)))
        IS_CMD := 1
    endif
else
    UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
    EXE     :=
    LIB_PREFIX := lib
    ifeq ($(UNAME_S),Darwin)
        IS_MACOS := 1
        SO_EXT   := .dylib
    else
        IS_LINUX := 1
        SO_EXT   := .so
    endif
endif

# Portable filesystem & logging helpers (works in sh AND Windows cmd.exe)
ifeq ($(IS_CMD),1)
    MKDIR_P = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RM_RF   = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
    RM_F    = if exist "$(subst /,\,$(1))" del /f /q "$(subst /,\,$(1))"
    CP_F    = copy /y "$(subst /,\,$(1))" "$(subst /,\,$(2))" >nul
    LOG     = @echo   $(1)  $(2)
else
    MKDIR_P = mkdir -p $(1)
    RM_RF   = rm -rf $(1)
    RM_F    = rm -f $(1)
    CP_F    = cp -f $(1) $(2)
    LOG     = @printf "  %-10s %s\n" "$(1)" "$(2)"
endif

# ------------------------------------------------------------------------------
# 2. Toolchain & User-Overridable Variables
# ------------------------------------------------------------------------------
CROSS_COMPILE ?=
CC            := $(CROSS_COMPILE)gcc
AR            := $(CROSS_COMPILE)ar
CLANG_FORMAT  ?= clang-format
VALGRIND      ?= valgrind
CPPCHECK      ?= cppcheck

# Build mode: debug | release | sanitize | freestanding
MODE          ?= debug
# Verbose output: V=0 (quiet) | V=1 (full commands)
V             ?= 0
# Extra pedantic warnings: STRICT=1
STRICT        ?= 1
# Extra user flags
EXTRA_CFLAGS  ?=
EXTRA_LDFLAGS ?=

# Install paths
PREFIX        ?= /usr/local
DESTDIR       ?=

ifeq ($(V),1)
    Q :=
else
    Q := @
endif

# ------------------------------------------------------------------------------
# 3. Directory Layout & File Discovery
# ------------------------------------------------------------------------------
INC_DIR       := include
SRC_DIR       := src
BACKEND_DIR   := backends
TOOLS_DIR     := tools
DIST_DIR      := dist
TEST_DIR      := tests
EX_DIR        := examples
BENCH_DIR     := benchmarks
BUILD_ROOT    := build
BUILD_DIR     := $(BUILD_ROOT)/$(MODE)

OBJ_DIR       := $(BUILD_DIR)/obj
BIN_DIR       := $(BUILD_DIR)/bin
LIB_DIR       := $(BUILD_DIR)/lib
TEST_BIN_DIR  := $(BUILD_DIR)/tests
EX_BIN_DIR    := $(BUILD_DIR)/examples
BENCH_BIN_DIR := $(BUILD_DIR)/bench

# Source discovery
INC_FILES     := $(wildcard $(INC_DIR)/*.h)
SRC_FILES     := $(wildcard $(SRC_DIR)/*.c)
SRC_HEADERS   := $(wildcard $(SRC_DIR)/*.h)
BACKEND_SRCS  := $(wildcard $(BACKEND_DIR)/*.c)
TEST_SRCS     := $(wildcard $(TEST_DIR)/test_*.c)
EX_SRCS       := $(wildcard $(EX_DIR)/*.c)
BENCH_SRCS    := $(wildcard $(BENCH_DIR)/*.c)

AMALG_SRC     := $(TOOLS_DIR)/amalgamate.c
AMALG_BIN     := $(BUILD_ROOT)/tools/amalgamate$(EXE)
DIST_HEADER   := $(DIST_DIR)/$(HEADER_NAME)

# Object & Dependency mapping
LIB_OBJS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(SRC_FILES))
LIB_DEPS      := $(LIB_OBJS:.o=.d)

STATIC_LIB    := $(LIB_DIR)/lib$(PROJECT).a
SHARED_LIB    := $(LIB_DIR)/$(LIB_PREFIX)$(PROJECT)$(SO_EXT)

TEST_NAMES    := $(patsubst $(TEST_DIR)/%.c,%,$(TEST_SRCS))
TEST_MOD_BINS := $(addprefix $(TEST_BIN_DIR)/mod_,$(addsuffix $(EXE),$(TEST_NAMES)))
TEST_AMG_BINS := $(addprefix $(TEST_BIN_DIR)/amg_,$(addsuffix $(EXE),$(TEST_NAMES)))
TEST_MOD_RUNS := $(addprefix run-mod-,$(TEST_NAMES))
TEST_AMG_RUNS := $(addprefix run-amg-,$(TEST_NAMES))

EX_BINS       := $(patsubst $(EX_DIR)/%.c,$(EX_BIN_DIR)/%$(EXE),$(EX_SRCS))
BENCH_BINS    := $(patsubst $(BENCH_DIR)/%.c,$(BENCH_BIN_DIR)/%$(EXE),$(BENCH_SRCS))

ALL_FMT_FILES := $(wildcard $(INC_DIR)/*.h $(SRC_DIR)/*.c $(SRC_DIR)/*.h \
                 $(BACKEND_DIR)/*.c $(BACKEND_DIR)/*.h $(TOOLS_DIR)/*.c \
                 $(TEST_DIR)/*.c $(TEST_DIR)/*.h $(EX_DIR)/*.c $(BENCH_DIR)/*.c)

# ------------------------------------------------------------------------------
# 4. Compiler & Linker Flags
# ------------------------------------------------------------------------------
CSTD          := -std=c99
WARN_FLAGS    := -Wall -Wextra -Werror -pedantic

ifeq ($(STRICT),1)
    WARN_FLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
                  -Wwrite-strings -Wcast-qual -Wundef
endif

DEP_FLAGS     := -MMD -MP
BASE_CFLAGS   := $(CSTD) $(WARN_FLAGS) $(DEP_FLAGS)
BASE_CPPFLAGS := -I$(INC_DIR) -I$(SRC_DIR)
BASE_LDFLAGS  :=
BASE_LDLIBS   :=

# Math library on Unix
ifeq ($(IS_WINDOWS),0)
    BASE_LDLIBS += -lm
endif

# Mode-specific flags
ifeq ($(MODE),debug)
    MODE_CFLAGS  := -g -O0 -DSOL_DEBUG
    MODE_LDFLAGS :=
else ifeq ($(MODE),release)
    MODE_CFLAGS  := -O3 -DNDEBUG
    MODE_LDFLAGS :=
else ifeq ($(MODE),sanitize)
    MODE_CFLAGS  := -g -O1 -DSOL_DEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
    MODE_LDFLAGS := -fsanitize=address,undefined
else ifeq ($(MODE),freestanding)
    MODE_CFLAGS  := -O2 -ffreestanding -nostdlib -DSOL_FREESTANDING
    MODE_LDFLAGS := -nostdlib
    BASE_LDLIBS  :=
else
    $(error Unknown MODE '$(MODE)'. Valid modes: debug, release, sanitize, freestanding)
endif

CFLAGS        := $(BASE_CFLAGS) $(MODE_CFLAGS) $(EXTRA_CFLAGS)
CPPFLAGS      := $(BASE_CPPFLAGS)
LDFLAGS       := $(BASE_LDFLAGS) $(MODE_LDFLAGS) $(EXTRA_LDFLAGS)
LDLIBS        := $(BASE_LDLIBS)

# Shared library flags
PIC_FLAG      :=
ifeq ($(IS_WINDOWS),0)
    PIC_FLAG := -fPIC
endif

# Amalgamated test flags (points strictly to dist/ without include/ or src/)
AMALG_TEST_CFLAGS := $(CSTD) $(WARN_FLAGS) $(MODE_CFLAGS) $(EXTRA_CFLAGS) \
                     -I$(DIST_DIR) -I$(TEST_DIR) -DSOL_BUILD_AMALGAMATED

# ------------------------------------------------------------------------------
# 5. Default & Phony Targets
# ------------------------------------------------------------------------------
.PHONY: all lib static shared amalgamate test test-modular test-amalgamated \
        test-san test-freestanding valgrind examples bench format format-check \
        lint install uninstall clean distclean info help \
        $(TEST_MOD_RUNS) $(TEST_AMG_RUNS)

all: static amalgamate

# ------------------------------------------------------------------------------
# 6. Core Library Build (Static & Shared)
# ------------------------------------------------------------------------------
lib: static

static: $(STATIC_LIB)

ifeq ($(strip $(LIB_OBJS)),)
$(STATIC_LIB):
	$(call LOG,INFO,No source files found in $(SRC_DIR)/ yet. Skipping static lib.)
else
$(STATIC_LIB): $(LIB_OBJS)
	$(Q)$(call MKDIR_P,$(LIB_DIR))
	$(call LOG,AR,$@)
	$(Q)$(AR) rcs $@ $^
endif

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	$(Q)$(call MKDIR_P,$(dir $@))
	$(call LOG,CC,$<)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Shared library build (compiles objects separately with PIC)
SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/shared/%.o,$(SRC_FILES))

shared: $(SHARED_LIB)

ifeq ($(strip $(SHARED_OBJS)),)
$(SHARED_LIB):
	$(call LOG,INFO,No source files found in $(SRC_DIR)/ yet. Skipping shared lib.)
else
$(SHARED_LIB): $(SHARED_OBJS)
	$(Q)$(call MKDIR_P,$(LIB_DIR))
	$(call LOG,LD-SO,$@)
	$(Q)$(CC) -shared $(LDFLAGS) $^ $(LDLIBS) -o $@
endif

$(OBJ_DIR)/shared/%.o: $(SRC_DIR)/%.c
	$(Q)$(call MKDIR_P,$(dir $@))
	$(call LOG,CC-PIC,$<)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(PIC_FLAG) -DSOL_BUILD_SHARED -c $< -o $@

# ------------------------------------------------------------------------------
# 7. Amalgamator Tool & Single-Header Generation
# ------------------------------------------------------------------------------
amalgamate: $(DIST_HEADER)

ifeq ($(wildcard $(AMALG_SRC)),)
$(DIST_HEADER):
	$(call LOG,INFO,$(AMALG_SRC) not found yet. Skipping amalgamation.)
else
$(AMALG_BIN): $(AMALG_SRC)
	$(Q)$(call MKDIR_P,$(dir $@))
	$(call LOG,CC-TOOL,$<)
	$(Q)$(CC) $(CSTD) $(WARN_FLAGS) -O2 $< -o $@

$(DIST_HEADER): $(AMALG_BIN) $(INC_FILES) $(SRC_HEADERS) $(SRC_FILES)
	$(Q)$(call MKDIR_P,$(DIST_DIR))
	$(call LOG,AMALG,$@)
	$(Q)$(AMALG_BIN) $(INC_DIR) $(SRC_DIR) $@
endif

# ------------------------------------------------------------------------------
# 8. Testing Targets (Modular + Amalgamated + Sanitizers + Freestanding)
# ------------------------------------------------------------------------------
test: test-modular test-amalgamated

ifeq ($(strip $(TEST_SRCS)),)
test-modular:
	$(call LOG,INFO,No tests found in $(TEST_DIR)/test_*.c yet.)

test-amalgamated:
	$(call LOG,INFO,No tests found in $(TEST_DIR)/test_*.c yet.)
else
test-modular: $(STATIC_LIB) $(TEST_MOD_RUNS)
	$(call LOG,PASS,All modular tests passed! [MODE=$(MODE)])

test-amalgamated: $(DIST_HEADER) $(TEST_AMG_RUNS)
	$(call LOG,PASS,All amalgamated single-header tests passed! [MODE=$(MODE)])
endif

# Build modular test binaries
$(TEST_BIN_DIR)/mod_%$(EXE): $(TEST_DIR)/%.c $(STATIC_LIB)
	$(Q)$(call MKDIR_P,$(TEST_BIN_DIR))
	$(call LOG,CC-TEST,$<)
	$(Q)$(CC) $(CPPFLAGS) -I$(TEST_DIR) $(CFLAGS) -DSOL_BUILD_MODULAR \
		$< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

# Build amalgamated test binaries (uses ONLY dist/solsticesfg.h)
$(TEST_BIN_DIR)/amg_%$(EXE): $(TEST_DIR)/%.c $(DIST_HEADER)
	$(Q)$(call MKDIR_P,$(TEST_BIN_DIR))
	$(call LOG,CC-AMG,$<)
	$(Q)$(CC) $(AMALG_TEST_CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

# Dynamic per-test execution targets (OS-agnostic & parallel-safe)
run-mod-%: $(TEST_BIN_DIR)/mod_%$(EXE)
	$(call LOG,RUN-MOD,$<)
	$(Q)$<

run-amg-%: $(TEST_BIN_DIR)/amg_%$(EXE)
	$(call LOG,RUN-AMG,$<)
	$(Q)$<

# Convenience shortcuts for Sanitizer & Freestanding checks
test-san:
	$(Q)$(MAKE) MODE=sanitize test

test-freestanding:
	$(Q)$(MAKE) MODE=freestanding static
	$(call LOG,PASS,Freestanding OS-kernel compilation check passed!)

valgrind: $(TEST_MOD_BINS)
ifeq ($(strip $(TEST_MOD_BINS)),)
	$(call LOG,INFO,No modular test binaries to check with Valgrind.)
else
	$(Q)for bin in $(TEST_MOD_BINS); do \
		echo "  VALGRIND   $$bin"; \
		$(VALGRIND) --leak-check=full --show-leak-kinds=all --error-exitcode=1 $$bin; \
	done
endif

# ------------------------------------------------------------------------------
# 9. Examples & Benchmarks
# ------------------------------------------------------------------------------
ifeq ($(strip $(EX_SRCS)),)
examples:
	$(call LOG,INFO,No examples found in $(EX_DIR)/ yet.)
else
examples: $(STATIC_LIB) $(EX_BINS)
endif

$(EX_BIN_DIR)/%$(EXE): $(EX_DIR)/%.c $(STATIC_LIB)
	$(Q)$(call MKDIR_P,$(EX_BIN_DIR))
	$(call LOG,CC-EX,$<)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

ifeq ($(strip $(BENCH_SRCS)),)
bench:
	$(call LOG,INFO,No benchmarks found in $(BENCH_DIR)/ yet.)
else
bench:
	$(Q)$(MAKE) MODE=release $(BENCH_BINS)
	$(Q)for b in $(BENCH_BINS); do \
		echo "  BENCH      $$b"; \
		$$b; \
	done
endif

$(BENCH_BIN_DIR)/%$(EXE): $(BENCH_DIR)/%.c $(STATIC_LIB)
	$(Q)$(call MKDIR_P,$(BENCH_BIN_DIR))
	$(call LOG,CC-BNCH,$<)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

# ------------------------------------------------------------------------------
# 10. Code Quality, Formatting & Static Analysis
# ------------------------------------------------------------------------------
format:
ifeq ($(strip $(ALL_FMT_FILES)),)
	$(call LOG,INFO,No C/H files found to format.)
else
	$(call LOG,FORMAT,Formatting all C/H sources...)
	$(Q)$(CLANG_FORMAT) -i $(ALL_FMT_FILES)
endif

format-check:
ifeq ($(strip $(ALL_FMT_FILES)),)
	$(call LOG,INFO,No C/H files found to check.)
else
	$(call LOG,FMT-CHK,Verifying .clang-format compliance...)
	$(Q)$(CLANG_FORMAT) --dry-run --Werror $(ALL_FMT_FILES)
endif

lint:
	$(call LOG,LINT,Running cppcheck static analysis...)
	$(Q)$(CPPCHECK) --enable=warning,style,performance,portability \
		--std=c99 --error-exitcode=1 --inline-suppr \
		-I$(INC_DIR) -I$(SRC_DIR) $(SRC_DIR) $(TOOLS_DIR)

# ------------------------------------------------------------------------------
# 11. Install / Uninstall & Cleanup
# ------------------------------------------------------------------------------
install: $(STATIC_LIB) $(DIST_HEADER)
	$(Q)$(call MKDIR_P,$(DESTDIR)$(PREFIX)/include)
	$(Q)$(call MKDIR_P,$(DESTDIR)$(PREFIX)/lib)
	$(call LOG,INSTALL,$(DESTDIR)$(PREFIX))
	$(Q)$(call CP_F,$(DIST_HEADER),$(DESTDIR)$(PREFIX)/include/$(HEADER_NAME))
	$(Q)$(call CP_F,$(STATIC_LIB),$(DESTDIR)$(PREFIX)/lib/lib$(PROJECT).a)

uninstall:
	$(call LOG,UNINSTALL,$(DESTDIR)$(PREFIX))
	$(Q)$(call RM_F,$(DESTDIR)$(PREFIX)/include/$(HEADER_NAME))
	$(Q)$(call RM_F,$(DESTDIR)$(PREFIX)/lib/lib$(PROJECT).a)

clean:
	$(call LOG,CLEAN,$(BUILD_ROOT)/)
	$(Q)$(call RM_RF,$(BUILD_ROOT))

distclean: clean
	$(call LOG,DISTCLEAN,$(DIST_HEADER))
	$(Q)$(call RM_F,$(DIST_HEADER))

# ------------------------------------------------------------------------------
# 12. Diagnostics & Help
# ------------------------------------------------------------------------------
info:
	@echo ============================================================
	@echo  Project        : $(PROJECT) v$(VERSION)
	@echo  OS / Shell     : Win=$(IS_WINDOWS) Linux=$(IS_LINUX) Mac=$(IS_MACOS) CMD=$(IS_CMD)
	@echo  Toolchain      : CC=$(CC) AR=$(AR) CROSS=$(CROSS_COMPILE)
	@echo  Mode / Strict  : MODE=$(MODE) STRICT=$(STRICT) V=$(V)
	@echo  CFLAGS         : $(CFLAGS)
	@echo  CPPFLAGS       : $(CPPFLAGS)
	@echo  LDFLAGS        : $(LDFLAGS)
	@echo  Sources        : $(words $(SRC_FILES)) file(s) in $(SRC_DIR)/
	@echo  Tests          : $(words $(TEST_SRCS)) file(s) in $(TEST_DIR)/
	@echo ============================================================

help:
	@echo SolsticeSFG Build System Targets:
	@echo   make [all]             - Build static lib + amalgamated single-header
	@echo   make static            - Build static library ($(STATIC_LIB))
	@echo   make shared            - Build shared library ($(SHARED_LIB))
	@echo   make amalgamate        - Build tool and generate $(DIST_HEADER)
	@echo   make test              - Run modular AND amalgamated test suites
	@echo   make test-modular      - Run tests linked against $(STATIC_LIB)
	@echo   make test-amalgamated  - Run tests against $(DIST_HEADER) only
	@echo   make test-san          - Run all tests with ASan + UBSan
	@echo   make test-freestanding - Verify zero-libc compilation (-ffreestanding)
	@echo   make valgrind          - Run modular tests under Valgrind (Unix)
	@echo   make examples          - Build programs in $(EX_DIR)/
	@echo   make bench             - Build and run benchmarks in release mode
	@echo   make format            - Format code using .clang-format
	@echo   make format-check      - Verify formatting (CI check)
	@echo   make lint              - Run cppcheck static analysis
	@echo   make install           - Install header and static lib to PREFIX
	@echo   make clean             - Remove $(BUILD_ROOT)/ directory
	@echo   make distclean         - Remove $(BUILD_ROOT)/ and generated $(DIST_HEADER)
	@echo   make info              - Print detected environment and active flags
	@echo.
	@echo Variables:
	@echo   MODE=debug^|release^|sanitize^|freestanding  (default: debug)
	@echo   CC=gcc^|clang^|...                          (default: gcc)
	@echo   CROSS_COMPILE=x86_64-w64-mingw32-         (optional prefix)
	@echo   STRICT=1^|0                                (extra warnings, default: 1)
	@echo   V=0^|1                                     (verbose commands, default: 0)

# Include auto-generated header dependency files
-include $(LIB_DEPS)
