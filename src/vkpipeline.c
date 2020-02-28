#include "vkpipeline.h"
#include "common.h"
#include "vkdevice.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define LOG( level, log, ... ) LOG_MODULE( level, vkpipeline, log, ##__VA_ARGS__ )

static uint32_t * pomLoadShaderData( const char * _path, size_t *_size ){
    FILE *shaderFile = fopen( _path, "rb" );
    if( !shaderFile ){
        LOG( ERR, "Could not open shader file" );
        return NULL;
    }

    fseek( shaderFile, 0, SEEK_END );
    size_t fSize = ftell( shaderFile );
    fseek( shaderFile, 0, SEEK_SET );

    if( !fSize ){
        LOG( WARN, "Shader file read as empty" );
        fclose( shaderFile );
        return NULL;
    }
    *_size = fSize;
    uint32_t * data = (uint32_t*) malloc( fSize );
    // Make sure we have a multiple of sizeof( uint32_t )
    if( fSize % sizeof( uint32_t ) != 0 ){
        LOG( WARN, "Shader file size not as expected. Shader compilation may fail" );
    }
    size_t numBlocks = fSize / sizeof( uint32_t );

    size_t nRead = fread( data, sizeof( uint32_t ), numBlocks, shaderFile );
    if( nRead != numBlocks ){
        LOG( WARN, "Shader file load did not read as expected. Compilation may fail" );
    }

    fclose( shaderFile );
    return data;
}

static int pomCreateShaderModule( VkShaderModule *_module, const char *_shaderPath, VkDevice *_dev ){
    size_t shaderSize;
    uint32_t *shaderData = pomLoadShaderData( _shaderPath, &shaderSize );
    if( !shaderData ){
        LOG( ERR, "Shader stage creation failed" );
        return 1;
    }

    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderSize,
        .pCode = shaderData
    };

    int err = 0;
    if( vkCreateShaderModule( *_dev, &moduleInfo, NULL, _module ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create shader module for %s", _shaderPath );
        err = 1;
    }

    free( shaderData );
    return err;
}

int pomShaderCreate( ShaderInfo *_shaderInfo ){
    if( !_shaderInfo->vertexShaderPath || !_shaderInfo->fragmentShaderPath ){
        LOG( ERR, "Shader requires vertex and fragment stages" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for shader creation" );
        return 1;
    }
    uint8_t currStage = 0;

    // Create vertex module
    VkShaderModule vertexModule;
    if( pomCreateShaderModule( &vertexModule, _shaderInfo->vertexShaderPath, dev ) ){
        LOG( ERR, "Could not create vertex module of shader" );
        return 1;
    }
    VkPipelineShaderStageCreateInfo *vertexStageInfo = &_shaderInfo->shaderStages[ currStage++ ];

    // Create fragment module
    VkShaderModule fragmentModule;
    if( pomCreateShaderModule( &fragmentModule, _shaderInfo->fragmentShaderPath, dev ) ){
        LOG( ERR, "Could not create fragment module of shader" );
        return 1;
    }
    VkPipelineShaderStageCreateInfo *fragmentStageInfo = &_shaderInfo->shaderStages[ currStage++ ];

    // Create vertex/fragment stages
    // Limit ourselves to `main` entry point for now
    // TODO - add support for specifying entry point
    *vertexStageInfo = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexModule,
        .pName = "main"
    };
    *fragmentStageInfo = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentModule,
        .pName = "main"
    };
    
    // Check if we should create the tesselation module/stage
    if( _shaderInfo->tesselationShaderPath ){
        VkShaderModule tessModule;
        if( pomCreateShaderModule( &tessModule, _shaderInfo->tesselationShaderPath, dev ) ){
            LOG( ERR, "Could not create tesselation module of shader" );
            return 1;
        }
        VkPipelineShaderStageCreateInfo *tessStageInfo = &_shaderInfo->shaderStages[ currStage++ ];
        *tessStageInfo = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            .module = tessModule,
            .pName = "main"
        };
    }

    // Now check if we should create the geometry module/stage
    // Check if we should create the tesselation module/stage
    if( _shaderInfo->geometryShaderPath ){
        VkShaderModule geometryModule;
        if( pomCreateShaderModule( &geometryModule, _shaderInfo->tesselationShaderPath, dev ) ){
            LOG( ERR, "Could not create tesselation module of shader" );
            return 1;
        }
        VkPipelineShaderStageCreateInfo *geometryStageInfo = &_shaderInfo->shaderStages[ currStage++ ];
        *geometryStageInfo = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            .module = geometryModule,
            .pName = "main"
        };
    }

    _shaderInfo->numStages = currStage;
    _shaderInfo->initialised = true;
    return 0;
}

int pomShaderDestroy( ShaderInfo *_shaderInfo ){
    if( !_shaderInfo->initialised ){
        LOG( WARN, "Trying to destroy uninitialised shader" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for shader deletion" );
        return 1;
    }
    for( uint8_t i = 0; i < _shaderInfo->numStages; i++ ){
        vkDestroyShaderModule( *dev, _shaderInfo->shaderStages[ i ].module, NULL );
    }
    _shaderInfo->initialised = false;

    return 0;
}