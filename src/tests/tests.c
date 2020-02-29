#include <stdio.h>
#include "hashmap.h"
#include "common.h"
#include "config.h"
#include "tests.h"
#include "queue.h"
#include <stdlib.h>
#include "threadpool.h"
#include <time.h>

// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

#define LOG( log, ... ) LOG_MODULE( DEBUG, tests, log, ##__VA_ARGS__ )

void testHashmap();
void testConfig();
void testQueues();
void testThreadpool();

// Equivalent to b-a
void timeDiff( struct timespec *a, struct timespec *b, struct timespec *out ){
    int64_t secDiff = b->tv_sec - a->tv_sec;
    int64_t nsDiff = b->tv_nsec - a->tv_nsec;
    if( nsDiff < 0 ){
        nsDiff = 1e9 - nsDiff;
        secDiff--;
    }
    out->tv_nsec = nsDiff;
    out->tv_sec = secDiff;
}

double concatTime( struct timespec *a ){
    double sec = (double) a->tv_sec;
    double nsec = (double) a->tv_nsec;
    nsec = nsec / (double) 1e9;
    return sec + nsec;
}

int getTime( struct timespec *_t ){
    return clock_gettime( CLOCK_PROCESS_CPUTIME_ID, _t );
}

int main(){
//    testHashmap();
//    testConfig();
//    testQueues();
    testThreadpool();
    return 0;
}

void testConfig(){
    if( loadSystemConfig( "./config.ini" ) ){
       // printf( "Error loading configuration file\n" );
    }
    const char * testPath = pomMapGet( &systemConfig.mapCtx, "modelBasePath", "test" );
    if( testPath ){
        LOG( "Model base path: %s", testPath );
    }

    pomMapSet( &systemConfig.mapCtx, "Delete", "Me" );
    if( pomMapRemove( &systemConfig.mapCtx, "Delete" ) ){
        LOG( "Could not remove node" );
    }

    saveSystemConfig( "./testSave.ini" );
    
    clearSystemConfig();
}

void testHashmap(){
    PomMapCtx hashMapCtx;
    pomMapInit( &hashMapCtx, 0 );

    pomMapSet( &hashMapCtx, "Test", "Value" );

    const char * str = pomMapGet( &hashMapCtx, "Test", NULL );

    LOG( "%s", str );

    pomMapClear( &hashMapCtx );
    return;

}

void testQueues(){
    LOG( "Testing queues" );
    PomQueueCtx *queueCtx = (PomQueueCtx*) malloc( sizeof( PomQueueCtx ) );
    pomQueueInit( queueCtx );

    PomHpGlobalCtx *hpgctx = (PomHpGlobalCtx*) malloc( sizeof( PomHpGlobalCtx ) );
    PomHpLocalCtx *hplctx = (PomHpLocalCtx*) malloc( sizeof( PomHpLocalCtx ) );

    pomHpGlobalInit( hpgctx );
    pomHpThreadInit( hpgctx, hplctx, 2 );

    void * pushVal = (void*) 12345;
    pomQueuePush( queueCtx, hpgctx, hplctx, pushVal );
    void * val = pomQueuePop( queueCtx, hpgctx, hplctx );
    if( val != pushVal ){
        LOG( "Queue didn't pop same value as was pushed" );
    }
    else{
        LOG( "Queue popped same value as was pushed" );
    }
}

void testThreadFuncSanity( void* UNUSED( _data ) ){
    // Do some work here. Not using thrd_sleep since
    // it makes profiling a bit more difficult
    for( int i = 0; i < 1e3; i++ ){
        int UNUSED( a ) = i + 10;
    }
}

void testThreadFuncTiming( void* _data ){
    struct timespec *t = (struct timespec*) _data;
    getTime( t );
}

void threadpoolSched( int numIter, PomThreadpoolCtx *_ctx, PomThreadpoolJob *job ){
    for( int i = 0; i < numIter; i++ ){
        pomThreadpoolScheduleJob( _ctx, job );
        //thrd_sleep( &(struct timespec){.tv_sec=0, .tv_nsec=1}, NULL );
    }
}

