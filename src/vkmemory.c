#include "vkmemory.h"
#include "vkdevice.h"

#define LOG( lvl, log, ... ) LOG_MODULE( lvl, VkMemory, log, ##__VA_ARGS__ )

static int findMemoryType( uint32_t *_index, VkMemoryPropertyFlags _memFlags,
                           VkMemoryRequirements *_memReqs ){
    VkPhysicalDevice *phyDev = pomGetPhysicalDevice();
    if( !phyDev ){
        LOG( ERR, "Attempting to allocated memory with no available physical device" );
        return 1;
    }
    // TODO - cache this
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties( *phyDev, &memProps );
    
    uint32_t cMaxScore = 0;
    uint32_t cMaxIdx = 0;
    for( uint32_t i = 0; i < memProps.memoryTypeCount; i++ ){
        VkMemoryType *cType = &memProps.memoryTypes[ i ];
        uint32_t mScore = 0;
        if( ( ( cType->propertyFlags & _memFlags ) == _memFlags ) &&
            ( _memReqs->memoryTypeBits & (1<<i) ) ){
            // Memory type meets requirements
            // TODO - check for desired properties and make real score
            mScore = 1;
            if( mScore > cMaxScore ){
                cMaxScore = mScore;
                cMaxIdx = i;
            }
        }
    }
    if( cMaxScore == 0 ){
        return 1;
    }
    *_index = cMaxIdx;
    return 0;
}
int pomVkAllocateMemory( PomVkMemoryCtx *_memCtx,
                         VkMemoryPropertyFlags _memFlags, 
                         VkMemoryRequirements *_memReq,
                         VkMemoryRequirements *UNUSED(_memDesired) ){
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to allocate memory with no available device" );
    }
    if( _memCtx->initialised ){
        LOG( WARN, "Attempting to reallocate memory" );
        return 1;
    }

    VkMemoryAllocateInfo allocInfo ={
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = _memReq->size
    };

    if( findMemoryType( &allocInfo.memoryTypeIndex, _memFlags, _memReq ) ){
        LOG( ERR, "Failed to find suitable memory type" );
        return 1;
    }

    if( vkAllocateMemory( *dev, &allocInfo, NULL, &_memCtx->memory ) != VK_SUCCESS ){
        LOG( ERR, "Failed to allocate memory" );
        return 1;
    }
    _memCtx->initialised = true;
    return 0;
}

int pomVkFreeMemory( PomVkMemoryCtx *_memCtx ){
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to free memory with no available device" );
    }
    if( !_memCtx->initialised ){
        LOG( WARN, "Attempting to free unallocated memory" );
        return 1;
    }
    vkFreeMemory( *dev, _memCtx->memory, NULL );
    return 0;
}