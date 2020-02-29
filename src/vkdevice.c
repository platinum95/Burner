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

// TODO - improve requirement system to encapsulate all requirements in a single struct
// TODO - improve requirement system to allow multiple devices
// TODO - improve requirement system to have weak requirements

typedef uint16_t (*VkQueueFeatureCheck)(VkPhysicalDevice*, uint32_t);

typedef struct VkQueueFamilyRequirement VkQueueFamilyRequirement;

struct VkQueueFamilyRequirement{
    VkQueueFlags flags;
    VkQueueFeatureCheck featureCheck;
    uint32_t minQueues;
    bool mayOverlap;
};

const char * const requiredExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
    // Require a queue for graphics calls
    {
        .flags = VK_QUEUE_GRAPHICS_BIT,
        .minQueues = 1,
        .mayOverlap = true,
        .featureCheck = NULL
    },
    // Require a presentation queue. May be the same
    // as the graphics queue.
    {
        .flags = 0x0,
        .minQueues = 1,
        .mayOverlap = true,
        .featureCheck = &gfxFeatureCheck
    }
};

// TODO - clean up all this struct stuff - maybe converge some of them
typedef struct VkQueueFamilyRequirementsCtx VkQueueFamilyRequirementsCtx;
typedef struct VkQueueFamilyCtx VkQueueFamilyCtx;
typedef struct VkQueueFamilyRequirementFound VkQueueFamilyRequirementFound;
typedef struct VkPhysicalDeviceCtx VkPhysicalDeviceCtx;
typedef struct VkDeviceCtx VkDeviceCtx;
typedef struct SwapchainInfo SwapchainInfo;

struct VkQueueFamilyRequirementsCtx{
    VkQueueFamilyRequirement *queueFamilyRequirements;
    uint32_t numRequirements;
};

VkQueueFamilyRequirementsCtx queueFamilyRequirementsCtx = {
    .numRequirements = sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ),
    .queueFamilyRequirements = queueFamRequirements
};

struct VkQueueFamilyCtx{
    VkQueueFamilyProperties *queueFamilyProperties;
    uint32_t numFamilies;
};

struct VkQueueFamilyRequirementFound{
    bool found;
    uint32_t reqIdx;
    uint32_t devQueueIdx;
};

struct SwapchainInfo{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    VkSurfaceFormatKHR *surfaceFormats;
    VkPresentModeKHR *presentModes;

    VkSurfaceFormatKHR chosenSurfaceFormat;
    VkPresentModeKHR chosenPresentMode;
    VkExtent2D extent;

    VkSwapchainKHR swapchain;

    VkImage *swapchainImages;
    uint32_t numSwapchainImages;

    bool initialised;
};

struct VkPhysicalDeviceCtx{
    VkPhysicalDevice phyDev;
    VkPhysicalDeviceFeatures phyDevFeatures;
    VkPhysicalDeviceProperties phyDevProps;
    VkQueueFamilyCtx queueFamiliesCtx;
    VkQueueFamilyRequirementFound queueReqFound[ sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ) ];
    SwapchainInfo swapchainInfo;
};

struct VkDeviceCtx{
    bool phyDeviceCreated;
    bool logicalDeviceCreated;
    VkPhysicalDeviceCtx physicalDeviceCtx;
    VkDevice logicalDevice;
    VkQueue mainGfxQueue;
};

VkDeviceCtx vkDeviceCtx = { 0 };

