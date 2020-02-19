#include <vulkan/vulkan.h>
#include "common.h"
#include <string.h>
#include <stdbool.h>
#include "pomIO.h"
#include "hashmap.h"
#include <stdlib.h>

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, vkinstance, log, ##__VA_ARGS__ )

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

    // TODO - make sure the GLFW extensions are supported (i.e. are in the map)
    createInfo->enabledExtensionCount = glfwExtCount;
    createInfo->ppEnabledExtensionNames = windowRequiredExt;

    VkInstance *instance = &pomVkCtx.instance;
    VkResult res = vkCreateInstance( createInfo, NULL, instance );

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
