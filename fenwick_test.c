#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int num_hashtables_bits = 8;
int num_hashtables = 176;
unsigned long long hashtable_size_index[257];

void cumulativeKeyCountAdd(int didx, long delta) {
    int idx = didx + 1;
    int max_idx = 1 << num_hashtables_bits;
    while (idx <= max_idx) {
        hashtable_size_index[idx] += delta;
        idx += (idx & -idx);
    }
}

int kvstoreFindHashtableIndexByKeyIndex(unsigned long target) {
    int result = 0, bit_mask = 1 << num_hashtables_bits;
    for (int i = bit_mask; i != 0; i >>= 1) {
        int current = result + i;
        if (current <= (1<<num_hashtables_bits) && target > hashtable_size_index[current]) {
            target -= hashtable_size_index[current];
            result = current;
        }
    }
    return result;
}

int main() {
    for (int i=0; i<176; i++) {
        cumulativeKeyCountAdd(i, 1);
    }
    printf("Total keys: %llu\n", hashtable_size_index[256]);
    for (unsigned long t=1; t<=176; t++) {
        int r = kvstoreFindHashtableIndexByKeyIndex(t);
        if (r < 0 || r >= 176) {
            printf("FAIL for target %lu: result %d\n", t, r);
            return 1;
        }
    }
    printf("SUCCESS\n");
    return 0;
}
