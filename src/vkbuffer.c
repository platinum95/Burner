#include "vkbuffer.h"
#include "vkdevice.h"

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkBuffer, log, ##__VA_ARGS__ )

/**************
** Buffer defs
***************/

int pomVkBufferCreate( PomVkBufferCtx *_buffCtx, VkBufferUsageFlags _usage,
                       VkDeviceSize _bSizeBytes, uint32_t _numQueueFamilies,
                       const uint32_t* _queueFamilies,
                       VkMemoryPropertyFlags _memFlags ){
    if( _buffCtx->initialised ){
        LOG( WARN, "Attempting to reinitialise buffer" );
        return 1;
    }

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = 0, // No sparse buffers for now
        .pNext = NULL,
        .size = _bSizeBytes,
        .usage = _usage,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = _queueFamilies,
        .sharingMode = ( _numQueueFamilies > 1 ) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE
    };

    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create buffer with no available device" );
        return 1;
    }
    if( vkCreateBuffer( *dev, &createInfo, NULL, &_buffCtx->buffer ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create buffer" );
        return 1;
    }
    if( pomLinkedListInit( &_buffCtx->childViews ) ){
        LOG( ERR, "Failed to initialised buffer's linked list of child views" );
        vkDestroyBuffer( *dev, _buffCtx->buffer, NULL );
        return 1;
    }
    // Get memory requirements
    vkGetBufferMemoryRequirements( *dev, _buffCtx->buffer, &_buffCtx->memoryRequirements );
    _buffCtx->memoryFlags = _memFlags;
    _buffCtx->initialised = true;
    atomic_store( &_buffCtx->inUse, false );
    _buffCtx->bufferInfo = createInfo;
    return 0;
}

int pomVkBufferDestroy( PomVkBufferCtx *_buffCtx ){
    if( !_buffCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised buffer" );
        return 1;
    }
    // Ensure this buffer isn't being used elsewhere
    if( atomic_load( &_buffCtx->inUse ) ){
        LOG( ERR, "Attempting to destroy buffer in use" );
        return 1;
    }
    // Make sure none of our child buffer views are in use
    PomLinkedListNode *cNode = _buffCtx->childViews.head;
    while( cNode ){
        PomVkBufferViewCtx *childCtx = (PomVkBufferViewCtx*) cNode->key;
        if( atomic_load( &childCtx->inUse ) ){
            // Child bufferview is in use
            LOG( ERR, "Attempting to destroy buffer while dependant view is in use" );
            return 1;
        }
        if( childCtx->initialised ){
            pomVkBufferViewDestroy( childCtx );
        }
        cNode = cNode->next;
    }
    // Destroy the linked list
    pomLinkedListClear( &_buffCtx->childViews );
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy buffer with no valid device" );
        return 1;
    }

    vkDestroyBuffer( *dev, _buffCtx->buffer, NULL );
    // Make sure any memory is freed
    pomVkFreeMemory( &_buffCtx->memCtx );
    _buffCtx->initialised = false;
    LOG( DEBUG, "Buffer destroyed successfully" );
    return 0;
}

// Return 0 if successfully bound, 1 if error occurred, 2 if already bound
int pomVkBufferBind( PomVkBufferCtx *_buffCtx, VkDeviceSize _offset ){
    bool bound = atomic_load( &_buffCtx->bound );
    if( bound ){
        // No need for buffer to be rebound
        atomic_store( &_buffCtx->inUse, true );
        return 2;
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to bind buffer with no valid device" );
        return 1;
    }
    if( !_buffCtx->initialised ){
        LOG( ERR, "Attempting to bind uninitialised buffer" );
        return 1;
    }

    if( !_buffCtx->memCtx.initialised ){
        // Memory not allocated
        VkMemoryPropertyFlags memFlags = _buffCtx->memoryFlags;
        VkMemoryRequirements *memReq = &_buffCtx->memoryRequirements;
        if( pomVkAllocateMemory( &_buffCtx->memCtx, memFlags, memReq, memReq ) ){
            LOG( ERR, "Failed to allocate buffer memory" );
            return 1;
        }
    }
    atomic_store( &_buffCtx->inUse, true );
    if( vkBindBufferMemory( *dev, _buffCtx->buffer, _buffCtx->memCtx.memory, _offset ) != VK_SUCCESS ){
        LOG( ERR, "Failed to bind buffer memory" );
        atomic_store( &_buffCtx->inUse, false );
        return 1;
    }
    atomic_store( &_buffCtx->bound, true );
    return 0;
}

int pomVkBufferUnbind( PomVkBufferCtx *_buffCtx ){
    // TODO - unbind handling
    // Vulkan doesn't have a vkBufferUnbindMemory type call.
    // Only way to unbind a buffer from memory is to destroy the buffer object.
    // At that point, the allocated memory can be released.
    // Maybe this function should then indicate that this buffer is not currently
    // in use, and may be destroyed if memory is required for another object.
    // For now, then, keep the buffer in memory but signal that this buffer
    // is not currently in use.
    // In that case, function name may need to be changed to be more explicit
    // that a call to it does not guarantee the underlying buffer to actually
    // be unbound.

    atomic_store( &_buffCtx->inUse, false );
    return 0;
}


/******************
 * BufferView defs
 ******************/

int pomVkBufferViewCreate( PomVkBufferViewCtx *_buffViewCtx, PomVkBufferCtx *_buffCtx,
                           VkDeviceSize _offset, VkDeviceSize _range, VkFormat _format ){
    if( _buffViewCtx->initialised ){
        LOG( WARN, "Attempting to reinitialise buffer view" );
        return 1;
    }

    if( !_buffCtx->initialised ){
        LOG( ERR, "Attempting to initialise bufferview with uninitialised buffer" );
        return 1;
    }

    // TODO - check texel size limits if applicable

    VkBufferViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .buffer = _buffCtx->buffer,
        .offset = _offset,
        .range = _range,
        .format = _format
    };

    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create bufferview with no valid device" );
        return 1;
    }
    if( vkCreateBufferView( *dev, &createInfo, NULL, &_buffViewCtx->bufferView ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create bufferview" );
        return 1;
    }

    // Register ourselves with the parent buffer
    if( pomLinkedListAdd( &_buffCtx->childViews, (PllKeyType) _buffCtx ) ){
        LOG( ERR, "Failed to register bufferview with parent buffer" );
        vkDestroyBufferView( *dev, _buffViewCtx->bufferView, NULL );
        return 1;
    }
    _buffViewCtx->initialised = true;
    atomic_store( &_buffViewCtx->inUse, false );
    _buffViewCtx->parentBuffer = _buffCtx;
    return 0;
}

int pomVkBufferViewDestroy( PomVkBufferViewCtx *_buffViewCtx ){
    if( !_buffViewCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised buffer view" );
        return 1;
    }
    // Ensure this buffer isn't being used elsewhere
    if( atomic_load( &_buffViewCtx->inUse ) ){
        LOG( ERR, "Attempting to destroy bufferview in use" );
        return 1;
    }

    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy bufferview with no valid device" );
        return 1;
    }

    // For now, we don't need to deregister ourselves from the parent buffer,
    // since we're unlikely to be removing views on the fly anyway.
    // This may change in future.
    vkDestroyBufferView( *dev, _buffViewCtx->bufferView, NULL );
    _buffViewCtx->initialised = false;
    LOG( DEBUG, "Bufferview destroyed successfully" );

    return 0;
}