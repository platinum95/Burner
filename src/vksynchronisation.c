#include "vksynchronisation.h"
#include "vkdevice.h"

#define LOG( level, log, ... ) LOG_MODULE( level, vksynchronisation, log, ##__VA_ARGS__ )

int pomSemaphoreCreate( PomSemaphoreCtx *_semaphoreCtx ){
    if( _semaphoreCtx->initialised ){
        LOG( WARN, "Attempting to re-initialise semaphore" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create semaphore with no available logical device" );
        return 1;
    }
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if( vkCreateSemaphore( *dev, &semaphoreInfo, NULL, &_semaphoreCtx->semaphore ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create semaphore" );
        return 1;
    }

    _semaphoreCtx->initialised = true;
    return 0;
}

int pomSemaphoreDestroy( PomSemaphoreCtx *_semaphoreCtx ){
    if( !_semaphoreCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised semaphore" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy semaphore with no available logical device" );
        return 1;
    }
    vkDestroySemaphore( *dev, _semaphoreCtx->semaphore, NULL );
    return 0;
}