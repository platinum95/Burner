#include "vkrendergroup.h"
#include <string.h>
#include <stdlib.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkRenderGroup, log, ##__VA_ARGS__ )

// TODO - auto set pipeline->shader->attributes
int pomVkRenderGroupCreate( PomVkRenderGroupCtx *_renderGroupCtx, PomPipelineCtx *_pipelineCtx,
                            uint32_t numModels, PomVkModelCtx *_models[] ){
    if( _renderGroupCtx->initialised ){
        LOG( WARN, "Attempting to reinitilise rendergroup" );
        return 1;
    }
    // Make sure model interfaces are the same
    // TODO - ensure no segfault on this dereference
    size_t baseStride = _models[ 0 ]->modelMeshInfo->dataStride;
    for( uint32_t i = 1; i < numModels; i++ ){
        PomVkModelCtx *model = _models[ i ];
        if( model->modelMeshInfo->dataStride != baseStride ){
            LOG( ERR, "RenderGroup model interfaces are not equal" );
            return 1;
        }
    }
    // Make sure model interface aligns with shader interface
    ShaderInfo *shaderInfo = &_pipelineCtx->shaderInfo;
    size_t shaderAttrsStride = shaderInfo->shaderInputAttributes.totalStride;
    if( shaderAttrsStride != baseStride ){
        LOG( ERR, "Model attributes do not align with shader input attributes" );
        return 1;
    }
    // TODO - make sure shader output attributes align with renderpass output
    _renderGroupCtx->modelList = (PomVkModelCtx**) malloc( sizeof( PomVkModelCtx ) * numModels );
    memcpy( _renderGroupCtx->modelList, _models, sizeof( PomVkModelCtx*) * numModels );
    _renderGroupCtx->numModels = numModels;
    _renderGroupCtx->pipelineCtx = _pipelineCtx;
    _renderGroupCtx->initialised = true;
    return 0;
}

int pomVkRenderGroupDestroy( PomVkRenderGroupCtx *_renderGroupCtx ){
    if( !_renderGroupCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised rendergroup" );
        return 1;
    }
    free( _renderGroupCtx->modelList );
    _renderGroupCtx->initialised = false;
    return 0;
}

int pomVkRenderGroupRecord( PomVkRenderGroupCtx *_renderGroupCtx, VkCommandBuffer _cmdBuffer){
    vkCmdBindPipeline( _cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _renderGroupCtx->pipelineCtx->pipeline );
    
    for( uint32_t i = 0; i < _renderGroupCtx->numModels; i++ ){
        PomVkModelCtx *model = _renderGroupCtx->modelList[ i ];
        if( !model->active ){
            // Ignore inactive model
            continue;
        }
        
        // Vertex data starts after index data in buffer
        size_t meshIndexSize = model->modelMeshInfo->numIndices * sizeof( uint32_t );
        VkDeviceSize offsets[ 1 ] = { meshIndexSize };

        vkCmdBindVertexBuffers( _cmdBuffer, 0, 1, &model->modelBuffer.buffer, offsets );
        vkCmdBindIndexBuffer( _cmdBuffer, model->modelBuffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

        vkCmdDraw( _cmdBuffer, model->modelMeshInfo->numVertices, 1, 0, 0);
    }

    return 0;
}

