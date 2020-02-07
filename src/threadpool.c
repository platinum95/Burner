#include "threadpool.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>

struct PomThreadpoolThreadCtx{
    uint16_t tId;
    _Atomic bool busy;
    _Atomic bool shouldLive, isLive;
    PomHpLocalCtx *hplctx;
    thrd_t tCtx;
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
    PomQueueCtx *jobQueue = (PomQueueCtx*) malloc( sizeof( PomQueueCtx ) );
    atomic_store( &_ctx->jobQueue, jobQueue );

    pomQueueInit( _ctx->jobQueue, sizeof( PomThreadpoolJob ) );
    _ctx->hpgctx = (PomHpGlobalCtx*) malloc( sizeof( PomHpGlobalCtx ) );
    pomHpGlobalInit( _ctx->hpgctx );
    cnd_init( &_ctx->tWaitCond );
    cnd_init( &_ctx->tJoinCond );
    for( int tId = 0; tId < _numThreads+1; tId++ ){
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        currThread->hplctx = (PomHpLocalCtx*) malloc( sizeof( PomHpLocalCtx ) );
        pomHpThreadInit( _ctx->hpgctx, currThread->hplctx, 2 );
    }
    for( int i = 0; i < _numThreads; i++ ){
        int tId = i+1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        atomic_init( &currThread->busy, false );
        atomic_init( &currThread->shouldLive, true );
        currThread->tId = tId;
        PomThreadHouseArg * hArg = (PomThreadHouseArg*) malloc( sizeof( PomThreadHouseArg ) );
        hArg->ctx = _ctx;
        hArg->tctx = currThread;
        thrd_create( &currThread->tCtx, threadHouse, hArg );
    }
    return 0;
}

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, void(*_func)(void*), void *_args ){
    PomThreadpoolJob *jobData = (PomThreadpoolJob*) malloc( sizeof( PomThreadpoolJob ) );
    jobData->func = _func;
    jobData->args = _args;
    pomQueuePush( atomic_load( &_ctx->jobQueue ), _ctx->hpgctx, _ctx->threadData[ 0 ].hplctx, jobData );
    cnd_signal( &_ctx->tWaitCond );
    return 0;
}

// Where the thread lives
int threadHouse( void *_arg ){
    PomThreadHouseArg * arg = (PomThreadHouseArg*) _arg;
    // Show we're busy while we set up
    atomic_store( &arg->tctx->busy, true );
    atomic_init( &arg->tctx->isLive, true );

    PomThreadpoolCtx *ctx = arg->ctx;
    PomThreadpoolThreadCtx *tctx = arg->tctx;
    free( _arg );
    PomQueueCtx *jobQueue = atomic_load( &ctx->jobQueue );
    atomic_store( &tctx->busy, false );
    mtx_t waitMtx;
    mtx_init( &waitMtx, mtx_plain );
    
    while( atomic_load( &tctx->shouldLive ) ){

        if( pomQueueLength( jobQueue ) == 0 ){
            // Tell main thread (if waiting) that we're sleeping
            cnd_signal( &ctx->tJoinCond );
            cnd_wait( &ctx->tWaitCond, &waitMtx );
        }

        PomThreadpoolJob *job = pomQueuePop( jobQueue, ctx->hpgctx, tctx->hplctx );
        if( job ){
            atomic_store( &tctx->busy, true );
            // We have a job to execute
            job->func( job->args );
            atomic_store( &tctx->busy, false );
            free( job );
        }


    }
    mtx_destroy( &waitMtx );
    atomic_store( &tctx->isLive, false );
    return 0;
}

// Block the calling thread until the jobqueue is empty.
int pomThreadpoolJoinAll( PomThreadpoolCtx *_ctx ){
    mtx_t wMtx;
    mtx_init( &wMtx, mtx_plain );

    // Wait for the current jobqueue to clear and tasks to finish
    while( pomQueueLength( atomic_load( &_ctx->jobQueue ) ) );
    for( int i = 0; i < _ctx->numThreads; i++ ){
        int tId = i + 1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        while( atomic_load( &currThread->busy ) ){
            cnd_wait( &_ctx->tJoinCond, &wMtx );
        }
    }
    mtx_destroy( &wMtx );
    
    return 0;
}

#include <stdio.h>

int pomThreadpoolClear( PomThreadpoolCtx *_ctx ){
    // Tell all threads to exit
    for( int i = 0; i < _ctx->numThreads; i++ ){
        int tId = i + 1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        atomic_store( &currThread->shouldLive, false );
    }

    // Wait for all the threads to finish their jobs
    pomThreadpoolJoinAll( _ctx );

    // Any waiting threads should now continue and exit
    // but we need to be sure they're dead before continuing
    int startIdx = 0;
    int freeCnt = 0, allocCnt = 0;
    PomThreadpoolThreadCtx *headThread = &_ctx->threadData[ 0 ];
    pomHpThreadClear( _ctx->hpgctx, headThread->hplctx );
    

    freeCnt += atomic_load( &headThread->hplctx->freeCntr );
    allocCnt += atomic_load( &headThread->hplctx->allocCntr );
    printf( "Thread %i: alloc %i, free %i\n", 0, allocCnt, freeCnt );
    free( headThread->hplctx );
    // Wait for all the threads to exit, then free their data
    for( int i = startIdx; i < _ctx->numThreads; i++ ){
        int tId = i + 1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        while( atomic_load( &currThread->isLive ) ){
            // Keep broadcasting on the offchance the thread is
            // stuck waiting
            cnd_broadcast( &_ctx->tWaitCond );
        }
        // Block till the thread dies
        thrd_join( currThread->tCtx, NULL );

        // Free the thread data
        pomHpThreadClear( _ctx->hpgctx, currThread->hplctx );
        int tAlloc = atomic_load( &currThread->hplctx->allocCntr );
        int tFree = atomic_load( &currThread->hplctx->freeCntr );

        freeCnt += tFree;
        allocCnt += tAlloc;
        printf( "Thread %i: alloc %i, free %i\n", tId, tAlloc, tFree );
        free( currThread->hplctx );
    }

    printf( "Allocated %i, freed %i\n", allocCnt, freeCnt );

    // Clear global hazard pointer and queue data
    pomQueueClear( _ctx->jobQueue, _ctx->hpgctx );
    pomHpGlobalClear( _ctx->hpgctx );

    // Free threadpool pointers
    free( _ctx->hpgctx );
    free( _ctx->jobQueue );
    free( _ctx->threadData );

    return 0;
}