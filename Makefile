# NexCache Makefile — v2.0
# ============================================================
CC      ?= gcc
CFLAGS  += -O2 -std=c11 -Wall -Wextra -Wpedantic -pthread -D_GNU_SOURCE \
            -Isrc -Isrc/memory -Isrc/core -Isrc/vector -Isrc/hashtable \
            -Isrc/segcache -Isrc/crdt -Isrc/bloom -Isrc/network -Isrc/security

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

BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests

# --- MODULI COMMUNITY EDITION (CE) ---
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

OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
LIB  := $(BUILD_DIR)/libnexcache.a
TESTS := $(BUILD_DIR)/test_arena $(BUILD_DIR)/test_core_v2 $(BUILD_DIR)/test_advanced $(BUILD_DIR)/test_v4

.PHONY: all clean test dirs banner

all: banner dirs $(LIB) $(TESTS)
	@echo "✅ NexCache v1.0 build COMPLETO"

banner:
	@echo "╔══════════════════════════════════════════╗"
	@echo "║   NexCache — Professional Build System   ║"
	@echo "╚══════════════════════════════════════════╝"
	@echo "  Arch: $(ARCH) | OS: $(UNAME_S)"

dirs:
	@mkdir -p $(BUILD_DIR)/memory $(BUILD_DIR)/core $(BUILD_DIR)/vector \
	           $(BUILD_DIR)/hashtable $(BUILD_DIR)/segcache $(BUILD_DIR)/crdt \
	           $(BUILD_DIR)/bloom $(BUILD_DIR)/network $(BUILD_DIR)/security

$(LIB): $(OBJS)
	@echo "  [AR]  $@"
	@ar rcs $@ $(OBJS)

# --- REGOLA CHIRURGICA PER SIMD ---
$(BUILD_DIR)/vector/quantization.o: $(SRC_DIR)/vector/quantization.c
	@echo "  [CC]  $< (SIMD Optimized)"
	@$(CC) $(CFLAGS) $(SIMD_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) $< -o $@ -L$(BUILD_DIR) -lnexcache $(LDFLAGS)

test: all
	@for t in $(TESTS); do echo "Running $$t..."; $$t || exit 1; done

clean:
	@rm -rf $(BUILD_DIR)
