
#include <stddef.h>
#include <stdint.h>
int __attribute__((weak)) RegisterConnectionTypeRdma(void) { return 0; }
int __attribute__((weak)) NexCacheRegisterConnectionTypeTLS(void) { return 0; }
typedef struct { const char *name; void *data[32]; } NexStorageAPI_Stub;
const NexStorageAPI_Stub __attribute__((weak)) NexCloudTierAPI = { "NexCloudTier" };
int __attribute__((weak)) nexvec_auto_quantization(void *caps, size_t n_vectors, float recall_target) { return 0; }

