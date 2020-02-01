
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

#define POM_MAP_DEFAULT_SIZE 32

typedef struct PomMapNode{
    const char * key;
    const char * value;
    struct PomMapNode * next;
}PomMapNode;

struct PomMapBucket{
    PomMapNode * listHead;
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
    _ctx->initialised = true;
    _ctx->numBuckets = _size;
    _ctx->numNodes = 0;
    return 0;
}

// Get a key if it exists, return `_default` otherwise
const char* pomMapGet( PomMapCtx *_ctx, const char * _key, const char * _default ){
    uint32_t idx = pomMapHashFunc( _ctx, _key );
    PomMapBucket * bucket = &_ctx->buckets[ idx ];
    PomMapNode * node = bucket->listHead;
    while( node ){
        if( strcmp( _key, node->key ) == 0 ){
            return node->value;
            break;
        }
        node = node->next;
    }
    return _default;
}

// Set a key to a given value
const char* pomMapSet( PomMapCtx *_ctx, const char * _key, const char * _value ){
    uint32_t idx = pomMapHashFunc( _ctx, _key );
    PomMapBucket * bucket = &_ctx->buckets[ idx ];
    PomMapNode * node = bucket->listHead;
    PomMapNode *pNode = NULL;
    while( node ){
        if( strcmp( _key, node->key ) == 0 ){
            break;
        }
        pNode = node;
        node = node->next;
    }
    if( node ){
        // Found a node
        node->value = _value;
    }
    else{
        // Add a new node
        node = (PomMapNode*) malloc( sizeof( PomMapNode ) );
        node->key = _key;
        node->value = _value;
        _ctx->numNodes++;
        if( pNode == NULL ){
            // First node in bucket
            bucket->listHead = node;
        }
        else{
            // Otherwise add to linked list
            pNode->next = node;
        }
    }

    if( _ctx->numNodes > _ctx->numBuckets ){
        // Resize the bucket array
        uint32_t newSize = _ctx->numBuckets << 1;
        pomMapResize( _ctx, newSize );
    }
    
    return _value;
}

// Get a key if it exists, otherwise add a new node with value `_default`
const char* pomMapGetSet( PomMapCtx *_ctx, const char * _key, const char * _default ){
    uint32_t idx = pomMapHashFunc( _ctx, _key );
    PomMapBucket * bucket = &_ctx->buckets[ idx ];
    PomMapNode * node = bucket->listHead;
    PomMapNode *pNode = NULL;
    while( node ){
        if( strcmp( _key, node->key ) == 0 ){
            break;
        }
        pNode = node;
        node = node->next;
    }
    if( node ){
        // Found a node
        return node->value;
    }
    else{
        // Add a new node
        node = (PomMapNode*) malloc( sizeof( PomMapNode ) );
        node->key = _key;
        node->value = _default;
        _ctx->numNodes++;
        if( pNode == NULL ){
            // First node in bucket
            bucket->listHead = node;
        }
        else{
            // Otherwise add to linked list
            pNode->next = node;
        }
    }
    
    return _default;
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