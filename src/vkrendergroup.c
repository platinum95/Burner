#include "vkrendergroup.h"
#include "vkdevice.h"
#include <string.h>
#include <stdlib.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkRenderGroup, log, ##__VA_ARGS__ )

static int rgGetDescriptorSetSize( PomVkRenderGroupCtx *_renderGroupCtx );
// TODO - auto set pipeline->shader->attributes
int pomVkRenderGroupCreate( PomVkRenderGroupCtx *_renderGroupCtx, PomPipelineCtx *_pipelineCtx,
                            uint32_t _numLocalDescriptors, PomVkDescriptorCtx *_localDescriptorsCtx,
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

    // Get/set the number of descriptor sets used by this rendergroup (local + model)
    if( rgGetDescriptorSetSize( _renderGroupCtx ) ){
        LOG( ERR, "Failed to get RenderGroups Descriptor Set count" );
        free( _renderGroupCtx->modelList );
        return 1;
    }

    _renderGroupCtx->allDescriptorSets = NULL;
    _renderGroupCtx->initialised = true;
    _renderGroupCtx->numLocalDescriptors = _numLocalDescriptors;
    _renderGroupCtx->localDescriptors = _localDescriptorsCtx;   // TODO - should we copy here?;lp
    return 0;
}

int pomVkRenderGroupDestroy( PomVkRenderGroupCtx *_renderGroupCtx ){
    if( !_renderGroupCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised rendergroup" );
        return 1;
    }
    if( _renderGroupCtx->allDescriptorSets ){
        free( _renderGroupCtx->allDescriptorSets );
    }
    free( _renderGroupCtx->modelList );
    _renderGroupCtx->initialised = false;
    return 0;
}

