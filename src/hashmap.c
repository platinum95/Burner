
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "common.h"

//#define LOG( log, ... ) LOG_MODULE( DEBUG, hashmap, log, ##__VA_ARGS__ )
#define LOG( log, ... )

#define POM_MAP_DEFAULT_SIZE 32 // Default number of buckets in list
#define POM_MAP_HEAP_SIZE 256   // Default size of the data heaps

typedef struct PomMapNode{
    //const char * key;
    //const char * value;
    size_t keyOffset;
    size_t valueOffset;
    struct PomMapNode * next;
}PomMapNode;

struct PomMapBucket{
    PomMapNode * listHead;
};

struct PomMapDataHeap{
    char * heap;
    uint32_t numHeapBlocks;
    size_t heapUsed;
    size_t fragmentedData;
};

inline uint32_t pomNextPwrTwo( uint32_t _size );
inline uint32_t pomMapHashFunc( PomMapCtx *_ctx, const char * key );
inline const char * pomMapGetNodeKey( PomMapCtx * _ctx, PomMapNode * _node );
inline const char * pomMapGetNodeValue( PomMapCtx * _ctx, PomMapNode * _node );

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

const char * pomMapGetNodeKey( PomMapCtx * _ctx, PomMapNode * _node ){
    return (const char *) (_ctx->dataHeap->heap + _node->keyOffset);
}

const char * pomMapGetNodeValue( PomMapCtx * _ctx, PomMapNode * _node ){
    return (const char *) (_ctx->dataHeap->heap + _node->valueOffset);
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
    _ctx->dataHeap->fragmentedData = 0;
    _ctx->initialised = true;
    _ctx->numBuckets = _size;
    _ctx->numNodes = 0;
    LOG( "Map initialised with heap size %i", 1 );

    return 0;
}


int pomMapAddData( PomMapCtx *_ctx, const char **_key, const char **_value ){
    // TODO add error handling in this function

    size_t curSize = _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE;
    size_t curUsed = _ctx->dataHeap->heapUsed;
    size_t keyLen = strlen( *_key ) + 1;
    size_t valLen = strlen( *_value ) + 1;
    size_t newDataLen = keyLen + valLen;
    size_t newHeapUsed = curUsed + newDataLen;

    //LOG( "Adding data %s:%s to heap", *_key, *_value );

    if( newHeapUsed > curSize ){
        // Check if it's worth optimising to remove fragmented data
        if( _ctx->dataHeap->fragmentedData >= newDataLen ){
            LOG( "New pair can fit in fragmented data, so optimising" );
            pomMapOptimise( _ctx );
            return pomMapAddData( _ctx, _key, _value );
        }
        // Increase block size
        _ctx->dataHeap->numHeapBlocks++;
        size_t newHeapSize = _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE;
        LOG( "Increasing heap size to %zu", newHeapSize );
        // TODO check for error on realloc
        _ctx->dataHeap->heap = realloc( _ctx->dataHeap->heap, newHeapSize );
        // Call recursively in case the new key/value pair exceeds block size
        // TODO change this from recursive to just checking for that here
         return pomMapAddData( _ctx, _key, _value );
        
        // Data is all in heap, but address may have changed so node key/value pointers are invalid. Need to update.
        // (or zero the memory and re-allocate everything)
    }
    // From here we have enough space in the heap for the new data
    char * keyLoc = _ctx->dataHeap->heap + curUsed;
    memcpy( keyLoc, *_key, keyLen );
    curUsed += keyLen;
    char * valLoc = _ctx->dataHeap->heap + curUsed;
    memcpy( valLoc, *_value, valLen );
    _ctx->dataHeap->heapUsed = curUsed + valLen;

    // Return the new data locations in the original params
    *_key = keyLoc;
    *_value = valLoc;

    return 0;
}

PomMapNode * pomMapCreateNode( PomMapCtx *_ctx, const char *_key, const char *_value ){
    pomMapAddData( _ctx, &_key, &_value );
    // Need to get the key/value offsets into the heap
    size_t keyOffset = _key - _ctx->dataHeap->heap;
    size_t valueOffset = _value - _ctx->dataHeap->heap;

    // Now create the new node
    PomMapNode * newNode = (PomMapNode*) malloc( sizeof( PomMapNode ) );
    //newNode->key = _key;
    //newNode->value = _value;
    newNode->keyOffset = keyOffset;
    newNode->valueOffset = valueOffset;
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
        const char * nodeKeyStr = pomMapGetNodeKey( _ctx, *node );
        if( strcmp( _key, nodeKeyStr ) == 0 ){
            return node;
            break;
        }
        node = &(*node)->next;
    }
    return node;
}

