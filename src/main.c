#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "system_hw.h"
#include "hashmap.h"

// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

int main( int argc, char ** argv ){
    printf( "Program Entry\n" );

    char * configPath;
    // Single argument pointing to config file,
    // or no argument for default
    if( argc == 2 ){
        configPath = argv[ 1 ];
    }
    else{
        configPath = DEFAULT_CONFIG_PATH;
    }
    printf( "Config file: %s\n", configPath );
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
    printf( "Program exit\n" );
    return EXIT_SUCCESS;
}