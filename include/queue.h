#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include "hazard_ptr.h"
#include <stdint.h>


//typedef struct PomQueueNode PomQueueNode;
typedef struct PomQueueCtx PomQueueCtx;

/*
struct PomQueueNode {
    void * data;
    PomQueueNode * _Atomic next;
};
*/

struct PomQueueCtx{
    PomCommonNode * _Atomic head;
    PomCommonNode * _Atomic tail;
    _Atomic uint32_t queueLength;
};

// Initialise the thread-safe queue
int pomQueueInit( PomQueueCtx *_ctx );

// Add an item to the queue
int pomQueuePush( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpgctx, PomHpLocalCtx *_hplctx, void * _data );

// Pop an item from the queue
void * pomQueuePop( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx );

// Clean up the queue
int pomQueueClear( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx );

uint32_t pomQueueLength( PomQueueCtx *_ctx );

#endif // QUEUE_H