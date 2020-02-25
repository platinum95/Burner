#include "vkdevice.h"
#include "common.h"
#include <stdint.h>
#include "vkinstance.h"
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include <string.h>
#include "pomIO.h"

#define LOG( log, ... ) LOG_MODULE( DEBUG, vkdevice, log, ##__VA_ARGS__ )
#define LOG_WARN( log, ... ) LOG_MODULE( WARN, vkdevice, log, ##__VA_ARGS__ )
#define LOG_ERR( log, ... ) LOG_MODULE( ERR, vkdevice, log, ##__VA_ARGS__ )

typedef uint16_t (*VkQueueFeatureCheck)(VkPhysicalDevice*, uint32_t);

typedef struct VkQueueFamilyRequirement VkQueueFamilyRequirement;

struct VkQueueFamilyRequirement{
    VkQueueFlags flags;
    VkQueueFeatureCheck featureCheck;
    uint32_t minQueues;
    bool mayOverlap;
};

uint16_t gfxFeatureCheck( VkPhysicalDevice *_phyDev, uint32_t qIdx ){
    // Check if the queue can do presentation
    VkSurfaceKHR *surface = pomIoGetSurface();
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR( *_phyDev, qIdx, *surface, &presentSupport );
    if( !presentSupport ){
        // Queue doesn't support presentation
        return 0;
    }
    return 1;
}

VkQueueFamilyRequirement queueFamRequirements[] = {
    {
        .flags = VK_QUEUE_GRAPHICS_BIT,
        .minQueues = 1,
        .mayOverlap = true,
        .featureCheck = &gfxFeatureCheck,
    }
};



typedef struct VkQueueFamilyRequirementsCtx VkQueueFamilyRequirementsCtx;
struct VkQueueFamilyRequirementsCtx{
    VkQueueFamilyRequirement *queueFamilyRequirements;
    uint32_t numRequirements;
};

VkQueueFamilyRequirementsCtx queueFamilyRequirementsCtx = {
    .numRequirements = sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ),
    .queueFamilyRequirements = queueFamRequirements
};

typedef struct VkQueueFamilyCtx VkQueueFamilyCtx;
struct VkQueueFamilyCtx{
    VkQueueFamilyProperties *queueFamilyProperties;
    uint32_t numFamilies;
};

typedef struct VkQueueFamilyRequirementFound VkQueueFamilyRequirementFound;
struct VkQueueFamilyRequirementFound{
    bool found;
    uint32_t reqIdx;
    uint32_t devQueueIdx;
};

typedef struct VkPhysicalDeviceCtx VkPhysicalDeviceCtx;
struct VkPhysicalDeviceCtx{
    VkPhysicalDevice phyDev;
    VkPhysicalDeviceFeatures phyDevFeatures;
    VkPhysicalDeviceProperties phyDevProps;
    VkQueueFamilyCtx queueFamiliesCtx;
    VkQueueFamilyRequirementFound queueReqFound[ sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ) ];
};

typedef struct VkDeviceCtx VkDeviceCtx;
struct VkDeviceCtx{
    bool initialised;
    VkPhysicalDeviceCtx physicalDeviceCtx;
    VkDevice logicalDevice;
    VkQueue mainGfxQueue;
};

VkDeviceCtx vkDeviceCtx = { 0 };


// Return 0 if ineligible, and a score >0 if eligible
static uint16_t phyDeviceElgibility( VkPhysicalDeviceCtx *_phyDevCtx ){
    // TODO - expand this function to actually score the device
    uint16_t score = 0;
    VkPhysicalDeviceFeatures *UNUSED(deviceFeatures) = &_phyDevCtx->phyDevFeatures;
    uint32_t numQueueFamilies = _phyDevCtx->queueFamiliesCtx.numFamilies;
    VkQueueFamilyProperties *qProps = _phyDevCtx->queueFamiliesCtx.queueFamilyProperties;
    VkPhysicalDeviceProperties *deviceProps = &_phyDevCtx->phyDevProps;
    VkPhysicalDevice *dev = &_phyDevCtx->phyDev;


    // TODO - implement proper device-type checking
    if( deviceProps->deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ){
        return 0;
    }

    VkQueueFamilyRequirement *qReqs = queueFamilyRequirementsCtx.queueFamilyRequirements;
    // Loop for each queue family requirement
    for( uint32_t i = 0; i < queueFamilyRequirementsCtx.numRequirements; i++ ){
        VkQueueFamilyRequirement *qReq = &qReqs[ i ];
        VkQueueFamilyRequirementFound *qReqFound = &_phyDevCtx->queueReqFound[ i ];

        // Loop for each queue family in this device
        for( uint32_t qId = 0; qId < numQueueFamilies; qId++ ){
            VkQueueFamilyProperties *qFamProp = &qProps[ i ];
            VkQueueFamilyRequirementFound *qFamFound = &_phyDevCtx->queueReqFound[ qId ];
            // Check for each requirement

            // First check if this queue is already in use for another requirement
            // and overlapping on this requirement (or the other) isn't allowed.
            // Check also for queue flags and queue count requirements.
            if( ( qFamFound->found && !qReq->mayOverlap ) ||
                ( qFamFound->found && !qReqs[ qFamFound->reqIdx ].mayOverlap ) ||
                ( ( qFamProp->queueFlags & qReq->flags ) != qReq->flags ) ||
                ( qFamProp->queueCount < qReq->minQueues ) ){
                
                // Queue family does not meet requirements
                continue;
            }

            // Check that the family has the required features
            // TODO - check for features
            if( qReq->featureCheck ){
                uint16_t featureScore = qReq->featureCheck( dev, qId );
                if( featureScore == 0 ){
                    // Required features not available with this queue family
                    continue;
                }
                score += featureScore;
            }

            // All requirements met, set the "found" struct for this device
            qReqFound->found = true;
            qReqFound->devQueueIdx = qId;
            qReqFound->reqIdx = i;
        }
    }

    // Check that we've found all required flags
    for( uint32_t i = 0; i < queueFamilyRequirementsCtx.numRequirements; i++ ){
        if( !_phyDevCtx->queueReqFound[ i ].found ){
            LOG_WARN( "Device %s does not support all required queue families", deviceProps->deviceName );
            return 0;
        }
    }
    
    return score;
}

