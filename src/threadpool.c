#include "threadpool.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined(__STDC_NO_THREADS__) || defined(_WIN32) || defined(__CYGWIN__)
#include "tinycthread.h"
#else
#include <threads.h>
#endif

struct PomThreadpoolThreadCtx{
    uint16_t tId;
    _Atomic bool busy;
    _Atomic bool shouldLive, isLive;
    PomHpLocalCtx *hplctx;
    thrd_t tCtx;
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

    pomQueueInit( _ctx->jobQueue );
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

int pomThreadpoolScheduleJob( PomThreadpoolCtx *_ctx, PomThreadpoolJob *_job ){
    pomQueuePush( atomic_load( &_ctx->jobQueue ), _ctx->hpgctx, _ctx->threadData[ 0 ].hplctx, _job );
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
            cnd_timedwait( &ctx->tWaitCond, &waitMtx, &(struct timespec){.tv_sec=0, .tv_nsec=500} );
        }

        PomThreadpoolJob *job = pomQueuePop( jobQueue, ctx->hpgctx, tctx->hplctx );
        if( job ){
            atomic_store( &tctx->busy, true );
            // We have a job to execute
            job->func( job->args );
            atomic_store( &tctx->busy, false );
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
    while( pomQueueLength( atomic_load( &_ctx->jobQueue ) ) ){
        // One of the worker threads should broadcast on this signal
        // when the queue is empty
        //cnd_timedwait( &_ctx->tJoinCond, &wMtx, &(struct timespec){.tv_sec=0, .tv_nsec=500} );
        PomThreadpoolJob *job = pomQueuePop( _ctx->jobQueue, _ctx->hpgctx, _ctx->threadData[ 0 ].hplctx );
        if( job ){
            // We have a job to execute
            job->func( job->args );
        }

    }
    // Make sure all the threads are idle before continuing
    for( int i = 0; i < _ctx->numThreads; i++ ){
        int tId = i + 1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        while( atomic_load( &currThread->busy ) ){
            cnd_timedwait( &_ctx->tJoinCond, &wMtx, &(struct timespec){.tv_sec=0, .tv_nsec=500} );
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

    // Wait for all the threads to exit, then free their data
    for( int i = startIdx; i < _ctx->numThreads; i++ ){
        int tId = i + 1;
        PomThreadpoolThreadCtx * currThread = &_ctx->threadData[ tId ];
        // TODO - Verify there's no error here. Valgrind gave an free'd-block access error
        // on this atomic_load
        while( atomic_load( &currThread->isLive ) ){
            // Keep broadcasting on the offchance the thread is
            // stuck waiting
            cnd_broadcast( &_ctx->tWaitCond );
        }
        // Block till the thread dies
        thrd_join( currThread->tCtx, NULL );

        // Free the thread data
        pomHpThreadClear( _ctx->hpgctx, currThread->hplctx );

        free( currThread->hplctx );
    }

    
    PomThreadpoolThreadCtx *headThread = &_ctx->threadData[ 0 ];

    // Clear reamining queue data to main threads retired list
    pomQueueClear( _ctx->jobQueue, _ctx->hpgctx, headThread->hplctx );
    
    // Now clear the main threads HP data
    pomHpThreadClear( _ctx->hpgctx, headThread->hplctx );\

    // Finally clear the HP data
    pomHpGlobalClear( _ctx->hpgctx );

    free( headThread->hplctx );

    // Free threadpool pointers
    free( _ctx->hpgctx );
    free( _ctx->jobQueue );
    free( _ctx->threadData );

    return 0;
}