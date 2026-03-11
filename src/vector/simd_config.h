#ifndef NEX_SIMD_CONFIG_H
#define NEX_SIMD_CONFIG_H

/* Forza il supporto hardware per il compilatore su x86 */
#if defined(__x86_64__) || defined(_M_X64)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC target("sse4.1,avx2")
#endif
#endif

#endif /* NEX_SIMD_CONFIG_H */