static uint16_t swapchainEligibility( VkPhysicalDeviceCtx *_phyDevCtx ){

    VkSurfaceKHR *surface = pomIoGetSurface();
    VkPhysicalDevice *phyDev = &_phyDevCtx->phyDev;
    // Get surface capabilities
    VkSurfaceCapabilitiesKHR *surfaceCaps = &_phyDevCtx->swapchainInfo.surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( *phyDev, *surface, surfaceCaps );

    // Get surface formats
    VkSurfaceFormatKHR *surfaceFormats = _phyDevCtx->swapchainInfo.surfaceFormats;
    uint32_t numFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR( *phyDev, *surface, &numFormats, NULL );
    if( !numFormats ){
        // No available surface formats, return
        return 0;
    }
    surfaceFormats = (VkSurfaceFormatKHR*) malloc( sizeof( VkSurfaceFormatKHR ) * numFormats );
    vkGetPhysicalDeviceSurfaceFormatsKHR( *phyDev, *surface, &numFormats, surfaceFormats );

    VkPresentModeKHR *presentModes = _phyDevCtx->swapchainInfo.presentModes;
    uint32_t numPresentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR( *phyDev, *surface, &numPresentModes, NULL );
    if( !numPresentModes ){
        // No available presentation modes
        free( surfaceFormats );
        return 0;
    }
    presentModes = (VkPresentModeKHR*) malloc( sizeof( VkPresentModeKHR ) * numPresentModes );
    vkGetPhysicalDeviceSurfacePresentModesKHR( *phyDev, *surface, &numPresentModes, presentModes );

    // Choose a format
    // TODO - improve format selection
    VkSurfaceFormatKHR *chosenFormat = &surfaceFormats[ 0 ];;
    for( uint32_t i = 0; i < numFormats; i++ ){
        VkSurfaceFormatKHR *cFormat = &surfaceFormats[ i ];
        if( cFormat->format == VK_FORMAT_B8G8R8A8_SRGB && 
            cFormat->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ){
            chosenFormat = cFormat;
            break;
        }
    }

    // Choose a presentation mode
    // TODO - improve presentation mode selection
    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for( uint32_t i = 0; i < numPresentModes; i++ ){
        VkPresentModeKHR cPresentMode = presentModes[ i ];
        if( cPresentMode == VK_PRESENT_MODE_MAILBOX_KHR ){
            chosenPresentMode = cPresentMode;
        }
    }

    // Set the extent
    VkExtent2D *chosenExtent = &_phyDevCtx->swapchainInfo.extent;
    if( surfaceCaps->currentExtent.width != UINT32_MAX ){
        // Swapchain extent must be set to surface resolution
        *chosenExtent = surfaceCaps->currentExtent;
    }else{
        // Set swapchain extent to window resolution, bound by
        // instance limits
        VkExtent2D min = surfaceCaps->minImageExtent;
        VkExtent2D max = surfaceCaps->maxImageExtent;
        *chosenExtent = *pomIoGetWindowExtent();
       
        chosenExtent->width = chosenExtent->width > max.width ? 
                                max.width : chosenExtent->width < min.width ?
                                min.width : chosenExtent->width;
        chosenExtent->height = chosenExtent->height > max.height ? 
                                max.height : chosenExtent->height < min.height ?
                                min.height : chosenExtent->height;
    }

    _phyDevCtx->swapchainInfo.chosenSurfaceFormat = *chosenFormat;
    _phyDevCtx->swapchainInfo.chosenPresentMode = chosenPresentMode;
    _phyDevCtx->swapchainInfo.initialised = true;

    return 1;
}


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
    
    // Search for required physical device extensions
    uint32_t devExtCount;
    vkEnumerateDeviceExtensionProperties( _phyDevCtx->phyDev, NULL, &devExtCount, NULL );
    VkExtensionProperties *devExtProps = (VkExtensionProperties*) malloc( sizeof( VkExtensionProperties ) * devExtCount );
    vkEnumerateDeviceExtensionProperties( _phyDevCtx->phyDev, NULL, &devExtCount, devExtProps );
    bool extFound[ sizeof( requiredExtensions ) / sizeof( const char * const ) ] = { false };
    for( uint32_t i = 0; i < sizeof( requiredExtensions ) / sizeof( const char * const ); i++ ){
        const char * const reqStr = requiredExtensions[ i ];

        for( uint32_t j = 0; j < devExtCount; j++ ){
            const char * const extStr = devExtProps[ j ].extensionName;
            if( strcmp( reqStr, extStr ) == 0 ){
                extFound[ i ] = true;
            }
        }
    }
    for( uint32_t i = 0; i < sizeof( requiredExtensions ) / sizeof( const char * const ); i++ ){
        if( !extFound[ i ] ){
            LOG_WARN( "Required extension(s) not present for device" );
            free( devExtProps );
            return 0;
        }
    }
    free( devExtProps );

    // Check for swapchain support
    uint16_t swapScore = swapchainEligibility( _phyDevCtx );

    if( !swapScore ){
        // Swapchain not eligible/not available
        return 0;
    }

    VkQueueFamilyRequirement *qReqs = queueFamilyRequirementsCtx.queueFamilyRequirements;
    // Loop for each queue family requirement
    for( uint32_t i = 0; i < queueFamilyRequirementsCtx.numRequirements; i++ ){
        VkQueueFamilyRequirement *qReq = &qReqs[ i ];
        VkQueueFamilyRequirementFound *qReqFound = &_phyDevCtx->queueReqFound[ i ];

        // Loop for each queue family in this device
        for( uint32_t qId = 0; qId < numQueueFamilies; qId++ ){
            VkQueueFamilyProperties *qFamProp = &qProps[ qId ];
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
    score += 1;
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
        if( phyDevCtxs[ i ].swapchainInfo.initialised ){
            free( phyDevCtxs[ i ].swapchainInfo.surfaceFormats );
            free( phyDevCtxs[ i ].swapchainInfo.presentModes );
        }
    }
    // Now free the array of all contexts
    free( phyDevCtxs );


    LOG( "Using device %s", devCtx->phyDevProps.deviceName );
    // Make sure the config is set with our chosen device
    pomMapSet( &systemConfig.mapCtx, "vulkan_device_name", devCtx->phyDevProps.deviceName );


    vkDeviceCtx.phyDeviceCreated = true;

    free( phyDevices );
    return 0;
}

