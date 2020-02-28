#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "system_hw.h"
#include "hashmap.h"
#include "common.h"
#include "vkinstance.h"
#include "vkdevice.h"
#include "vkpipeline.h"

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


    if( loadSystemConfig( configPath ) ){
        printf( "Error loading configuration file\n" );
    }

    LOG( "Create Vk instance" );
    if( pomCreateVkInstance() ){
        LOG( "Error in instance creation" );
    }
    LOG( "Find device" );
    if( pomPickPhysicalDevice() ){
        LOG( "Error in physical device creation" );
    }
    LOG( "Create logical device" );
    if( pomCreateLogicalDevice() ){
        LOG( "Failed to create logical device" );
    }

    LOG( "Create shaders" );
    ShaderInfo basicShaders = {
        .vertexShaderPath = "./res/shaders/basicV.vert.spv",
        .fragmentShaderPath = "./res/shaders/basicV.vert.spv"
    };
    if( pomShaderCreate( &basicShaders ) ){
        LOG( "Failed to create shaders" );
    }
    LOG( "Destroy shaders" );
    if( pomShaderDestroy( &basicShaders ) ){
        LOG( "Failed to destroy shaders" );
    }

    LOG( "Destroy logical device" );
    if( pomDestroyLogicalDevice() ){
        LOG( "Failed to create logical device" );
    }
    LOG( "Destroy device" )
    if( pomDestroyPhysicalDevice() ){
        LOG( "Error in destroying device" );
    }
    LOG( "Destroy Vk instance" );
    if( pomDestroyVkInstance() ){
        LOG( "Error in deletion of vk instance" );
    } 
    LOG( "Program exit" );

    if( saveSystemConfig( configPath ) ){
        LOG( "Couldn't write new config files" );
    }
    clearSystemConfig();
    return EXIT_SUCCESS;
}