int pomPickPhysicalDevice(){
    VkInstance *instance = pomGetVkInstance();
    if( !instance ){
        // VK not instantiated
        LOG_ERR( "Vulkan Instance not instantiated" );
        return 1;
    }
    // Make sure a render surface has been created before we continue
    pomIoCreateSurface();

    uint32_t phyDevCount;
    vkEnumeratePhysicalDevices( *instance, &phyDevCount, NULL );
    if( phyDevCount == 0 ){
        LOG_ERR( "Could not find any compatible devices" );
        return 1;
    }
    LOG( "Found %i Vulkan device(s)", phyDevCount );
    VkPhysicalDevice *phyDevices = (VkPhysicalDevice*) malloc( sizeof( VkPhysicalDevice ) * phyDevCount );
    vkEnumeratePhysicalDevices( *instance, &phyDevCount, phyDevices );

    // Create a PomPhysicalDeviceCtx for each potential device.
    // When we've chosen one, copy it to the main static context and free this block.
    VkPhysicalDeviceCtx *phyDevCtxs = (VkPhysicalDeviceCtx*) malloc( sizeof( VkPhysicalDeviceCtx ) *phyDevCount );

    // See if we're supposed to find a particular device
    const char * requestedDev = pomMapGet( &systemConfig.mapCtx, "vulkan_device_name", NULL );
    // Rate each available device and choose best
    uint16_t hiScore = 0, hiIdx = 0;
    for( uint32_t i = 0; i < phyDevCount; i++ ){
        // Populate the context
        VkPhysicalDevice *dev = &phyDevices[ i ];
        VkPhysicalDeviceProperties devProps;
        vkGetPhysicalDeviceProperties( *dev, &devProps );
        VkPhysicalDeviceFeatures devFeats;
        vkGetPhysicalDeviceFeatures( *dev, &devFeats );
        uint32_t qFamCount;
        vkGetPhysicalDeviceQueueFamilyProperties( *dev, &qFamCount, NULL );
        VkQueueFamilyProperties *qProps = (VkQueueFamilyProperties*) malloc( sizeof( VkQueueFamilyProperties ) * qFamCount );
        vkGetPhysicalDeviceQueueFamilyProperties( *dev, &qFamCount, qProps );

        phyDevCtxs[ i ].phyDev = *dev;
        phyDevCtxs[ i ].phyDevProps = devProps;
        phyDevCtxs[ i ].phyDevFeatures = devFeats;
        phyDevCtxs[ i ].queueFamiliesCtx.queueFamilyProperties = qProps;
        phyDevCtxs[ i ].queueFamiliesCtx.numFamilies = qFamCount;

        uint16_t devScore = phyDeviceElgibility( &phyDevCtxs[ i ] );
        if( requestedDev && strcmp( devProps.deviceName, requestedDev ) == 0 ){
            // Found the requested device, check if its suitable
            if( devScore == 0 ){
                LOG_WARN( "Requested device %s does not meet requirements", requestedDev );
                continue;
            }
            LOG( "Using config requested device" );
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
        // TODO - free memory here
        return 1;
    }
    
    // Copy our chosen device ctx over to main static context
    VkPhysicalDeviceCtx *devCtx = &phyDevCtxs[ hiIdx ];
    vkDeviceCtx.physicalDeviceCtx = *devCtx;
    devCtx = &vkDeviceCtx.physicalDeviceCtx;
    // Free up the queue family property arrays for all devices (except the chosen one)
    for( uint32_t i = 0; i < phyDevCount; i++ ){
        if( i == hiIdx ){
            continue;
        }
        free( phyDevCtxs[ i ].queueFamiliesCtx.queueFamilyProperties );
    }
    // Now free the array of all contexts
    free( phyDevCtxs );


    LOG( "Using device %s", devCtx->phyDevProps.deviceName );
    // Make sure the config is set with our chosen device
    pomMapSet( &systemConfig.mapCtx, "vulkan_device_name", devCtx->phyDevProps.deviceName );

    // At this point we have a physical device selected,
    // with a context for it that specifies the queue families
    // that are required and their indices.
    // Can remove duplicate indices as some requirements may
    // share a family.

    // Set up the logical device
    // First specify the queues to be created
    // TODO - extend this to take the queue requirements into account - ie create the queues that were requested
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo vkCreateQueueInfo ={
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .queueFamilyIndex = devCtx->queueReqFound[ 0 ].devQueueIdx,
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
    
    VkResult devRes = vkCreateDevice( devCtx->phyDev, &vkDevCreateInfo, NULL, &vkDeviceCtx.logicalDevice );
    if( devRes != VK_SUCCESS ){
        LOG_ERR( "Failed to create logical device" );
        return 1;
    }
    LOG( "Logical device created" );
    vkGetDeviceQueue( vkDeviceCtx.logicalDevice, devCtx->queueReqFound[ 0 ].devQueueIdx, 0, &vkDeviceCtx.mainGfxQueue );
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
    
    // Destroy the surface that we may have initialised
    pomIoDestroySurface();

    vkDeviceCtx.initialised = false;
    return 0;
}
