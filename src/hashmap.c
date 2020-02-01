
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

#define POM_MAP_DEFAULT_SIZE 32 // Default number of buckets in list
#define POM_MAP_HEAP_SIZE 256   // Default size of the data heaps

typedef struct PomMapNode{
    const char * key;
    const char * value;
    struct PomMapNode * next;
}PomMapNode;

struct PomMapBucket{
    PomMapNode * listHead;
};

struct PomMapDataHeap{
    char * heap;
    uint32_t numHeapBlocks;
    size_t heapUsed;

};

inline uint32_t pomNextPwrTwo( uint32_t _size );
inline uint32_t pomMapHashFunc( PomMapCtx *_ctx, const char * key );

uint32_t pomNextPwrTwo( uint32_t _size ){
    // From bit-twiddling hacks (Stanford)
    _size--;
    _size |= _size >> 1;
    _size |= _size >> 2;
    _size |= _size >> 4;
    _size |= _size >> 8;
    _size |= _size >> 16;
    _size++;
    return _size;
}

uint32_t pomMapHashFunc( PomMapCtx *_ctx, const char * key ){
    
    // Use sbdm hashing function
    uint32_t hash = 0;
    int c = *key;

    while( c ){
        hash = c + (hash << 6) + (hash << 16) - hash;
        c = *key++;
    }

    // Mask off for the number of buckets we actually can index
    return hash & ( _ctx->numBuckets - 1 );
}

// Initialise the map with optional starting size suggestion
int pomMapInit( PomMapCtx *_ctx, uint32_t _size ){
    if( _size == 0 ){
        _size = POM_MAP_DEFAULT_SIZE;
    }

    // Round size to next power of 2
    _size = pomNextPwrTwo( _size );
    
    // Allocate space for buckets, and zero the memory (set all list heads to NULL)
    _ctx->buckets = (PomMapBucket*) calloc( _size, sizeof( PomMapBucket ) );
    _ctx->dataHeap = (PomMapDataHeap*) calloc( _size, sizeof( PomMapDataHeap ) );
    _ctx->dataHeap->heap = (char*) calloc( POM_MAP_HEAP_SIZE, sizeof( char ) );
    _ctx->dataHeap->numHeapBlocks = 1;
    _ctx->dataHeap->heapUsed = 0;
    _ctx->initialised = true;
    _ctx->numBuckets = _size;
    _ctx->numNodes = 0;
    return 0;
}


int pomMapAddData( PomMapCtx *_ctx, const char *_key, const char *_value ){
    // TODO add error handling in this function

    size_t curSize = _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE;
    size_t curUsed = _ctx->dataHeap->heapUsed;
    size_t keyLen = strlen( _key ) + 1;
    size_t valLen = strlen( _value ) + 1;
    size_t newDataLen = keyLen + valLen;
    size_t newHeapUsed = curUsed + newDataLen;

    if( newHeapUsed > curSize ){
        // Increase block size
        _ctx->dataHeap->numHeapBlocks++;
        size_t newHeapSize = _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE;
        // TODO check for error on realloc
        _ctx->dataHeap->heap = realloc( _ctx->dataHeap->heap, newHeapSize );
        // Call recursively in case the new key/value pair exceeds block size
        // TODO change this from recursive to just checking for that here
        return pomMapAddData( _ctx, _key, _value );
    }
    // From here we have enough space in the heap for the new data
    char * keyLoc = _ctx->dataHeap->heap + curUsed;
    memcpy( keyLoc, _key, keyLen );
    curUsed += keyLen;
    char * valLoc = _ctx->dataHeap->heap + curUsed;
    memcpy( valLoc, _value, valLen );
    _ctx->dataHeap->heapUsed = curUsed + valLen;

    // Return the new data locations in the original params
    _key = keyLoc;
    _value = valLoc;

    return 0;
}

PomMapNode * pomMapCreateNode( PomMapCtx *_ctx, const char *_key, const char *_value ){
    pomMapAddData( _ctx, _key, _value );
    // Now create the new node
    PomMapNode * newNode = (PomMapNode*) malloc( sizeof( PomMapNode ) );
    newNode->key = _key;
    newNode->value = _value;
    newNode->next = NULL;
    
    return newNode;
}

// Return a double pointer to either a true node, or a reference to
// a node (ie `next` ptr in linked list)
PomMapNode ** pomMapGetNodeRef( PomMapCtx *_ctx, const char *_key ){
    uint32_t idx = pomMapHashFunc( _ctx, _key );
    PomMapBucket * bucket = &_ctx->buckets[ idx ];
    PomMapNode ** node = &(bucket->listHead);
    while( *node ){
        if( strcmp( _key, (*node)->key ) == 0 ){
            return node;
            break;
        }
        node = &(*node)->next;
    }
    return node;
}

