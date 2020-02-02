#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct PomMapBucket PomMapBucket;
typedef struct PomMapDataHeap PomMapDataHeap;

typedef struct PomMapCtx{
    uint32_t numBuckets;
    uint32_t numNodes;
    PomMapBucket *buckets;
    PomMapDataHeap *dataHeap;
    bool initialised;
}PomMapCtx;

// Initialise the map with optional starting size suggestion
int pomMapInit( PomMapCtx *_ctx, uint32_t _size );

// Get a key if it exists, return `_default` otherwise
const char* pomMapGet( PomMapCtx *_ctx, const char * _key, const char * _default );

// Set a key to a given value
const char* pomMapSet( PomMapCtx *_ctx, const char * _key, const char * _value );

// Get a key if it exists, otherwise add a new node with value `_default`
const char* pomMapGetSet( PomMapCtx *_ctx, const char * _key, const char * _default );

// Remove a key
int pomMapRemove( PomMapCtx *_ctx, const char * _key );

// Clean up the map
int pomMapClear( PomMapCtx *_ctx );

// Suggest a resize to the map
int pomMapResize( PomMapCtx *_ctx, uint32_t _size );

// Try optimise the data stored to help with caching
int pomMapOptimise( PomMapCtx *_ctx );

// Request an immutable pointer to the data heap
const char * pomMapGetDataHeap( PomMapCtx *_ctx, size_t *_heapUsed, size_t *_heapSize );

#endif // HASHMAP_H