int pomVkRenderGroupAllocateDescriptorSets( PomVkRenderGroupCtx *_renderGroupCtx,
                                            VkDescriptorPool _descriptorPool ){
    if( !_renderGroupCtx->initialised ){
        LOG( ERR, "Attempting to allocate uninitialised RenderGroup's Descriptor Sets" );
        return 1;
    }
    if( _renderGroupCtx->allDescriptorSets ){
        LOG( WARN, "Attempting to reallocate RenderGroup's Descriptor Sets" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for shader creation" );
        return 1;
    }
    uint32_t swapchainImageCount;
    if( !pomGetSwapchainImages( &swapchainImageCount ) ){
        LOG( ERR, "Failed to get swapchain image count" );
        return 1;
    }

//    uint32_t totalDS = _renderGroupCtx->numModelDescriptorSets +
//                       _renderGroupCtx->numRenderGroupDescriptorSets;
        
    //uint32_t numDSLPerBuffer = totalDS / swapchainImageCount;
    // TODO - maybe check a round division here?

    // Create an array of DescriptorSetLayouts from Shader
    VkDescriptorSetLayout *layouts = NULL;
    ShaderDescriptorSetCtx *shaderDSCtx = &_renderGroupCtx->pipelineCtx->shaderInfo.
                                            shaderInputAttributes.descriptorSetLayoutCtx;
    VkDescriptorSetLayout *shaderRGDSLs = shaderDSCtx->renderGroupSetLayouts;
    VkDescriptorSetLayout *shaderModelDSLs = shaderDSCtx->modelSetLayouts;
    uint32_t numShaderModelDSL = shaderDSCtx->numModelLocalLayouts;
    uint32_t numShaderRGDSL = shaderDSCtx->numRenderGroupLocalLayouts;
    //uint32_t numShaderDSL = numShaderRGDSL + numShaderModelDSL;

    
    uint32_t numModels = _renderGroupCtx->numModels;
    const size_t rgDSLArraySize = sizeof( VkDescriptorSetLayout ) * numShaderRGDSL;
    const size_t modelDSLArraySize = sizeof( VkDescriptorSetLayout ) * numShaderModelDSL;
    const size_t setsPerSwapchainImage = ( numShaderModelDSL * numModels ) + numShaderRGDSL;
    const uint32_t totalSetCount = setsPerSwapchainImage * swapchainImageCount;
    const size_t layoutAllocateSize = totalSetCount * sizeof( VkDescriptorSetLayout );
    const size_t setSize = totalSetCount * sizeof( VkDescriptorSet );
    // Create a (temporary) spot in memory to hold all the layouts we're going to allocate sets for
    layouts = (VkDescriptorSetLayout*) malloc( layoutAllocateSize );
    VkDescriptorSetLayout *currLayoutOffset = layouts;
    
    // Create the layout array indicating the sets we're going to allocate
    // Loop for each swapchain image
    for( uint32_t i = 0; i < swapchainImageCount; i++ ){
        memcpy( currLayoutOffset, shaderRGDSLs, rgDSLArraySize );
        currLayoutOffset += 1;
        // Loop for each model
        for( uint32_t modelIdx = 0; modelIdx < numModels; modelIdx++ ){
            memcpy( currLayoutOffset, shaderModelDSLs, modelDSLArraySize );
            currLayoutOffset += 1;
        }
    }

    // Now allocate the descriptor sets
    VkDescriptorSetAllocateInfo setAllocInfo;
    setAllocInfo.descriptorPool = _descriptorPool;
    setAllocInfo.descriptorSetCount = totalSetCount;
    setAllocInfo.pNext = NULL;
    setAllocInfo.pSetLayouts = layouts;
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    VkDescriptorSet *descriptorSets = (VkDescriptorSet*) malloc( setSize );
    if( vkAllocateDescriptorSets( *dev, &setAllocInfo, descriptorSets ) != VK_SUCCESS ){
        LOG( ERR, "Failed to allocate descriptor set" );
        return 1;
    }
    // Set up the descriptor sets
    for( uint32_t i = 0; i < swapchainImageCount; i++ ){
        VkDescriptorSet *currSetGroup = &descriptorSets[ setsPerSwapchainImage * i ];
        uint32_t currSetOffset = 0;
        // Set up rendergroup-local DS
        for( uint32_t rgDsIdx = 0; rgDsIdx < _renderGroupCtx->numLocalDescriptors; rgDsIdx++ ){
            PomVkDescriptorCtx *rgDescriptorCtx = &_renderGroupCtx->localDescriptors[ rgDsIdx ];
            VkDescriptorBufferInfo *buffInfo = pomVkDescriptorGetBufferInfo( rgDescriptorCtx );
            if( !buffInfo ){
                LOG( ERR, "Could not get render-local descriptor buffer info" );
                // TODO - cleanup here
                return 1;
            }
            VkWriteDescriptorSet writeDS = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = currSetGroup[ currSetOffset ],
                .dstBinding = 0, // TODO - get binding here (maybe from shader?)
                .dstArrayElement = 0, // Assume no arrays for now
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Assume only UBOs for now
                .descriptorCount = 1, // 1 descriptor per DS for now
                .pBufferInfo = buffInfo,
                .pImageInfo = NULL, // Only UBOs for now
                .pTexelBufferView = NULL, // Ditto
            };
            currSetOffset++;
            // Could maybe defer this till the end?
            vkUpdateDescriptorSets( *dev, 1, &writeDS, 0, NULL );

        }

        // Loop for each model
        for( uint32_t modelIdx = 0; modelIdx < numModels; modelIdx++ ){
            PomVkModelCtx *model = _renderGroupCtx->modelList[ modelIdx ];

            PomVkDescriptorCtx *modelDescriptorCtx = &model->modelDescriptorCtx;
            VkDescriptorBufferInfo *descBuffInfo = pomVkDescriptorGetBufferInfo( modelDescriptorCtx );
            if( !descBuffInfo ){
                LOG( ERR, "Could not get model-local descriptor buffer info" );
                // TODO - cleanup here
                return 1;
            }
            VkWriteDescriptorSet writeDS = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = currSetGroup[ currSetOffset ],
                .dstBinding = 1, // TODO - get binding here (maybe from shader?)
                .dstArrayElement = 0, // Assume no arrays for now
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Assume only UBOs for now
                .descriptorCount = 1, // 1 descriptor per DS for now
                .pBufferInfo = descBuffInfo,
                .pImageInfo = NULL, // Only UBOs for now
                .pTexelBufferView = NULL, // Ditto
            };
            currSetOffset++;
            // Could maybe defer this till the end?
            vkUpdateDescriptorSets( *dev, 1, &writeDS, 0, NULL );
        }
    }
    _renderGroupCtx->allDescriptorSets = descriptorSets;
    _renderGroupCtx->setsPerSwapchainImage = setsPerSwapchainImage;
    free( layouts );

    return 0;
}