// Get a key if it exists, return `_default` otherwise
const char* pomMapGet( PomMapCtx *_ctx, const char * _key, const char * _default ){

    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( !*node ){
        // Node was not found
        return _default;
    }
    // Node was found
    return (*node)->key;
}

// Set a key to a given value
const char* pomMapSet( PomMapCtx *_ctx, const char * _key, const char * _value ){
    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( *node ){
        // Node exists, so set data
        pomMapAddData( _ctx, _key, _value );
        (*node)->key = _key;
        (*node)->value = _value;
        return _value;
    }

    // Node doesn't exist so needs to be added
    PomMapNode * newNode = pomMapCreateNode( _ctx, _key, _value );
    // Node points to the `next` ptr in the linked list of a given bucket
    *node = newNode;
    _ctx->numNodes++;

    // Check if we need to resize
    if( _ctx->numNodes > _ctx->numBuckets ){
        // Resize the bucket array
        uint32_t newSize = _ctx->numBuckets << 1;
        pomMapResize( _ctx, newSize );
    }

    // Resize may have shuffled data around, so return a valid value pointer
    return newNode->value;
}

// Get a key if it exists, otherwise add a new node with value `_default`
const char* pomMapGetSet( PomMapCtx *_ctx, const char * _key, const char * _default ){
    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( *node ){
        // Node exists, so return data
        return (*node)->value;
    }

        // Node doesn't exist so needs to be added
    PomMapNode * newNode = pomMapCreateNode( _ctx, _key, _default );
    // Node points to the `next` ptr in the linked list of a given bucket
    *node = newNode;
    _ctx->numNodes++;

    // Check if we need to resize
    if( _ctx->numNodes > _ctx->numBuckets ){
        // Resize the bucket array
        uint32_t newSize = _ctx->numBuckets << 1;
        pomMapResize( _ctx, newSize );
    }

    // Resize may have shuffled data around, so return a valid value pointer
    return newNode->value;
}


// Clean up the map
int pomMapClear( PomMapCtx *_ctx ){
    for( int i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * curNode = curBucket->listHead;
        PomMapNode * nextNode = NULL;
        while( curNode ){
            nextNode = curNode->next;
            free( curNode );
            curNode = nextNode;
        }
    }
    free( _ctx->buckets );
    _ctx->numNodes = 0;
    _ctx->numBuckets = 0;
    _ctx->initialised = false;
    return 0;
}

// Suggest a resize to the map
int pomMapResize( PomMapCtx *_ctx, uint32_t _size ){
    // TODO - optimise the data heap here
    if( !_ctx->initialised || _size <= _ctx->numBuckets ){
        // No need to resize
        return 1;
    }

    // Round `_size` to a power of 2
    _size = pomNextPwrTwo( _size );

    // Construct long linked list from all nodes
    PomMapNode * nodeHead = NULL;
    PomMapNode * lastNode = NULL;
    for( int i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * nodeIter = curBucket->listHead;
        if( !nodeIter ){
            // Empty bucket
            continue;
        }
        if( !nodeHead ){
            // First node in the big list
            nodeHead = nodeIter;
        }
        else{
            // Connect up first node in this bucket to last node in last bucket
            lastNode->next = nodeIter;
        }
        while( nodeIter->next ){
            // Cycle through to find the last node in this bucket
            nodeIter = nodeIter->next;
        }
        lastNode = nodeIter;
    }
    
    // Now reallocate the bucket array with the new size and zero it
    _ctx->buckets = (PomMapBucket*) realloc( _ctx->buckets, _size );
    memset( _ctx->buckets, 0, sizeof( PomMapBucket ) *_size );

    // Add all nodes from linked list to new bucket array
    PomMapNode * curNode = nodeHead;
    while( curNode ){
        PomMapNode * nextNode = curNode->next;
        // Break apart the list as we go
        curNode->next = NULL;

        uint32_t idx = pomMapHashFunc( _ctx, curNode->key );
        PomMapBucket * bucket = &_ctx->buckets[ idx ];
        PomMapNode ** node = &(bucket->listHead);

        // Cycle to the end of the bucket's list
        while( *node ){
            node = &((*node)->next);
        }
        // Add the current node to the end of the bucket's list
        *node = curNode;

        curNode = nextNode;
    }

    return 0;
}