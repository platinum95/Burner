#include <vulkan/vulkan.h>
#include "common.h"
#include <string.h>
#include <stdbool.h>
#include "pomIO.h"
#include "hashmap.h"
#include <stdlib.h>
#include "config.h"

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, vkinstance, log, ##__VA_ARGS__ )
#define LOG_WARN( log, ... ) LOG_MODULE( WARN, vkinstance, log, ##__VA_ARGS__ )

typedef struct PomVkCtx PomVkCtx;

struct PomVkCtx{
    bool initialised;
    VkInstance instance;
    VkApplicationInfo appInfo;
    VkInstanceCreateInfo createInfo;
    PomMapCtx extMap;
};

PomVkCtx pomVkCtx = { 0 };

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

    createInfo->enabledLayerCount = 0;
    
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
    createInfo->enabledExtensionCount = glfwExtCount;
    createInfo->ppEnabledExtensionNames = windowRequiredExt;

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
        pomMapSet( &vkAvailValidLayersMap, prop->layerName, "" );
    }
    free( validProps );
    
    // Get requested validation layers
    // TODO - check if we have 0 requested validation layers
    const char * reqVLayersConf = pomMapGetSet( &systemConfig.mapCtx, "vk_validation", DEFAULT_VK_VALIDATION_LAYERS );
    // Make a copy of the string so we can mess about with its contents
    size_t reqVLayersLen = strlen( reqVLayersConf ) + 1;
    char * reqVLayers = (char*) malloc( sizeof( char ) * reqVLayersLen );
    //strcpy( reqVLayers, reqVLayersConf );

    // Find commas (specifying separators)
    // TODO - handle whitespace (ignore it)
    uint16_t lastStart = 0, safeCpy = 0;
    uint32_t numLayers = 0;
    for( uint16_t i = 0; i < reqVLayersLen; i++ ){
        const char *c = &reqVLayersConf[ i ];
        if( *c == ',' || *c == '\0' ){
            // Reached a separator or end of line
            //c = NULL;
            // Copy from lastStart to i into our new mutable string
            char * currStr = &reqVLayers[ safeCpy ];
            const char * currStrConf = &reqVLayersConf[ lastStart ];
            size_t layerStrMemLen = i - lastStart + 1;
            memcpy( currStr, currStrConf, layerStrMemLen * sizeof( char ) );
            uint16_t currStrEnd = safeCpy + layerStrMemLen - 1;
            reqVLayers[ currStrEnd ] = '\0';

            // Assume the next validation layer starts on the next character
            lastStart = i + 1;

            // Check if the requested layer is available
            if( pomMapGet( &vkAvailValidLayersMap, currStr, NULL ) ){
                // Validation layer is available, so keep it in our list
                safeCpy = currStrEnd + 1;
                numLayers++;
            }
            else{
                // Layer is not available
                LOG_WARN( "Validation layer %s is not available", currStr );
                // Keep safeCpy where it is so next valid layer will be copied there
            }
        }
    }
    const char ** layers =(const char**) &reqVLayers;
    // reqVLayers is a null-separated array of strings
    createInfo->ppEnabledLayerNames = layers;
    createInfo->enabledExtensionCount = numLayers;
    
    VkInstance *instance = &pomVkCtx.instance;
    VkResult res = vkCreateInstance( createInfo, NULL, instance );

    free( reqVLayers );

    pomVkCtx.initialised = ( res == VK_SUCCESS );
    return res == VK_SUCCESS ? 0 : 1;
}

int pomDestroyVkInstance(){
    if( !pomVkCtx.initialised ){
        LOG( "Attempt to destroy uninitialised Vk instance" );
        return 1;
    }
    vkDestroyInstance( pomVkCtx.instance, NULL );
    
    // Destroy IO since we may have initialised it
    pomIoDestroy();

    pomMapClear( &pomVkCtx.extMap );
    return 0;
}
