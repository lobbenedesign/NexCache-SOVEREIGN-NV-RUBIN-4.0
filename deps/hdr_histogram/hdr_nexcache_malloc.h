#ifndef HDR_MALLOC_H__
#define HDR_MALLOC_H__

void *nexcache_malloc(size_t size);
void *zcalloc_num(size_t num, size_t size);
void *nexcache_realloc(void *ptr, size_t size);
void nexcache_free(void *ptr);

#define hdr_malloc nexcache_malloc
#define hdr_calloc zcalloc_num
#define hdr_realloc nexcache_realloc
#define hdr_free nexcache_free
#endif
