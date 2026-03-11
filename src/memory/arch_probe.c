#include "arch_probe.h"
#include <string.h>
#include <stdio.h>

#if defined(__aarch64__)

// ─── STRATEGIA ARM64 ────────────────────────────────────────────────
#if defined(__linux__)
#include <sys/auxv.h>
#ifndef HWCAP2_PAC
#define HWCAP2_PAC (1UL << 17) // Definito in asm/hwcap.h dal kernel 5.8
#endif
#ifndef HWCAP2_ADDRESS_AUTH_ARCH_QARMA5
#define HWCAP2_ADDRESS_AUTH_ARCH_QARMA5 (1UL << 28)
#endif
#endif

static int aarch64_get_va_bits(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return 48; // fallback conservativo
    uintptr_t max_addr = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        uintptr_t start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2)
            if (end > max_addr) max_addr = end;
    }
    fclose(f);
    // Converti max_addr in numero di bit
    int bits = 0;
    while ((1ULL << bits) <= max_addr) bits++;
    return (bits < 39) ? 48 : bits; // minimo 48 bit
}

static void probe_aarch64(NexArchInfo *info) {
    bool pac_hw = false;
#if defined(__linux__)
    unsigned long hwcap2 = getauxval(AT_HWCAP2);
    pac_hw = !!(hwcap2 & HWCAP2_PAC) ||
             !!(hwcap2 & HWCAP2_ADDRESS_AUTH_ARCH_QARMA5);
#elif defined(__APPLE__)
    // Su Apple Silicon PAC è sempre abilitato, TBI in user space no
    pac_hw = true;
#endif

    info->tbi_enabled = true;
    info->pac_enabled = pac_hw;

    int va_bits = aarch64_get_va_bits();

    // Con TBI sempre abilitato: bit [63:56] sono SEMPRE liberi (8 bit)
    info->metadata_shift = 56;
    info->metadata_bits = 8;
    info->addr_mask = 0x00FFFFFFFFFFFFFF;

    snprintf(info->description, sizeof(info->description),
             "ARM64: TBI=%d PAC=%d VA=%d-bit | metadata=[63:56] (8 bit)",
             info->tbi_enabled, info->pac_enabled, va_bits);
}

#elif defined(__x86_64__) || defined(_M_X64)

// ─── STRATEGIA x86-64 ───────────────────────────────────────────────
#include <cpuid.h>

static bool x86_detect_la57(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    uintptr_t max_addr = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        uintptr_t s, e;
        if (sscanf(line, "%lx-%lx", &s, &e) == 2)
            if (e > max_addr) max_addr = e;
    }
    fclose(f);
    return max_addr > 0x0000800000000000ULL;
}

static bool x86_detect_lam(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid_count(7, 1, eax, ebx, ecx, edx);
    return !!(eax & (1u << 26));
}

static void probe_x86_64(NexArchInfo *info) {
    info->la57_enabled = x86_detect_la57();
    info->lam_enabled = x86_detect_lam();
    info->tbi_enabled = false;
    info->pac_enabled = false;

    if (info->la57_enabled) {
        info->metadata_shift = 57;
        info->metadata_bits = 7;
        info->addr_mask = 0x01FFFFFFFFFFFFFF;
        snprintf(info->description, sizeof(info->description),
                 "x86-64: LA57=yes LAM=%d | metadata=[63:57] (7 bit)",
                 info->lam_enabled);
    } else {
        info->metadata_shift = 48;
        info->metadata_bits = 16;
        info->addr_mask = 0x0000FFFFFFFFFFFF;
        snprintf(info->description, sizeof(info->description),
                 "x86-64: LA57=no LAM=%d | metadata=[63:48] (16 bit)",
                 info->lam_enabled);
    }
}

#else
static void probe_unsupported(NexArchInfo *info) {
    info->metadata_shift = 0;
    info->metadata_bits = 0;
    info->addr_mask = 0;
}
#endif

// ─── IMPLEMENTAZIONE COMUNE ─────────────────────────────────────────
NexArchInfo g_nexarch = {0};

int nexarch_probe(void) {
    memset(&g_nexarch, 0, sizeof(g_nexarch));

#if defined(__aarch64__)
    probe_aarch64(&g_nexarch);
#elif defined(__x86_64__) || defined(_M_X64)
    probe_x86_64(&g_nexarch);
#else
    probe_unsupported(&g_nexarch);
    return -1;
#endif

    if (g_nexarch.metadata_bits < 6) {
        fprintf(stderr, "NEXCACHE FATAL: architettura non supporta "
                        "abbastanza bit liberi per tagged pointers "
                        "(%d bit disponibili, minimo 6)\n",
                g_nexarch.metadata_bits);
        return -1;
    }

    fprintf(stderr, "NexCache arch: %s\n", g_nexarch.description);
    return 0;
}
