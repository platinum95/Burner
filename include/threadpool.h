#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdint.h>
#include "queue.h"

typedef struct PomThreadpoolCtx PomThreadpoolCtx;
typedef struct PomThreadpoolThreadCtx PomThreadpoolThreadCtx;

struct PomThreadpoolCtx{
    uint16_t numThreads;
    PomQueueCtx *jobQueue;
    PomThreadpoolThreadCtx *threadData;
    PomHpGlobalCtx *hpgctx;
};

int pomThreadpoolInit( PomThreadpoolCtx *_ctx, uint16_t _numThreads );

int pomThreadpoolJoinAll( PomThreadpoolCtx *_ctx );

int pomThreadpoolClear( PomThreadpoolCtx *_ctx );

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, void(*func)(void*), void *_args );

#endif // THREADPOOL_H