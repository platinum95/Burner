#include <stddef.h>
#include "hazard_ptr.h"

typedef struct PomQueueNode PomQueueNode;
typedef struct PomQueueCtx PomQueueCtx;

struct PomQueueNode {
    void * data;
    PomQueueNode * _Atomic next;
};

struct PomQueueCtx{
    PomQueueNode * _Atomic head;
    PomQueueNode * _Atomic tail;
};

// Initialise the thread-safe queue
int pomQueueInit( PomQueueCtx *_ctx, size_t _dataLen );

// Add an item to the queue
int pomQueuePush( PomQueueCtx *_ctx, PomHpLocalCtx *_hplctx, void * _data );

// Pop an item from the queue
void * pomQueuePop( PomQueueCtx *_ctx, PomHpGlobalCtx *_hpctx, PomHpLocalCtx *_hplctx );

// Clean up the queue
int pomQueueClear( PomQueueCtx *_ctx );