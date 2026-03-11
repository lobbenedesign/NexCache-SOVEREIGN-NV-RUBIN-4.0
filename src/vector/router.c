#include "router.h"

NexVectorDecision nex_vector_route(uint64_t count, int dim, int has_filters, const NexVectorCaps *caps) {
    NexVectorDecision d;
    d.recall_target = 0.99f;
    d.use_gpu = 0;

    // Auto-select quantization precision logic
    d.precision = nexvec_auto_quantization(caps, count, d.recall_target);

    // AI Semantic Routing for index type
    if (caps && caps->gpu_available && count > 100000) {
        d.algo = ALGO_CAGRA;
        d.use_gpu = 1;
    } else if (has_filters) {
        d.algo = ALGO_FILTERED; // Filtered-DiskANN logic
    } else if (count > 10000000) {
        d.algo = ALGO_DISKANN; // SSD offloading for giant datasets (CXL compatible)
    } else if (count > 100000) {
        d.algo = ALGO_HNSW; // RAM optimized HNSW
    } else {
        d.algo = ALGO_FLAT; // Perfect recall for small datasets
    }

    return d;
}