struct timespec threadpoolProfile( void *data ){
    uint8_t numThreads = 3;
    uint32_t sanityNumIter = 1e4;
    uint32_t timingNumIter = 1e4;
    struct timespec sjStart, sjEnd, sjTime;
    PomThreadpoolCtx *ctx = (PomThreadpoolCtx*) malloc( sizeof( PomThreadpoolCtx ) );
    pomThreadpoolInit( ctx, numThreads );
    PomThreadpoolJob sanityJob = {
        .func= testThreadFuncSanity,
        .args = data
    };
    struct timespec eTime;
    PomThreadpoolJob timingJob = {
        .func= testThreadFuncTiming,
        .args = &eTime
    };
    
    // Run sanity check
    getTime( &sjStart );
    threadpoolSched( sanityNumIter, ctx, &sanityJob );
    pomThreadpoolJoinAll( ctx );
    getTime( &sjEnd );
    timeDiff( &sjStart, &sjEnd, &sjTime );

    double timingAccumulator = 0.0;
    struct timespec sTime;
    // Run timing check
    for( uint32_t i = 0; i < timingNumIter; i++ ){
        getTime( &sTime );
        pomThreadpoolScheduleJob( ctx, &timingJob );
        thrd_sleep( &(struct timespec){.tv_sec=0, .tv_nsec=15e3}, NULL );
        pomThreadpoolJoinAll( ctx );
        struct timespec tDiff;
        timeDiff( &sTime, &eTime, &tDiff );
        double jTimeTaken = concatTime( &tDiff );
        timingAccumulator += jTimeTaken;
    }
    timingAccumulator = timingAccumulator / timingNumIter;
    LOG( "Average time to start job %1.6fus", timingAccumulator * 1e6 );

    timingAccumulator = 0.0;
    // Run timing check for direct call
    for( uint32_t i = 0; i < timingNumIter; i++ ){
        getTime( &sTime );
        testThreadFuncTiming( &eTime );
        struct timespec tDiff;
        timeDiff( &sTime, &eTime, &tDiff );
        double jTimeTaken = concatTime( &tDiff );
        timingAccumulator += jTimeTaken;
    }
    timingAccumulator = timingAccumulator / timingNumIter;
    LOG( "Average time to start direct call %1.6fus", timingAccumulator * 1e6 );

    pomThreadpoolClear( ctx );
    free( ctx );

    return sjTime;
}

void seqProfile( int numIter, void * data ){
    for( int i = 0; i < numIter; i++){
        testThreadFuncSanity( data );
    }
}


void testThreadpool(){
    // allocs (at worst) = 31 + (numIter * 4)
    struct timespec tpStart, tpEnd, seqStart, seqEnd, tpTime, seqTime, sjTime;
    _Atomic int var = 0;
    //int var = 0;
    getTime( &tpStart );
    sjTime = threadpoolProfile( &var );
    getTime( &tpEnd );
    LOG( "Finished threadpool profile" );
    atomic_store( &var, 0 );
    getTime( &seqStart );
  //  seqProfile( numIter, &var );
    getTime( &seqEnd );

    timeDiff( &tpStart, &tpEnd, &tpTime );
    timeDiff( &seqStart, &seqEnd, &seqTime );

    double tpTimeMs = concatTime( &tpTime );
    double seqTimeMs = concatTime( &seqTime );
    double sjTimeMs = concatTime( &sjTime );
    
    //float tpTimeMs = ( (float) tPoolTime / (float) CLOCKS_PER_SEC ) * 1e3;
    //float seqTimeMs = ( (float) seqTime / (float) CLOCKS_PER_SEC ) * 1e3;
    float tpToSeqRatio = tpTimeMs / seqTimeMs ;
    LOG( "Threaded time: %f. Seq time %f. Tp is %f times slower or %f time faster.", tpTimeMs, seqTimeMs, tpToSeqRatio, 1/tpToSeqRatio );
    LOG( "SJ Time %f", sjTimeMs );
}

