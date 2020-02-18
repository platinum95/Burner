#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdint.h>
#include "queue.h"
#if defined(__STDC_NO_THREADS__) || defined(_WIN32) || defined(__CYGWIN__)
#include "tinycthread.h"
#else
#include <threads.h>
#endif

typedef struct PomThreadpoolCtx PomThreadpoolCtx;
typedef struct PomThreadpoolThreadCtx PomThreadpoolThreadCtx;

typedef struct PomThreadpoolJob PomThreadpoolJob;

struct PomThreadpoolJob{
    void (*func)(void*);
    void *args;
};


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

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, PomThreadpoolJob *_job );

#endif // THREADPOOL_H