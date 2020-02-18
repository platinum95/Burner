#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "system_hw.h"
#include "hashmap.h"
#include "common.h"

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, main, log, ##__VA_ARGS__ )

// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

int main( int argc, char ** argv ){
    LOG( "Program Entry" );

    char * configPath;
    // Single argument pointing to config file,
    // or no argument for default
    if( argc == 2 ){
        configPath = argv[ 1 ];
    }
    else{
        configPath = DEFAULT_CONFIG_PATH;
    }
    LOG( "Config file: %s", configPath );

/*
    SystemConfig sysConfig;
    if( loadSystemConfig( configPath, &sysConfig ) ){
        printf( "Error loading configuration file\n" );
    }

    const char * testPath = getConfigValue( &sysConfig, "modelBasePath", "test" )->value;
    if( testPath ){
        printf( "Model base path: %s\n", testPath );
    }
    
    clearSystemConfig( &sysConfig );
*/
    LOG( "Program exit" );
    return EXIT_SUCCESS;
}