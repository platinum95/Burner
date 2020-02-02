#include <stdio.h>
#include "hashmap.h"
#include "common.h"
#include "config.h"


// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

#define LOG( log, ... ) LOG_MODULE( DEBUG, tests, log, ##__VA_ARGS__ )

void testHashmap();
void testConfig();

int testModules(){
    testHashmap();
    testConfig();
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

    pomMapSet( &sysConfig.mapCtx, "TestAdd", "NewVal" );

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

