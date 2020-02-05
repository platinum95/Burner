#include "threadpool.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>



struct PomThreadpoolThreadCtx{
    uint16_t tId;
    _Atomic bool busy;
    _Atomic bool live;
    PomHpLocalCtx *hplctx;
    thrd_t *tCtx;
    
};

typedef struct PomThreadpoolJob PomThreadpoolJob;

struct PomThreadpoolJob{
    void (*func)(void*);
    void *args;
};

typedef struct PomThreadHouseArg{
    PomThreadpoolCtx * ctx;
    PomThreadpoolThreadCtx *tctx;
}PomThreadHouseArg;

// Where the thread lives
int threadHouse( void *_arg );

int pomThreadpoolInit( PomThreadpoolCtx *_ctx, uint16_t _numThreads ){
    _ctx->numThreads = _numThreads;
    _ctx->threadData = (PomThreadpoolThreadCtx*) malloc( sizeof( PomThreadpoolThreadCtx ) * ( _numThreads + 1 ) );
    _ctx->jobQueue = (PomQueueCtx*) malloc( sizeof( PomQueueCtx ) );
    pomQueueInit( _ctx->jobQueue, sizeof( PomThreadpoolJob ) );
    pomHpGlobalInit( _ctx->hpgctx );
    for( int tId = 0; tId < _numThreads+1; tId++ ){
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        currThread->hplctx = (PomHpLocalCtx*) malloc( sizeof( PomHpLocalCtx ) );
        pomHpThreadInit( _ctx->hpgctx, currThread->hplctx, 2 );
        atomic_init( &currThread->busy, false );
        atomic_init( &currThread->live, true );
        currThread->tId = tId;
        currThread->tCtx = (thrd_t*) malloc( sizeof( thrd_t ) );
        PomThreadHouseArg * hArg = (PomThreadHouseArg*) malloc( sizeof( PomThreadHouseArg ) );
        hArg->ctx = _ctx;
        hArg->tctx = currThread;
        thrd_create( currThread->tCtx, threadHouse, hArg );
        // TODO - handle errors
    }
    return 0;
}

int pomThreadpoolClear( PomThreadpoolCtx *_ctx ){
    // TODO - Implement this
    return 0;
}

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, void(*_func)(void*), void *_args ){
    PomThreadpoolJob *jobData = (PomThreadpoolJob*) malloc( sizeof( PomThreadpoolJob ) );
    jobData->func = _func;
    jobData->args = _args;
    pomQueuePush( _ctx->jobQueue, _ctx->threadData[ 0 ].hplctx, jobData );
    return 0;
}

// Where the thread lives
int threadHouse( void *_arg ){
    PomThreadHouseArg * arg = (PomThreadHouseArg*) _arg;
    // Show we're busy while we set up
    atomic_store( &arg->tctx->busy, true );

    PomThreadpoolCtx *ctx = arg->ctx;
    PomThreadpoolThreadCtx *tctx = arg->tctx;
    free( _arg );

    while( atomic_load( &tctx->live ) ){
        PomThreadpoolJob *job = pomQueuePop( ctx->jobQueue, ctx->hpgctx, tctx->hplctx );
        if( job ){
            atomic_store( &tctx->busy, true );
            // We have a job to execute
            job->func( job->args );

            atomic_store( &tctx->busy, false );
        }

        // TODO - implement a proper thread-sleep method (i.e. condition to wake on scheduleJob )
        struct timespec tsleep;
        tsleep.tv_sec = 0;
        tsleep.tv_nsec = 100;
        thrd_sleep( &tsleep, NULL );
    }
    return 0;
}


int pomThreadpoolJoinAll( PomThreadpoolCtx *_ctx ){
    // TODO - implement
    return 0;
}