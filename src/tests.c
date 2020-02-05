#include <stdio.h>
#include "hashmap.h"
#include "common.h"
#include "config.h"
#include "tests.h"
#include "queue.h"
#include <stdlib.h>
#include "threadpool.h"

// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

#define LOG( log, ... ) LOG_MODULE( DEBUG, tests, log, ##__VA_ARGS__ )

void testHashmap();
void testConfig();
void testQueues();
void testThreadpool();

int testModules(){
//    testHashmap();
//    testConfig();
    testQueues();
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
    pomQueuePush( queueCtx, hplctx, pushVal );
    void * val = pomQueuePop( queueCtx, hpgctx, hplctx );
    if( val != pushVal ){
        LOG( "Queue didn't pop same value as was pushed" );
    }
    else{
        LOG( "Queue popped same value as was pushed" );
    }
}

void testThreadFunc( void* _data ){
    _Atomic int *var = (_Atomic int *) _data;
    atomic_fetch_add( var, 1 );
    //int *var  = (int*) _data;
    //(*var)++;
}

void testThreadpool(){
    const int numIter = 1000;
    _Atomic int var = 0;
    //int var = 0;
    PomThreadpoolCtx *ctx = (PomThreadpoolCtx*) malloc( sizeof( PomThreadpoolCtx ) );
    pomThreadpoolInit( ctx, 4 );
    for( int i = 0; i < numIter; i++ ){
        LOG( "Start thread %u", i );
        pomThreadpoolScheduleJob( ctx, testThreadFunc, &var );
    }
    pomThreadpoolJoinAll( ctx );
    int newVar = atomic_load( &var );
    if( atomic_load( &var ) != numIter ){
        LOG( "Thread test failed, var = %u, iter = %u", newVar, numIter );
    }
    else{
        LOG( "Thread test succeeded" );
    }
}

