#include "vkmodel.h"
#include "vkdevice.h"
#include <string.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkModel, log, ##__VA_ARGS__ )

// We'll just look at the first submodel/mesh in the model.
// TODO - handle actual models, not just meshes
int pomVkModelCreate( PomVkModelCtx *_modelCtx, PomModelMeshInfo *_meshInfo ){
    if( _modelCtx->initialised ){
        LOG( WARN, "Attempting to reinitialised model" );
        return 1;
    }
    // Model buffer will be vertex + index in one
    VkBufferUsageFlags bufferFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    const size_t uboAlignment = 0x20; // TODO - get this from PhysicalDeviceLimits.minUniformBufferOffsetAlignment
    const size_t boundaryMask = SIZE_MAX - ( uboAlignment - 1 );

    _modelCtx->modelMeshInfo = _meshInfo;
    const size_t modelSize = _meshInfo->dataSize;
    const size_t alignedUboOffset = ( ( modelSize - 1 ) + uboAlignment ) & boundaryMask;
    const size_t uboPadding = alignedUboOffset - modelSize;
    const size_t descriptorDataSize = sizeof( Mat4x4 );
    const size_t modelBufferSize = modelSize + uboPadding + descriptorDataSize;
    if( pomVkBufferCreate( &_modelCtx->modelBuffer, bufferFlags, 
                           modelBufferSize, 1, NULL, 
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) ){
        LOG( ERR, "Failed to create model buffer" );
        return 1;
    }
    // Model buffer laid out in memory as <indices><vertices><padding><descriptor data>

    // Set up model descriptor
    PomVkUniformBufferObject ubo = {
        .data = &_modelCtx->transformationMatrix,
        .dataSize = sizeof( Mat4x4 )
    };

    PomVkDescriptorMemoryInfo memInfo ={
        .bufferCtx = &_modelCtx->modelBuffer,
        .uboOffset = alignedUboOffset
    };

    if( pomVkDescriptorCreate( &_modelCtx->modelDescriptorCtx, &ubo, &memInfo ) ){
        LOG( ERR, "Failed to create model descriptor" );
        pomVkBufferDestroy( &_modelCtx->modelBuffer ); 
        return 1;
    }

    
    _modelCtx->initialised = true;
    _modelCtx->active = false;
    return 0;
}

int pomVkModelDestroy( PomVkModelCtx *_modelCtx ){
    if( !_modelCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised model" );
        return 1;
    }
    if( pomVkBufferDestroy( &_modelCtx->modelBuffer ) ){
        LOG( ERR, "Failed to destroy model's buffer" );
        return 1;
    }
    _modelCtx->initialised = false;
    _modelCtx->active = false;

    return 0;
}

// Indicate that model must be available to the GPU.
int pomVkModelActivate( PomVkModelCtx *_modelCtx ){
    if( !_modelCtx->initialised ){
        LOG( ERR, "Attempting to activate uninitialised model" );
        _modelCtx->active = false;
        return 1;
    }
    if( _modelCtx->active ){
        LOG( DEBUG, "Activating already active model" );
        return 2;
    }
    int bindRet = pomVkBufferBind( &_modelCtx->modelBuffer, 0 );
    if( bindRet == 1 ){
        // Error binding
        LOG( ERR, "Failed to bind model buffer" );
        return 1;
    }
    if( bindRet == 2 ){
        // Model already bound, memory in GPU
        LOG( DEBUG, "Model buffer already bound" );
        _modelCtx->active = true;
        return 0;
    }
    // If we get here, we've bound the buffer and need to send our
    // model data to it.
    // TODO - updload model data
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to activate model with no available device" );
        return 1;
    }
    // After binding the buffer we're guaranteed the memory was allocated.
    // TODO - staging buffers
    // TODO - handle index buffer
    size_t bufferSize = _modelCtx->modelBuffer.bufferInfo.size;
    size_t modelMeshSize = _modelCtx->modelMeshInfo->dataSize;
    VkDeviceMemory deviceMemory = _modelCtx->modelBuffer.memCtx.memory;
    void* data;
    vkMapMemory( *dev, deviceMemory,
                 0, bufferSize, 0, &data );
    memcpy( data, _modelCtx->modelMeshInfo->dataBlockOffset, (size_t) modelMeshSize );
    
    vkUnmapMemory( *dev, deviceMemory );
    
    // Update the descriptor data too
    pomVkDescriptorUpdate( &_modelCtx->modelDescriptorCtx, *dev );
    _modelCtx->active = true;
    return 0;
}

// Indicate that model is not needed by GPU. This does not
// guarantee that the model will be removed, just that it
// may be removed.
int pomVkModelDeactivate( PomVkModelCtx *_modelCtx ){
    _modelCtx->active = false;
    if( !_modelCtx->initialised ){
        LOG( ERR, "Attempting to deactivate uninitialised model" );
        return 1;
    }

    if( pomVkBufferUnbind( &_modelCtx->modelBuffer ) ){
        LOG( ERR, "Failed to unbind model data" );
        return 1;
    }
    return 0;
}

int pomVkModelUpdateDescriptors( PomVkModelCtx *_modelCtx, VkDevice _device ){
    // For now just update the only descriptor we have. This might change to a more complex
    // function later on that selectively updates some descriptors, or updates different types
    // of descriptors.
    return pomVkDescriptorUpdate( &_modelCtx->modelDescriptorCtx, _device );
}

PomVkDescriptorCtx* pomVkModelGetDescriptor( PomVkModelCtx *_modelCtx ){
    if( !_modelCtx->initialised ){
        LOG( ERR, "Attempting to get descriptor from uninitialised model" );
        return NULL;
    }
    return &_modelCtx->modelDescriptorCtx;
}