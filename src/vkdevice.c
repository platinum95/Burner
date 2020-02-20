#include "vkdevice.h"
#include "common.h"
#include <stdint.h>
#include "vkinstance.h"
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include <string.h>

#define LOG( log, ... ) LOG_MODULE( DEBUG, vkdevice, log, ##__VA_ARGS__ )
#define LOG_WARN( log, ... ) LOG_MODULE( WARN, vkdevice, log, ##__VA_ARGS__ )
#define LOG_ERR( log, ... ) LOG_MODULE( ERR, vkdevice, log, ##__VA_ARGS__ )


typedef struct VkDeviceCtx VkDeviceCtx;
struct VkDeviceCtx{
    bool initialised;
    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;
    VkQueue mainGfxQueue;
};
VkDeviceCtx vkDeviceCtx = { 0 };

VkQueueFamilyProperties queueRequiredProps = {
    .minImageTransferGranularity = { .width=0, .height=0, .depth=0 },
    .queueCount = 0,
    .queueFlags = (VkQueueFlags) 0,
    .timestampValidBits = 0
};

typedef struct VkQueueFlagReq{
    VkQueueFlags qFlag;
    uint32_t qIdx;
    bool found;
} VkQueueFlagReq;


VkQueueFlagReq vkQueueFlagsReq[] = {
    { .qFlag = VK_QUEUE_GRAPHICS_BIT },
    { .qFlag = VK_QUEUE_TRANSFER_BIT }
};

// Return 0 if ineligible, and a score >0 if eligible
static uint16_t phyDeviceElgibility( const VkPhysicalDevice *_device, const VkPhysicalDeviceProperties *_props ){
    // TODO implement this
    uint16_t score = 0;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures( *_device, &deviceFeatures );
    uint32_t qFamCount;
    vkGetPhysicalDeviceQueueFamilyProperties( *_device, &qFamCount, NULL );
    VkQueueFamilyProperties *qProps = (VkQueueFamilyProperties*) malloc( sizeof( VkQueueFamilyProperties ) * qFamCount );
    vkGetPhysicalDeviceQueueFamilyProperties( *_device, &qFamCount, qProps );

    
    if( _props->deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ){
        score += 1;
    }
    uint32_t numReqFlags = sizeof( vkQueueFlagsReq ) / sizeof( vkQueueFlagsReq[ 0 ] );
    // Reset queue flags for this device
    for( uint32_t reqId = 0; reqId < numReqFlags; reqId++ ){
        VkQueueFlagReq *fReq = &vkQueueFlagsReq[ reqId ];
        fReq->found = false;
    }
    // Make sure the queue flags we require are available in this device
    for( uint32_t i = 0; i < qFamCount; i++ ){
        VkQueueFamilyProperties *qFamProp = &qProps[ i ];
        for( uint32_t reqId = 0; reqId < numReqFlags; reqId++ ){
            VkQueueFlagReq *fReq = &vkQueueFlagsReq[ reqId ];
            if( fReq->found ){
                continue;
            }
            if( qFamProp->queueFlags & fReq->qFlag ){
                // Found the queue flag
                fReq->found = true;
            }
        }
    }

    // Check that we've found all required flags
    for( uint32_t reqId = 0; reqId < numReqFlags; reqId++ ){
        VkQueueFlagReq *fReq = &vkQueueFlagsReq[ reqId ];
        if( !fReq->found ){
            LOG_WARN( "Device %s does not support all required queue families", _props->deviceName );
            return 0;
        }
    }

    free( qProps );
    
    return score;
}