int pomCreateLogicalDevice(){
    // At this point we have a physical device selected,
    // with a context for it that specifies the queue families
    // that are required and their indices.
    // Can remove duplicate indices as some requirements may
    // share a family.
    // Work out the unique queues required
    VkDeviceCtx *devCtx = &vkDeviceCtx;
    VkPhysicalDeviceCtx *phyDevCtx = &devCtx->physicalDeviceCtx;
    const int numReq = sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement );
    bool queueIdUnique[ sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ) ] = { true };
    for( int i = 0; i < numReq; i++ ){
        const VkQueueFamilyRequirementFound *queueReqFound = &phyDevCtx->queueReqFound[ i ];
        uint32_t qIdx = queueReqFound->devQueueIdx;
        for( int j = i+1; j < numReq; j++ ){
            if( phyDevCtx->queueReqFound[ i ].devQueueIdx == qIdx ){
                queueIdUnique[ j ] = false;
            }
        }
    }

    // TODO - queue lengths on requirement overlap
    
    // Set up the logical device
    // First specify the queues to be created
    float queuePriority = 1.0f;
    uint32_t queueFamilyCount = 0;
    VkDeviceQueueCreateInfo vkQueueCreateInfoBlock[ sizeof( queueFamRequirements ) / sizeof( VkQueueFamilyRequirement ) ] = { { 0 } };
    VkDeviceQueueCreateInfo * nextFreeQueueInfo = &vkQueueCreateInfoBlock[ 0 ];
    for( int i = 0; i < numReq; i++ ){
        const VkQueueFamilyRequirementFound *queueReqFound = &phyDevCtx->queueReqFound[ i ];
        if( !queueIdUnique[ i ] ){
            // No need to add this requirement's queue
            continue;
        }
        nextFreeQueueInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        nextFreeQueueInfo->queueCount = queueFamilyRequirementsCtx.queueFamilyRequirements[ queueReqFound->reqIdx ].minQueues;
        nextFreeQueueInfo->queueFamilyIndex = queueReqFound->devQueueIdx;
        nextFreeQueueInfo->pQueuePriorities = &queuePriority;
        
        queueFamilyCount++;
        // Point to next queue info struct in the block
        nextFreeQueueInfo = nextFreeQueueInfo + 1;
    }

    // Now set up required device features
    // TODO - set this up properly
    VkPhysicalDeviceFeatures vkDeviceFeatures = { 0 };

    // Now create the actual device
    VkDeviceCreateInfo vkDevCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queueFamilyCount,
        .pQueueCreateInfos = vkQueueCreateInfoBlock,
        .pEnabledFeatures = &vkDeviceFeatures,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = sizeof( requiredExtensions ) / sizeof( const char * const ),
        .ppEnabledExtensionNames = requiredExtensions

    };
    
    VkResult devRes = vkCreateDevice( phyDevCtx->phyDev, &vkDevCreateInfo, NULL, &vkDeviceCtx.logicalDevice );
    if( devRes != VK_SUCCESS ){
        LOG_ERR( "Failed to create logical device" );
        return 1;
    }
    LOG( "Logical device created" );
    vkGetDeviceQueue( vkDeviceCtx.logicalDevice, phyDevCtx->queueReqFound[ 0 ].devQueueIdx, 0, &vkDeviceCtx.mainGfxQueue );

    devCtx->logicalDeviceCreated = true;

    // Now create the swapchain
    SwapchainInfo *swapchainInfo = &vkDeviceCtx.physicalDeviceCtx.swapchainInfo;
    const VkSurfaceCapabilitiesKHR *surfaceCaps = &swapchainInfo->surfaceCaps;
    uint32_t imageCount = surfaceCaps->minImageCount + 1;
    if( imageCount > surfaceCaps->maxImageCount && surfaceCaps->maxImageCount != 0 ){
        imageCount = surfaceCaps->maxImageCount;
    }

    VkSwapchainCreateInfoKHR createSwapchain = { 0 };
    createSwapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createSwapchain.surface = *pomIoGetSurface();
    createSwapchain.minImageCount = imageCount;
    createSwapchain.imageFormat = swapchainInfo->chosenSurfaceFormat.format;
    createSwapchain.imageColorSpace = swapchainInfo->chosenSurfaceFormat.colorSpace;
    createSwapchain.imageExtent = swapchainInfo->extent;
    createSwapchain.imageArrayLayers = 1;
    createSwapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createSwapchain.preTransform = surfaceCaps->currentTransform;
    createSwapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createSwapchain.presentMode = swapchainInfo->chosenPresentMode;
    createSwapchain.clipped = VK_TRUE;
    createSwapchain.oldSwapchain = VK_NULL_HANDLE;

    // Check if we need concurrent access to this swapchain
    // TODO - do a proper check instead of just checking 1st and 2nd indices
    VkQueueFamilyRequirementFound *queueReqFound = &vkDeviceCtx.physicalDeviceCtx.queueReqFound[ 0 ];
    uint32_t queueFamilyIndices[] = { queueReqFound[ 0 ].devQueueIdx, queueReqFound[ 1 ].devQueueIdx };
    if( queueIdUnique[ 0 ] && queueIdUnique[ 1 ] ){
        // Swapchain will be accessed from 2 different queue families
        createSwapchain.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createSwapchain.queueFamilyIndexCount = 2;
        createSwapchain.pQueueFamilyIndices = queueFamilyIndices;
    }else{
        // Swapchain will be accessed from a single queue family
        createSwapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createSwapchain.queueFamilyIndexCount = 0;
        createSwapchain.pQueueFamilyIndices = NULL;    
    }

    if( vkCreateSwapchainKHR( vkDeviceCtx.logicalDevice,
                              &createSwapchain, NULL,
                              &swapchainInfo->swapchain ) != VK_SUCCESS ){
        LOG_ERR( "Unable to create swapchain" );
        return 1;
    }

    // Now get the swapchain images
    uint32_t numSwapchainImages = 0;
    vkGetSwapchainImagesKHR( vkDeviceCtx.logicalDevice, swapchainInfo->swapchain,
                             &numSwapchainImages, NULL );
    if( !numSwapchainImages ){
        LOG_ERR( "Could not get swapchain images" );
        return 1;
    }
    swapchainInfo->swapchainImages = (VkImage*) malloc( sizeof( VkImage ) * numSwapchainImages );
    vkGetSwapchainImagesKHR( vkDeviceCtx.logicalDevice, swapchainInfo->swapchain,
                             &numSwapchainImages, swapchainInfo->swapchainImages );

    swapchainInfo->numSwapchainImages = numSwapchainImages;

    return 0;
}

