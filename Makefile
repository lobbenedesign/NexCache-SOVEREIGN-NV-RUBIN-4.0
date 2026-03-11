# NexCache Makefile — v2.0
# ============================================================
CC      ?= gcc
CFLAGS  := -O2 -std=c11 -Wall -Wextra -Wpedantic \
            -pthread -D_GNU_SOURCE \
            -Isrc -Isrc/memory -Isrc/core -Isrc/vector \
            -Isrc/ai -Isrc/network -Isrc/security \
            -Isrc/compression -Isrc/wasm -Isrc/consensus \
            -Isrc/streams -Isrc/observability \
            -Isrc/persistence -Isrc/cluster

UNAME_S := $(shell uname -s)
ARCH    := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
    LDFLAGS := -lpthread -lm
else
    LDFLAGS := -lpthread -lm -lrt
endif

# Flags per accelerazione hardware (applicate solo dove serve)
SIMD_FLAGS :=
ifeq ($(ARCH),x86_64)
    SIMD_FLAGS := -msse4.1 -mavx2 -mfma
endif

# Detection LZ4/Zstd...
LZ4_AVAILABLE  := $(shell pkg-config --exists liblz4  2>/dev/null && echo yes || echo no)
ZSTD_AVAILABLE := $(shell pkg-config --exists libzstd 2>/dev/null && echo yes || echo no)

ifeq ($(LZ4_AVAILABLE),yes)
    CFLAGS += -DHAVE_LZ4 $(shell pkg-config --cflags liblz4)
    LDFLAGS += $(shell pkg-config --libs liblz4)
endif
ifeq ($(ZSTD_AVAILABLE),yes)
    CFLAGS += -DHAVE_ZSTD $(shell pkg-config --cflags libzstd)
    LDFLAGS += $(shell pkg-config --libs libzstd)
endif

# Directory e Sorgenti
BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests

SRCS_MEMORY := $(SRC_DIR)/memory/arena.c $(SRC_DIR)/memory/hybrid.c $(SRC_DIR)/memory/arch_probe.c $(SRC_DIR)/memory/hazard_ptr.c
SRCS_CORE   := $(SRC_DIR)/core/engine.c $(SRC_DIR)/core/scheduler.c $(SRC_DIR)/bloom/nexbloom.c
SRCS_VECTOR := $(SRC_DIR)/vector/router.c $(SRC_DIR)/vector/quantization.c $(SRC_DIR)/vector/hnsw.c
SRCS_HASHTABLE := $(SRC_DIR)/hashtable/nexdash.c
SRCS_SEGCACHE  := $(SRC_DIR)/segcache/segcache.c
SRCS_CRDT      := $(SRC_DIR)/crdt/crdt.c
SRCS_CORE_V4   := $(SRC_DIR)/core/vll.c $(SRC_DIR)/core/subkey_ttl.c $(SRC_DIR)/core/nexstorage.c $(SRC_DIR)/core/planes.c

ALL_SRCS := $(SRCS_MEMORY) $(SRCS_CORE) $(SRCS_VECTOR) $(SRCS_HASHTABLE) $(SRCS_SEGCACHE) $(SRCS_CRDT) $(SRCS_CORE_V4)
ALL_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SRCS))

LIBRARY := $(BUILD_DIR)/libnexcache.a
TESTS   := $(BUILD_DIR)/test_arena $(BUILD_DIR)/test_core_v2 $(BUILD_DIR)/test_advanced $(BUILD_DIR)/test_v4

.PHONY: all clean test dirs banner

all: banner dirs $(LIBRARY) $(TESTS)
	@echo "✅ NexCache v1.0 build COMPLETO"

banner:
	@echo "╔══════════════════════════════════════════╗"
	@echo "║   NexCache — Professional Build System   ║"
	@echo "╚══════════════════════════════════════════╝"
	@echo "  Arch: $(ARCH) | OS: $(UNAME_S)"

dirs:
	@mkdir -p $(BUILD_DIR)/memory $(BUILD_DIR)/core $(BUILD_DIR)/vector $(BUILD_DIR)/hashtable $(BUILD_DIR)/segcache $(BUILD_DIR)/crdt $(BUILD_DIR)/bloom

$(LIBRARY): $(ALL_OBJS)
	@echo "  [AR]  $@"
	@ar rcs $@ $^

# --- REGOLA CHIRURGICA PER SIMD ---
# Applichiamo SIMD_FLAGS *solo* a quantization.o per non "snaturare" il resto del binario
$(BUILD_DIR)/vector/quantization.o: $(SRC_DIR)/vector/quantization.c
	@echo "  [CC]  $< (SIMD Optimized)"
	@$(CC) $(CFLAGS) $(SIMD_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIBRARY)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) $< -o $@ -L$(BUILD_DIR) -lnexcache $(LDFLAGS)

test: all
	@for t in $(TESTS); do echo "Running $$t..."; $$t || exit 1; done

clean:
	@rm -rf $(BUILD_DIR)
