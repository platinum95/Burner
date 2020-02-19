#include <vulkan/vulkan.h>
#include "common.h"
#include <string.h>
#include <stdbool.h>
#include "pomIO.h"
#include "hashmap.h"
#include <stdlib.h>
#include "config.h"
#include "stack.h"

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, vkinstance, log, ##__VA_ARGS__ )
#define LOG_WARN( log, ... ) LOG_MODULE( WARN, vkinstance, log, ##__VA_ARGS__ )
#define LOG_ERR( log, ... ) LOG_MODULE( ERR, vkinstance, log, ##__VA_ARGS__ )

typedef struct PomVkCtx PomVkCtx;

struct PomVkCtx{
    bool initialised;
    VkInstance instance;
    VkApplicationInfo appInfo;
    VkInstanceCreateInfo createInfo;
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    VkDebugUtilsMessengerEXT debugMessenger;
    PomMapCtx extMap;
};

PomVkCtx pomVkCtx = { 0 };

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT UNUSED(messageSeverity),
    VkDebugUtilsMessageTypeFlagsEXT UNUSED(messageType),
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* UNUSED(pUserData)) {

        LOG( "validation layer: %s", pCallbackData->pMessage );

        return VK_FALSE;
}


int pomCreateVkInstance() {
    // Make sure IO has been initialised
    pomIoInit();

    VkApplicationInfo *appInfo = &pomVkCtx.appInfo;
    appInfo->sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    uint32_t aVersion = VK_MAKE_VERSION( BURNER_VERSION_MAJOR,
                                         BURNER_VERSION_MINOR,
                                         BURNER_VERSION_PATCH );
    appInfo->pApplicationName = BURNER_NAME;
    appInfo->pEngineName = BURNER_NAME;
    appInfo->applicationVersion = aVersion;
    appInfo->engineVersion = aVersion;
    appInfo->apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo *createInfo = &pomVkCtx.createInfo;
    createInfo->sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo->pApplicationInfo = appInfo;

    // Get and set Vk extensions required by the window manager
    uint32_t glfwExtCount;
    const char ** windowRequiredExt = pomIoGetWindowExtensions( &glfwExtCount );

    // Now find the additional extensions required by the program
    const char * extraExts[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    size_t numExtraExt = sizeof( extraExts ) / sizeof( extraExts[ 0 ] );
    uint32_t totalExtCount = glfwExtCount + numExtraExt;
    const char ** totalExts = (const char**) malloc( sizeof( char * ) * totalExtCount );
    
    // Copy all extensions into the overall list
    for( uint32_t i = 0; i < glfwExtCount; i++ ){
        totalExts[ i ] = windowRequiredExt[ i ];
    }
    for( uint32_t i = 0; i < numExtraExt; i++ ){
        totalExts[ glfwExtCount + i ] = extraExts[ i ];
    }
    
    // TODO - error check
    uint32_t vkAvailExt = 0;
    vkEnumerateInstanceExtensionProperties( NULL, &vkAvailExt, NULL );

    VkExtensionProperties *extProps = (VkExtensionProperties*) malloc( sizeof( VkExtensionProperties ) * vkAvailExt );
    vkEnumerateInstanceExtensionProperties( NULL, &vkAvailExt, extProps );
    
    // Populate a hashmap with available extensions
    pomMapInit( &pomVkCtx.extMap, vkAvailExt );

    for( uint32_t i = 0; i < vkAvailExt; i++ ){
        // Add each extension to the map
        VkExtensionProperties *prop = &extProps[ i ];
        const char * extName = prop->extensionName;
        uint32_t extVer = prop->specVersion;
        // TODO - make this nicer
        char buf[ 50 ];
        sprintf( buf, "%u", extVer );

        pomMapSet( &pomVkCtx.extMap, extName, buf );
    }
    free( extProps );

    // TODO - make sure the GLFW extensions are supported (i.e. are in the map)
    createInfo->enabledExtensionCount = totalExtCount;
    createInfo->ppEnabledExtensionNames = totalExts;

    // Get available validation layers
    // TODO - handle 0 avail props
    uint32_t vkAvailValid = 0;
    vkEnumerateInstanceLayerProperties( &vkAvailValid, NULL );
    VkLayerProperties *validProps = (VkLayerProperties*) malloc( sizeof( VkLayerProperties ) * vkAvailValid );
    vkEnumerateInstanceLayerProperties( &vkAvailValid, validProps );
    PomMapCtx vkAvailValidLayersMap;
    pomMapInit( &vkAvailValidLayersMap, vkAvailValid );
    for( uint32_t i = 0; i < vkAvailValid; i++ ){
        VkLayerProperties * prop = &validProps[ i ];
        pomMapSet( &vkAvailValidLayersMap, prop->layerName, "0" );
    }
    free( validProps );
    
    // Get requested validation layers
    // TODO - check if we have 0 requested validation layers
    const char * reqVLayersConf = pomMapGetSet( &systemConfig.mapCtx, "vk_validation", DEFAULT_VK_VALIDATION_LAYERS );
    
    // Make a copy of the string so we can change separators to null-terminators
    size_t reqVLayersLen = strlen( reqVLayersConf ) + 1;
    char * reqVLayers = (char*) malloc( sizeof( char ) * reqVLayersLen );
    strcpy( reqVLayers, reqVLayersConf );

    // Create a new array to place the string pointers into
    const char ** reqVLayerList = malloc( sizeof( char* ) * vkAvailValid );

    // Find commas (specifying separators)
    // TODO - handle whitespace (ignore it)
    uint16_t lastStart = 0;
    uint32_t numLayers = 0;
    for( uint16_t i = 0; i < reqVLayersLen; i++ ){
        const char *c = &reqVLayers[ i ];
        if( *c == ',' || *c == '\0' ){
            // Reached a separator or end of line
            c = NULL;
            // Copy from lastStart to i into our new mutable string
            char * currStr = &reqVLayers[ lastStart ];

            // Assume the next validation layer starts on the next character
            lastStart = i + 1;

            // Check if the requested layer is available
            const char * vLayerStatus = pomMapGet( &vkAvailValidLayersMap, currStr, NULL );
            if( vLayerStatus ){
                // Check if we've already added this layer
                if( strcmp( vLayerStatus, "0" ) == 0 ){
                    // Add the layer to the array
                    // TODO - check for array OOB here
                    reqVLayerList[ numLayers ] = currStr;
                    numLayers++;
                    pomMapSet( &vkAvailValidLayersMap, currStr, "1" );
                }else{
                    // Layer is already added, do nothing
                    ;
                }
            }
            else{
                // Layer is not available
                LOG_WARN( "Validation layer %s is not available", currStr );
                // Keep safeCpy where it is so next valid layer will be copied there
            }
        }
    }

    createInfo->ppEnabledLayerNames = reqVLayerList;
    createInfo->enabledLayerCount = numLayers;

    VkInstance *instance = &pomVkCtx.instance;

    // Set up debug callback
    VkDebugUtilsMessengerCreateInfoEXT *dbCreateInfo = &pomVkCtx.debugCreateInfo;
    memset( dbCreateInfo, 0, sizeof( VkDebugUtilsMessengerCreateInfoEXT ) );

    dbCreateInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbCreateInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbCreateInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbCreateInfo->pfnUserCallback = debugCallback;
    dbCreateInfo->pUserData = NULL; // Optional

    createInfo->pNext = dbCreateInfo;

    VkResult res = vkCreateInstance( createInfo, NULL, instance );
    if( res != VK_SUCCESS ){
        LOG_ERR( "Failed to create Vulkan instance" );
    }

    // Now set up debug callback
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT) 
        vkGetInstanceProcAddr( *instance, "vkCreateDebugUtilsMessengerEXT" );
    
    if ( func ) {
        func( *instance, dbCreateInfo, NULL, &pomVkCtx.debugMessenger );
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    // TODO - defer this freeing
    free( reqVLayers );
    free( reqVLayerList );
    free( totalExts );

    pomMapClear( &vkAvailValidLayersMap );

    pomVkCtx.initialised = ( res == VK_SUCCESS );
    return res == VK_SUCCESS ? 0 : 1;
}

int pomDestroyVkInstance(){
    if( !pomVkCtx.initialised ){
        LOG( "Attempt to destroy uninitialised Vk instance" );
        return 1;
    }
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) 
        vkGetInstanceProcAddr( pomVkCtx.instance, "vkDestroyDebugUtilsMessengerEXT" );
    if ( func ) {
        func( pomVkCtx.instance, pomVkCtx.debugMessenger, NULL );
    }
    else{
        LOG_ERR( "Could not destroy debug messenger" );
    }
    vkDestroyInstance( pomVkCtx.instance, NULL );
    
    // Destroy IO since we may have initialised it
    pomIoDestroy();

    pomMapClear( &pomVkCtx.extMap );
    return 0;
}