int pomPickPhysicalDevice(){
    VkInstance *instance = pomGetVkInstance();
    if( !instance ){
        // VK not instantiated
        LOG_ERR( "Vulkan Instance not instantiated" );
        return 1;
    }
    uint32_t phyDevCount;
    vkEnumeratePhysicalDevices( *instance, &phyDevCount, NULL );
    if( phyDevCount == 0 ){
        LOG_ERR( "Could not find any compatible devices" );
        return 1;
    }
    LOG( "Found %i Vulkan device(s)", phyDevCount );
    VkPhysicalDevice *phyDevices = (VkPhysicalDevice*) malloc( sizeof( VkPhysicalDevice ) * phyDevCount );
    vkEnumeratePhysicalDevices( *instance, &phyDevCount, phyDevices );

    // See if we're supposed to find a particular device
    const char * requestedDev = pomMapGet( &systemConfig.mapCtx, "vulkan_device_name", NULL );
    // Rate each available device and choose best
    uint16_t hiScore = 0, hiIdx = 0;
    for( uint32_t i = 0; i < phyDevCount; i++ ){
        VkPhysicalDevice *dev = &phyDevices[ i ];
        VkPhysicalDeviceProperties devProps;
        vkGetPhysicalDeviceProperties( *dev, &devProps );
        uint16_t devScore = phyDeviceElgibility( dev, &devProps );
        if( requestedDev && strcmp( devProps.deviceName, requestedDev ) == 0 ){
            // Found the requested device, check if its suitable
            if( devScore == 0 ){
                LOG_WARN( "Requested device %s does not meet requirements", requestedDev );
                continue;
            }
            hiIdx = i;
            hiScore = devScore;
            break;
        }
        if( devScore > hiScore ){
            hiScore = devScore;
            hiIdx = i;
        }
    }

    if( hiScore == 0 ){
        LOG_ERR( "Could not find a suitable device" );
        return 1;
    }
    VkPhysicalDevice *dev = &phyDevices[ hiIdx ];
    vkDeviceCtx.physicalDevice = *dev;
    VkPhysicalDeviceProperties devProps;
    vkGetPhysicalDeviceProperties( *dev, &devProps );
    const char * devName = devProps.deviceName;
    if( requestedDev && strcmp( devName, requestedDev ) != 0 ){
        LOG_WARN( "Requested device %s could not be found", requestedDev );
    }
    LOG( "Using device %s", devName );
    // Make sure the config is set with our chosen device
    pomMapSet( &systemConfig.mapCtx, "vulkan_device_name", devProps.deviceName );

    // Now that we have a physical device selected, get the required Queue indices
    uint32_t qFamCount;
    vkGetPhysicalDeviceQueueFamilyProperties( *dev, &qFamCount, NULL );
    VkQueueFamilyProperties *qProps = (VkQueueFamilyProperties*) malloc( sizeof( VkQueueFamilyProperties ) * qFamCount );
    vkGetPhysicalDeviceQueueFamilyProperties( *dev, &qFamCount, qProps );
    uint32_t numReqFlags = sizeof( vkQueueFlagsReq ) / sizeof( vkQueueFlagsReq[ 0 ] );
    for( uint32_t i = 0; i < qFamCount; i++ ){
        VkQueueFamilyProperties *qFamProp = &qProps[ i ];
        for( uint32_t reqId = 0; reqId < numReqFlags; reqId++ ){
            VkQueueFlagReq *fReq = &vkQueueFlagsReq[ reqId ];
            if( fReq->found ){
                continue;
            }
            if( qFamProp->queueFlags & fReq->qFlag ){
                // Found the queue flag
                fReq->found = true;
                fReq->qIdx = i;
                
            }
        }
    }
    free( qProps );

    // Set up the logical device
    // First specify the queues to be created
    // TODO - extend beyond just the graphics queue
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo vkCreateQueueInfo ={
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .queueFamilyIndex = vkQueueFlagsReq[ 0 ].qIdx,
        .pQueuePriorities = &queuePriority
    };

    // Now set up required device features
    // TODO - set this up properly
    VkPhysicalDeviceFeatures vkDeviceFeatures = { 0 };

    // Now create the actual device
    VkDeviceCreateInfo vkDevCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &vkCreateQueueInfo,
        .pEnabledFeatures = &vkDeviceFeatures,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL
    };
    
    VkResult devRes = vkCreateDevice( *dev, &vkDevCreateInfo, NULL, &vkDeviceCtx.logicalDevice );
    if( devRes != VK_SUCCESS ){
        LOG_ERR( "Failed to create logical device" );
        return 1;
    }
    LOG( "Logical device created" );
    vkGetDeviceQueue( vkDeviceCtx.logicalDevice, vkQueueFlagsReq[ 0 ].qIdx, 0, &vkDeviceCtx.mainGfxQueue );
    vkDeviceCtx.initialised = true;

    free( phyDevices );
    return 0;
}

VkQueue * pomDeviceGetGraphicsQueue(){
    
    return &vkDeviceCtx.mainGfxQueue;
}

int pomDestroyPhysicalDevice(){
    if( !vkDeviceCtx.initialised ){
        LOG_WARN( "Trying to destroy uninitiated device" );
        return 1;
    }
    vkDestroyDevice( vkDeviceCtx.logicalDevice, NULL );

    vkDeviceCtx.initialised = false;
    return 0;
}
