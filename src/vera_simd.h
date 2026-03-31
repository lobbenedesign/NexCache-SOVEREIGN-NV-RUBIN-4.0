/* NEX-VERA: SVE2 Vectorized SIMD Utilities (GODMODE) 
 * Optimized for NVIDIA Rubin / ARMv9-A Architecture.
 */
#ifndef VERA_SIMD_H
#define VERA_SIMD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>

/* NEX-VERA: SVE2-optimized multi-char delimiter finding.
 * Scans for any character in 'delims' (up to 16 chars) in 's' up to 'n' bytes. */
static inline const char *vera_sve2_find_any(const char *s, size_t n, const uint8_t *delims, size_t dlen) {
    svuint8_t v_delims = svld1_u8(svptrue_b8(), delims);
    for (size_t i = 0; i < n; i += svcntb()) {
        svbool_t pg = svwhilelt_b8((uint32_t)i, (uint32_t)n);
        svuint8_t data = svld1_u8(pg, (const uint8_t*)(s + i));
        svbool_t cmp = svmatch_u8(pg, data, v_delims);
        if (svptest_any(pg, cmp)) {
            size_t pos = svcntp_b8(pg, svbrkb_b_z(pg, cmp));
            return s + i + pos;
        }
    }
    return NULL;
}
#endif

/* Fallback: Generic delimiter searching. */
static inline const char *vera_find_any(const char *s, size_t n, const uint8_t *delims, size_t dlen) {
#ifdef __ARM_FEATURE_SVE
    return vera_sve2_find_any(s, n, delims, dlen);
#else
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < dlen; j++) {
            if ((uint8_t)s[i] == delims[j]) return s + i;
        }
    }
    return NULL;
#endif
}

#endif /* VERA_SIMD_H */
