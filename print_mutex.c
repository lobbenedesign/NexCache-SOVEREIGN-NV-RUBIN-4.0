#include <stdio.h>
#include <pthread.h>

int main() {
    pthread_mutex_t m;
    pthread_mutex_init(&m, NULL);
    pthread_mutex_lock(&m);
    pthread_mutex_unlock(&m);
    unsigned long long *p = (unsigned long long *)&m;
    printf("Mutex bytes after unlock: 0x%016llx 0x%016llx\n", p[0], p[1]);
    return 0;
}