// Get a value if it exists, return `_default` otherwise
const char* pomMapGet( PomMapCtx *_ctx, const char * _key, const char * _default ){

    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( !*node ){
        // Node was not found
        return _default;
    }
    // Node was found, return its value
    return pomMapGetNodeValue( _ctx, *node );
}

// Set a key to a given value
const char* pomMapSet( PomMapCtx *_ctx, const char * _key, const char * _value ){
    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( *node ){
        // Node exists, so set data
        LOG( "Setting existing key %s", _key );
        // Get node's current memory footprint and add it to fragmented data record
        size_t currKeyLen = strlen( pomMapGetNodeKey( _ctx, *node ) ) + 1;
        size_t currValLen = strlen( pomMapGetNodeValue( _ctx, *node ) ) + 1;
        _ctx->dataHeap->fragmentedData += currKeyLen + currValLen;

        // Add new key/value pair to list
        pomMapAddData( _ctx, &_key, &_value );
        size_t keyOffset = _key - _ctx->dataHeap->heap;
        size_t valueOffset = _value - _ctx->dataHeap->heap;
        (*node)->keyOffset = keyOffset;
        (*node)->valueOffset = valueOffset;
        //(*node)->key = _key;
        //(*node)->value = _value;
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
    return pomMapGetNodeValue( _ctx, newNode );
}

// Get a key if it exists, otherwise add a new node with value `_default`
const char* pomMapGetSet( PomMapCtx *_ctx, const char * _key, const char * _default ){
    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( *node ){
        // Node exists, so return data
        return pomMapGetNodeValue( _ctx, *node );
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
    return pomMapGetNodeValue( _ctx, newNode );
}


// Remove a key
int pomMapRemove( PomMapCtx *_ctx, const char * _key ){
    PomMapNode ** node = pomMapGetNodeRef( _ctx, _key );
    if( !*node ){
        // Node doesn't exist so exit
        return 1;
    }
    LOG( "Removing node %s", _key );
    size_t currKeyLen = strlen( pomMapGetNodeKey( _ctx, *node ) ) + 1;
    size_t currValLen = strlen( pomMapGetNodeValue( _ctx, *node ) ) + 1;
    _ctx->dataHeap->fragmentedData += currKeyLen + currValLen;
    PomMapNode **nodeToDel = node;
    *node = (*node)->next;
    free( *nodeToDel );
    return 0;
}

// TODO - keep track of unnecessary strings (from removed/updated key/values),
// then when requiring a resize of the heap check if its useful to also optimise
// the heap to take out the old strings (i.e. if you'd free more than 10% of the heap)


// Clean up the map
int pomMapClear( PomMapCtx *_ctx ){
    LOG( "Clearing hashmap" );
    uint32_t freeCnt = 0;
    for( uint32_t i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * curNode = curBucket->listHead;
        PomMapNode * nextNode = NULL;
        while( curNode ){
            nextNode = curNode->next;
            free( curNode );
            curNode = nextNode;
            freeCnt++;
        }
    }
    free( _ctx->buckets );
    LOG( "Cleared %i buckets and %i nodes", _ctx->numBuckets, freeCnt );
    if( freeCnt != _ctx->numNodes ){
        LOG( "Number of freed nodes (%i) not equal to number of recorded nodes (%i)", freeCnt, _ctx->numNodes );
    }
    free( _ctx->dataHeap->heap );
    LOG( "Freed data heap of size %i", _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE );
    free( _ctx->dataHeap );
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
        LOG( "Not resizing" );
        return 1;
    }

    // Round `_size` to a power of 2
    _size = pomNextPwrTwo( _size );
    LOG( "Resizing map to %i", _size );

    // Construct long linked list from all nodes
    PomMapNode * nodeHead = NULL;
    PomMapNode * lastNode = NULL;
    uint32_t nodesCounted = 0;
    for( uint32_t i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * nodeIter = curBucket->listHead;
        if( !nodeIter ){
            // Empty bucket
            continue;
        }
        if( !nodeHead ){
            // First node in the big list
            nodeHead = nodeIter;
            nodesCounted++;
        }
        else{
            // Connect up first node in this bucket to last node in last bucket
            lastNode->next = nodeIter;
            nodesCounted++;
        }
        while( nodeIter->next ){
            // Cycle through to find the last node in this bucket
            nodeIter = nodeIter->next;
            nodesCounted++;
        }
        lastNode = nodeIter;
    }
    if( nodesCounted != _ctx->numNodes ){
        LOG( "Counted %i nodes, but have %i on record", nodesCounted, _ctx->numNodes );
    }
    // Now reallocate the bucket array with the new size and zero it
    _ctx->buckets = (PomMapBucket*) realloc( _ctx->buckets, sizeof( PomMapBucket ) * _size );
    memset( _ctx->buckets, 0, sizeof( PomMapBucket ) *_size );
    _ctx->numBuckets = _size;

    // Add all nodes from linked list to new bucket array
    PomMapNode * curNode = nodeHead;
    while( curNode ){
        PomMapNode * nextNode = curNode->next;
        // Break apart the list as we go
        curNode->next = NULL;
        const char * currNodeKey = pomMapGetNodeKey( _ctx, curNode );
        //uint32_t idx = pomMapHashFunc( _ctx, curNode->key );
        uint32_t idx = pomMapHashFunc( _ctx, currNodeKey );
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


int pomMapOptimise( PomMapCtx *_ctx ){
    // Count the required bytes of all nodes
    size_t totalBytesReq = 0;
    for( uint32_t i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * nodeIter = curBucket->listHead;
        while( nodeIter ){
            const char * key = pomMapGetNodeKey( _ctx, nodeIter );
            const char * val = pomMapGetNodeValue( _ctx, nodeIter );
            size_t keyLen = strlen( key ) + 1;
            size_t valLen = strlen( val ) + 1;
            totalBytesReq += keyLen + valLen;
            nodeIter = nodeIter->next;
        }
    }
    
    // Check if we can downsize the heap
    // Rounding up by x/y -> (x+y-1)/y
    uint32_t blocksReq = ( totalBytesReq + POM_MAP_HEAP_SIZE - 1 ) / POM_MAP_HEAP_SIZE;
    size_t newHeapSize = blocksReq * POM_MAP_HEAP_SIZE;
    LOG( "Current heap size: %zu. Total bytes required %zu",
         _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE,
         newHeapSize );
    // Create new heap to copy data into
    char * newHeap = (char*) malloc( sizeof( char ) * newHeapSize );
    size_t currOffset = 0;
    LOG( "Reordering hashmap" );
    // Copy the data to the new buffer and update the key/value pointers
    for( uint32_t i = 0; i < _ctx->numBuckets; i++ ){
        PomMapBucket * curBucket = &_ctx->buckets[ i ];
        PomMapNode * nodeIter = curBucket->listHead;
        while( nodeIter ){
            const char * key = pomMapGetNodeKey( _ctx, nodeIter );
            const char * val = pomMapGetNodeValue( _ctx, nodeIter );
            size_t keyLen = strlen( key ) + 1;
            size_t valLen = strlen( val ) + 1;
            char * newKey = newHeap + currOffset;
            char * newVal = newKey + keyLen;

            memcpy( newKey, key, keyLen );
            memcpy( newVal, val, valLen );

            size_t keyOffset = newKey - newHeap;
            size_t valOffset = newVal - newHeap;
            nodeIter->keyOffset = keyOffset;
            nodeIter->valueOffset = valOffset;
            nodeIter = nodeIter->next;
            currOffset += keyLen + valLen;            
        }
    }
    _ctx->dataHeap->heapUsed = totalBytesReq;
    // Free up the old heap and update the context with the new heap
    free( _ctx->dataHeap->heap );
    _ctx->dataHeap->heap = newHeap;
    _ctx->dataHeap->numHeapBlocks = blocksReq;
    _ctx->dataHeap->fragmentedData = 0;

    return 0;
}

const char * pomMapGetDataHeap( PomMapCtx *_ctx, size_t *_heapUsed, size_t *_heapSize ){
    if( _heapUsed ){
        *_heapUsed = _ctx->dataHeap->heapUsed;
    }
    if( _heapSize ){
        *_heapSize = _ctx->dataHeap->numHeapBlocks * POM_MAP_HEAP_SIZE;
    }
    return _ctx->dataHeap->heap;
}