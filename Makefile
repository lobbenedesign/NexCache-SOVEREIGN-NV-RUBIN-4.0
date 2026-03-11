# NexCache Makefile — v2.4 (Enterprise Quality)
# ============================================================
CC      ?= gcc
CFLAGS  += -O3 -std=c11 -Wall -Wextra -pthread -D_GNU_SOURCE \
            -Isrc -Isrc/memory -Isrc/core -Isrc/vector -Isrc/hashtable \
            -Isrc/segcache -Isrc/crdt -Isrc/bloom -Isrc/network -Isrc/security

UNAME_S := $(shell uname -s)
ARCH    := $(shell uname -m)

LDFLAGS += -lpthread -lm
ifneq ($(UNAME_S),Darwin)
    LDFLAGS += -lrt
endif

# Detection Librerie Esterne
LZ4_FOUND  := $(shell pkg-config --exists liblz4 && echo yes || echo no)
ZSTD_FOUND := $(shell pkg-config --exists libzstd && echo yes || echo no)

ifeq ($(LZ4_FOUND),yes)
    CFLAGS  += -DHAVE_LZ4
    LDFLAGS += -llz4
endif
ifeq ($(ZSTD_FOUND),yes)
    CFLAGS  += -DHAVE_ZSTD
    LDFLAGS += -lzstd
endif

# Flags SIMD
ifeq ($(ARCH),x86_64)
    SIMD_FLAGS := -msse4.1 -mavx2 -mfma
else
    SIMD_FLAGS := 
endif

BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests

# --- SORGENTI DISPOSTI PER MODULO ---
SRCS := $(SRC_DIR)/memory/arena.c \
        $(SRC_DIR)/memory/hybrid.c \
        $(SRC_DIR)/memory/arch_probe.c \
        $(SRC_DIR)/memory/hazard_ptr.c \
        $(SRC_DIR)/core/engine.c \
        $(SRC_DIR)/core/scheduler.c \
        $(SRC_DIR)/core/vll.c \
        $(SRC_DIR)/core/subkey_ttl.c \
        $(SRC_DIR)/core/nexstorage.c \
        $(SRC_DIR)/core/planes.c \
        $(SRC_DIR)/hashtable/nexdash.c \
        $(SRC_DIR)/segcache/segcache.c \
        $(SRC_DIR)/crdt/crdt.c \
        $(SRC_DIR)/bloom/nexbloom.c \
        $(SRC_DIR)/vector/quantization.c \
        $(SRC_DIR)/vector/router.c \
        $(SRC_DIR)/vector/hnsw.c \
        $(SRC_DIR)/network/protocol_detect.c \
        $(SRC_DIR)/network/websocket.c \
        $(SRC_DIR)/security/quota.c

OBJS      := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
LIB       := $(BUILD_DIR)/libnexcache.a
TEST_BINS := $(BUILD_DIR)/test_arena $(BUILD_DIR)/test_core_v2 $(BUILD_DIR)/test_v4

.PHONY: all clean dirs tests

all: dirs $(LIB) $(TEST_BINS)
	@echo "✅ NexCache v1.0 Release Build Success"

dirs:
	@mkdir -p $(BUILD_DIR)/memory $(BUILD_DIR)/core $(BUILD_DIR)/vector \
	           $(BUILD_DIR)/hashtable $(BUILD_DIR)/segcache $(BUILD_DIR)/crdt \
	           $(BUILD_DIR)/bloom $(BUILD_DIR)/network $(BUILD_DIR)/security

# Creazione Libreria
$(LIB): $(OBJS)
	@echo "  [AR]  $@"
	@ar rcs $@ $(OBJS)

# Regola per SIMD (Assicura che dirs esista)
$(BUILD_DIR)/vector/quantization.o: $(SRC_DIR)/vector/quantization.c | dirs
	@echo "  [CC]  $< (SIMD Optimized)"
	@$(CC) $(CFLAGS) $(SIMD_FLAGS) -c $< -o $@

# Regola generale per oggetti (Assicura che dirs esista)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Linker per Test
$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) $< -o $@ -L$(BUILD_DIR) -lnexcache $(LDFLAGS)

clean:
	@rm -rf $(BUILD_DIR)
