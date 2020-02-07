#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdint.h>
#include "queue.h"
#include "threads.h"

typedef struct PomThreadpoolCtx PomThreadpoolCtx;
typedef struct PomThreadpoolThreadCtx PomThreadpoolThreadCtx;

struct PomThreadpoolCtx{
    uint16_t numThreads;
    PomQueueCtx * _Atomic jobQueue;
    PomThreadpoolThreadCtx *threadData;
    PomHpGlobalCtx *hpgctx;
    cnd_t tWaitCond, tJoinCond;
};

// Initialise the threadpool
int pomThreadpoolInit( PomThreadpoolCtx *_ctx, uint16_t _numThreads );

// Block the calling thread until the current jobqueue is empty.
int pomThreadpoolJoinAll( PomThreadpoolCtx *_ctx );

int pomThreadpoolClear( PomThreadpoolCtx *_ctx );

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, void(*func)(void*), void *_args );

#endif // THREADPOOL_H