int pomVkRenderGroupRecord( PomVkRenderGroupCtx *_renderGroupCtx, VkCommandBuffer _cmdBuffer,
                            uint32_t bufferIdx ){
      
    vkCmdBindPipeline( _cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                       _renderGroupCtx->pipelineCtx->pipeline );
    
    // Bind rendergroup-local Descriptors
    const uint32_t descriptorSetOffset =  _renderGroupCtx->setsPerSwapchainImage * bufferIdx;
    VkDescriptorSet *descriptorSets = &_renderGroupCtx->allDescriptorSets[ descriptorSetOffset ];
    const uint32_t numRgLocalDSL = _renderGroupCtx->pipelineCtx->shaderInfo.shaderInputAttributes.
                                        descriptorSetLayoutCtx.numRenderGroupLocalLayouts;
    const uint32_t numModelLocalDSL = _renderGroupCtx->pipelineCtx->shaderInfo.shaderInputAttributes.
                                        descriptorSetLayoutCtx.numModelLocalLayouts;
    vkCmdBindDescriptorSets( _cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             _renderGroupCtx->pipelineCtx->graphicsPipelineInfo.layout,
                             0, numRgLocalDSL,
                             descriptorSets, 0, NULL );
    uint32_t numRgLocalLayouts =  _renderGroupCtx->pipelineCtx->shaderInfo.shaderInputAttributes.
                                    descriptorSetLayoutCtx.numRenderGroupLocalLayouts;
    VkDescriptorSet *modelDescriptorSets = &descriptorSets[ numRgLocalLayouts ];

    // TODO - maybe a per-model secondary command?
    for( uint32_t i = 0; i < _renderGroupCtx->numModels; i++ ){
        PomVkModelCtx *model = _renderGroupCtx->modelList[ i ];
        if( !model->active ){
            // Ignore inactive model
            continue;
        }
        
        // Bind model-local descriptors
        VkDescriptorSet *modelDS = &modelDescriptorSets[ i ];
        vkCmdBindDescriptorSets( _cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                _renderGroupCtx->pipelineCtx->graphicsPipelineInfo.layout,
                                1, numModelLocalDSL, // Only 1 DS per model for now
                                modelDS, 0, NULL );

        // Vertex data starts after index data in buffer
        size_t meshIndexSize = model->modelMeshInfo->numIndices * sizeof( uint32_t );
        VkDeviceSize offsets[ 1 ] = { meshIndexSize };
        
        vkCmdBindVertexBuffers( _cmdBuffer, 0, 1, &model->modelBuffer.buffer, offsets );
        vkCmdBindIndexBuffer( _cmdBuffer, model->modelBuffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

        vkCmdDraw( _cmdBuffer, model->modelMeshInfo->numVertices, 1, 0, 0);
    }

    return 0;
}


static int rgGetDescriptorSetSize( PomVkRenderGroupCtx *_renderGroupCtx ){
    ShaderInfo *shaderInfo = &_renderGroupCtx->pipelineCtx->shaderInfo;
    // For brevity, DSL = DescriptorSetLayouts
    uint32_t numModelDSL = shaderInfo->shaderInputAttributes.
                                descriptorSetLayoutCtx.numModelLocalLayouts;
    uint32_t numRenderGroupDSL = shaderInfo->shaderInputAttributes.
                                    descriptorSetLayoutCtx.numRenderGroupLocalLayouts;
    uint32_t numModels = _renderGroupCtx->numModels;
    // TODO - Get number of swapchain images here
    uint32_t swapchainImageCount;
    if( !pomGetSwapchainImages( &swapchainImageCount ) ){
        LOG( ERR, "Failed to get swapchain image count" );
        return 1;
    }
    uint32_t numRgLocalDSets = numRenderGroupDSL * swapchainImageCount;
    uint32_t numModelDSets = ( numModelDSL * numModels ) * swapchainImageCount;
    
    _renderGroupCtx->numRenderGroupDescriptorSets = numRgLocalDSets;
    _renderGroupCtx->numModelDescriptorSets = numModelDSets;
    return 0;
}

