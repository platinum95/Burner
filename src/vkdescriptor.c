#include "vkdescriptor.h"
#include <string.h>

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkDescriptor, log, ##__VA_ARGS__ )

// Create a descriptor. If memoryCtx is NULL, allocate memory. Otherwise use given memory context
int pomVkDescriptorCreate( PomVkDescriptorCtx *_descriptorCtx, 
                           const PomVkUniformBufferObject *_ubo,
                           PomVkDescriptorMemoryInfo *_memoryInfo  ){
    if( _descriptorCtx->initialised ){
        LOG( WARN, "Attempting to reinitialised VkDescriptor" );
        return 1;
    }
    if( !_memoryInfo ){
        // TODO - allocate memory here
        _descriptorCtx->uboDeviceMemory.bufferCtx = &_descriptorCtx->uboDeviceMemory.bufferCtxOwned;
        _descriptorCtx->uboDeviceMemory.uboOffset = 0;
        VkMemoryPropertyFlags uboBufferFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if( pomVkBufferCreate( _descriptorCtx->uboDeviceMemory.bufferCtx,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               _ubo->dataSize, 1, NULL, uboBufferFlags ) ){
            LOG( ERR, "Failed to create UBO buffer" );
            return 1;
        }
        if( pomVkBufferBind( _descriptorCtx->uboDeviceMemory.bufferCtx, 0 ) ){
            LOG( ERR, "Failed to bind UBO buffer" );
            pomVkBufferDestroy( _descriptorCtx->uboDeviceMemory.bufferCtx );
            return 1;
        }
    }
    else{
        // Memory already allocated/owned by caller
        _descriptorCtx->uboDeviceMemory.bufferCtx = _memoryInfo->bufferCtx;
        _descriptorCtx->uboDeviceMemory.uboOffset = _memoryInfo->uboOffset;
        // Ensure the buffer copy is uninitialised so we know not to free the preowned memory
        _descriptorCtx->uboDeviceMemory.bufferCtxOwned.initialised = false;
    }

    // Set up Descriptor Buffer Info
    VkDescriptorBufferInfo *descriptorBufferInfo = &_descriptorCtx->descriptorBufferInfo;
    descriptorBufferInfo->buffer = _descriptorCtx->uboDeviceMemory.bufferCtx->buffer;
    descriptorBufferInfo->offset = _descriptorCtx->uboDeviceMemory.uboOffset;
    descriptorBufferInfo->range = _ubo->dataSize;

    _descriptorCtx->uboMainMemory = *_ubo;
    _descriptorCtx->initialised = true;
    return 0;    
}

int pomVkDescriptorDestroy( PomVkDescriptorCtx *_descriptorCtx ){
    if( !_descriptorCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised VkDescriptor" );
        return 1;
    }
    if( !_descriptorCtx->uboDeviceMemory.bufferCtxOwned.initialised ){
        // Nothing to do
        _descriptorCtx->initialised = false;
        return 0;
    }
    // If we get here, we own the buffer memory for this descriptor
    if( pomVkBufferDestroy( &_descriptorCtx->uboDeviceMemory.bufferCtxOwned ) ){
        LOG( ERR, "Failed to destroy descriptor buffer" );
        return 1;
    }
    return 0;
}

VkDescriptorBufferInfo* pomVkDescriptorGetBufferInfo( PomVkDescriptorCtx *_descriptorCtx ){
    if( !_descriptorCtx->initialised ){
        LOG( ERR, "Attempting to get descriptor buffer info for uninitialised descriptor" );
        return NULL;
    }
    return &_descriptorCtx->descriptorBufferInfo;
}

// Update the memory in VRAM with the memory in main memory
int pomVkDescriptorUpdate( PomVkDescriptorCtx *_descriptorCtx, VkDevice _device ){
    // TODO - add staging support? Or maybe push constants. Either way, just memcpy for now
    if( !_descriptorCtx->initialised ){
        LOG( ERR, "Attempting to update uninitialised descriptor" );
        return 1;
    }
    void *deviceData;
    PomVkDescriptorMemoryInfo *uboDeviceMemoryCtx = &_descriptorCtx->uboDeviceMemory;
    VkDeviceMemory deviceMemory = uboDeviceMemoryCtx->bufferCtx->memCtx.memory;
    PomVkUniformBufferObject *uboMainMemory = &_descriptorCtx->uboMainMemory;

    if( vkMapMemory( _device, deviceMemory, uboDeviceMemoryCtx->uboOffset, uboMainMemory->dataSize,
                     0, &deviceData ) != VK_SUCCESS ){
        LOG( ERR, "Failed to map descriptor memory" );
        return 1;
    }
    memcpy( deviceData, _descriptorCtx->uboMainMemory.data, _descriptorCtx->uboMainMemory.dataSize );
    vkUnmapMemory( _device, deviceMemory );
    return 0;
}



/************
 * Descriptor pool defs
 * *********/

int pomVkDescriptorPoolCreate( PomVkDescriptorPoolCtx *_poolCtx,
                               const PomVkDescriptorPoolInfo *_poolInfo, VkDevice _device ){
    if( _poolCtx->initialised ){
        LOG( WARN, "Attempting to reinitialise Descriptor Pool" );
        return 1;
    }

    // TODO - create all pool sizes. Just do UBO for now
    VkDescriptorPoolSize uboSize = {
        .descriptorCount = _poolInfo->descriptorTypeSizes[ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ],
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pPoolSizes = &uboSize,
        .poolSizeCount = 1,
        .maxSets = uboSize.descriptorCount
    };

    if( vkCreateDescriptorPool( _device, &poolCreateInfo, NULL, &_poolCtx->descriptorPool ) !=
            VK_SUCCESS ){
        LOG( ERR, "Failed to create Descriptor Pool" );
        return 1;
    }

    memcpy( &_poolCtx->poolInfo, _poolInfo, sizeof( PomVkDescriptorPoolInfo ) );
    _poolCtx->initialised = true;
    return 0;
}

int pomVkDescriptorPoolDestroy( PomVkDescriptorPoolCtx *_poolCtx, VkDevice _device ){
    if( !_poolCtx->initialised ){
        LOG( ERR, "Attempting to destroy uninitialised Descriptor Pool" );
        return 1;
    }
    vkDestroyDescriptorPool( _device, _poolCtx->descriptorPool, NULL );
    return 0;
}

VkDescriptorPool pomVkGetDescriptorPool( PomVkDescriptorPoolCtx *_poolCtx ){
    if( !_poolCtx->initialised ){
        LOG( ERR, "Attempting to get uninitialised Descriptor Pool" );
        return NULL;
    }
    return _poolCtx->descriptorPool;
}