VkFormat * pomGetSwapchainImageFormat(){
    return &vkDeviceCtx.physicalDeviceCtx.swapchainInfo.chosenSurfaceFormat.format;
}


VkQueue * pomDeviceGetGraphicsQueue( uint32_t *idx ){
    // TODO - proper idx support
    *idx = 0;
    return &vkDeviceCtx.mainGfxQueue;
}

VkExtent2D * pomGetSwapchainExtent(){
    return &vkDeviceCtx.physicalDeviceCtx.swapchainInfo.extent;
}

VkImage * pomGetSwapchainImages( uint32_t *numImages ){
    if( !vkDeviceCtx.physicalDeviceCtx.swapchainInfo.initialised ){
        LOG_ERR( "Attempting to get uninitialised swapchain images" );
        return NULL;
    }
    *numImages = vkDeviceCtx.physicalDeviceCtx.swapchainInfo.numSwapchainImages;
    
    return vkDeviceCtx.physicalDeviceCtx.swapchainInfo.swapchainImages;
}

VkDevice * pomGetLogicalDevice(){
    // TODO - error check here
    return &vkDeviceCtx.logicalDevice;
}

int pomDestroyLogicalDevice(){
    if( !vkDeviceCtx.logicalDeviceCreated ){
        LOG_WARN( "Trying to destroy uninitialised logical device" );
        return 1;
    }

    free( vkDeviceCtx.physicalDeviceCtx.swapchainInfo.swapchainImages );
    vkDestroySwapchainKHR( vkDeviceCtx.logicalDevice, vkDeviceCtx.physicalDeviceCtx.swapchainInfo.swapchain, NULL );
    vkDestroyDevice( vkDeviceCtx.logicalDevice, NULL );

    vkDeviceCtx.logicalDeviceCreated = false;

    return 0;
}

int pomDestroyPhysicalDevice(){
    if( !vkDeviceCtx.phyDeviceCreated ){
        LOG_WARN( "Trying to destroy uninitiated device" );
        return 1;
    }
    
    
    // Destroy the surface that we may have initialised
    pomIoDestroySurface();

    vkDeviceCtx.phyDeviceCreated = false;
    return 0;
}
