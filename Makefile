# NexCache Makefile — v3.0 (Omnibus Release)
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

# Flags SIMD (Haswell safe per GitHub)
ifeq ($(ARCH),x86_64)
    SIMD_FLAGS := -msse4.1 -mavx2 -mfma
endif

BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests

# --- SCOPERTA AUTOMATICA DEI SORGENTI ---
# Prendiamo tutti i file .c nella cartella src e sottocartelle
SRCS := $(shell find $(SRC_DIR) -name "*.c" | grep -v "enterprise" | grep -v "valkey-")

OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
LIB  := $(BUILD_DIR)/libnexcache.a
TEST_BINS := $(BUILD_DIR)/test_arena $(BUILD_DIR)/test_core_v2 $(BUILD_DIR)/test_v4

.PHONY: all clean dirs tests

all: dirs $(LIB) $(TEST_BINS)
	@echo "✅ NexCache build COMPLETO con successo (Omnibus mode)"

dirs:
	@mkdir -p $(shell find $(SRC_DIR) -type d | sed 's|^$(SRC_DIR)|$(BUILD_DIR)|')

$(LIB): $(OBJS)
	@echo "  [AR]  $@"
	@ar rcs $@ $(OBJS)

# Regola speciale per SIMD
$(BUILD_DIR)/vector/quantization.o: $(SRC_DIR)/vector/quantization.c
	@mkdir -p $(dir $@)
	@echo "  [CC]  $< (SIMD Optimized)"
	@$(CC) $(CFLAGS) $(SIMD_FLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) $< -o $@ -L$(BUILD_DIR) -lnexcache $(LDFLAGS)

clean:
	@rm -rf $(BUILD_DIR)
