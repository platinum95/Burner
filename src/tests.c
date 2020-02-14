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

int testModules(){
//    testHashmap();
//    testConfig();
//    testQueues();
    testThreadpool();
    return 0;
}

void testConfig(){
    SystemConfig sysConfig;
    if( loadSystemConfig( "./config.ini", &sysConfig ) ){
        printf( "Error loading configuration file\n" );
    }

    const char * testPath = pomMapGet( &sysConfig.mapCtx, "modelBasePath", "test" );
    if( testPath ){
        LOG( "Model base path: %s", testPath );
    }

    pomMapSet( &sysConfig.mapCtx, "Delete", "Me" );
    if( pomMapRemove( &sysConfig.mapCtx, "Delete" ) ){
        LOG( "Could not remove node" );
    }

    saveSystemConfig( &sysConfig, "./testSave.ini" );
    
    clearSystemConfig( &sysConfig );
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
    pomQueueInit( queueCtx, sizeof( uint64_t ) );

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

void testThreadFunc( void* _data ){
    //_Atomic int *var = (_Atomic int *) _data;
    //atomic_fetch_add( var, 1 );

    thrd_sleep( &( struct timespec){.tv_sec=0, .tv_nsec=0}, NULL );
    //int *var  = (int*) _data;
    //(*var)++;
}

void threadpoolSched( int numIter, PomThreadpoolCtx *_ctx, PomThreadpoolJob *job ){
    for( int i = 0; i < numIter; i++ ){
        pomThreadpoolScheduleJob( _ctx, job );
        //thrd_sleep( &(struct timespec){.tv_sec=0, .tv_nsec=1}, NULL );
    }
}
struct timespec threadpoolProfile( int numIter, void *data ){
    struct timespec sjStart, sjEnd, sjTime;
    PomThreadpoolCtx *ctx = (PomThreadpoolCtx*) malloc( sizeof( PomThreadpoolCtx ) );
    pomThreadpoolInit( ctx, 4 );
    PomThreadpoolJob job = {
        .func= testThreadFunc,
        .args = data
    };
    
    getTime( &sjStart );
    
    threadpoolSched( numIter, ctx, &job );
    pomThreadpoolJoinAll( ctx );
    
    getTime( &sjEnd );
    timeDiff( &sjStart, &sjEnd, &sjTime );
    
    pomThreadpoolClear( ctx );
    free( ctx );

    return sjTime;
}

void seqProfile( int numIter, void * data ){
    for( int i = 0; i < numIter; i++){
        testThreadFunc( data );
    }
}


void testThreadpool(){
    // allocs (at worst) = 31 + (numIter * 4)
    struct timespec tpStart, tpEnd, seqStart, seqEnd, tpTime, seqTime, sjTime;
    const int numIter = 1e4;
    _Atomic int var = 0;
    //int var = 0;
    getTime( &tpStart );
    sjTime = threadpoolProfile( numIter, &var );
    getTime( &tpEnd );
    LOG( "Finished threadpool profile" );
    atomic_store( &var, 0 );
    getTime( &seqStart );
    seqProfile( numIter, &var );
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

