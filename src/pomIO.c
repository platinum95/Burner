#include "pomIO.h"
#include "common.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "vkinstance.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define LOG( log, ... ) LOG_MODULE( DEBUG, PomIO, log, ##__VA_ARGS__ )
#define LOG_ERR( log, ... ) LOG_MODULE( ERROR, PomIO, log, ##__VA_ARGS__ )
#define LOG_WARN( log, ... ) LOG_MODULE( WARN, PomIO, log, ##__VA_ARGS__ )

typedef struct PomIoCtx PomIoCtx;

struct PomIoCtx{
    bool initialised;
    VkExtent2D windowExtent;
    GLFWwindow *window;
    bool surfaceInitialised;
    VkSurfaceKHR renderSurface;
};

PomIoCtx pomIoCtx = { 0 };

// TODO - remove
GLFWwindow *getWindow(){
    return pomIoCtx.window;
}

// Initialise IO given values in the system configuration
int pomIoInit(){
    if( pomIoCtx.initialised ){
        LOG_WARN( "Attempting to reinitialise IO" );
        return 1;
    }
    if( !glfwInit() ){
        LOG_ERR( "Failed to start GLFW" );
        return 2;
    }

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

    const char * cwidth = pomMapGetSet( &systemConfig.mapCtx, "window_width", DEFAULT_WIDTH );
    const char * cheight = pomMapGetSet( &systemConfig.mapCtx, "window_height", DEFAULT_HEIGHT );
    int width = atoi( cwidth );
    int height = atoi( cheight );

    if( !( width && height ) ){
        // One of the values failed to convert
        if( !( strcmp( cwidth, DEFAULT_WIDTH ) || strcmp( cheight, DEFAULT_HEIGHT ) ) ){
            // str->int conversion failed for the default (compile-time) values.
            // This means there's a problem with the compile-time values.
            // Revert to final fallback.
            width = 800;
            height = 600;
            LOG_WARN( "Compile-time defaults for window size are invalid" );
        }
        else{
            // Parse the defined compile-time default values
            width = atoi( DEFAULT_WIDTH );
            height = atoi( DEFAULT_HEIGHT );
            // Check the values
            if( !( width && height ) ){
                width = 800;
                height = 600;
            }
        }
        LOG( "Resetting config values for window size" );
        // Set the config with parsable size values
        char buf[10];
        sprintf( buf, "%u", width );
        pomMapSet( &systemConfig.mapCtx, "window_width", buf );
        sprintf( buf, "%u", height );
        pomMapSet( &systemConfig.mapCtx, "window_height", buf );
    }
    
    // Create the GLFW context
    pomIoCtx.window = glfwCreateWindow( width, height, BURNER_NAME, NULL, NULL );

    pomIoCtx.windowExtent = (VkExtent2D){
        .width = width,
        .height = height
    };

    pomIoCtx.initialised = true;
    return 0;
}

// Clean up any IO-related items
int pomIoDestroy(){
    if( !pomIoCtx.initialised ){
        LOG_WARN( "Trying to destroy uninitialised IO" );
        return 1;
    }
    glfwDestroyWindow( pomIoCtx.window );
    glfwTerminate();
    return 0;
}

// Reset the IO (mainly window) with new configuration values
int pomIoReset(){
    // TODO - implement this
    return 0;
}

// Return the extensions required by the window handler as an
// array of strings. Number of extensions stored in _ecount
const char ** pomIoGetWindowExtensions( uint32_t * _ecount ){
    if( !pomIoCtx.initialised ){
        LOG_ERR( "Attempting to get extensions for uninitialised window" );
        *_ecount = 0;
        return NULL;
    }
    return glfwGetRequiredInstanceExtensions( _ecount );
}

const VkExtent2D *pomIoGetWindowExtent(){
    return &pomIoCtx.windowExtent;
}


// Check if the window manager is telling us to close
bool pomIoShouldClose(){
    return glfwWindowShouldClose( pomIoCtx.window );
}

// Poll for IO events
int pomIoPoll(){
    glfwPollEvents();
    return 0;
}

// Create the rendering surface
int pomIoCreateSurface(){
    if( pomIoCtx.surfaceInitialised ){
        LOG_WARN( "Surface already initialised" );
    }
    VkInstance *inst = pomGetVkInstance();
    if( !inst ){
        LOG_ERR( "Cannot create surface with uninitialised Vk Instance" );
        return 1;
    }
    VkResult res = glfwCreateWindowSurface( *inst, pomIoCtx.window, NULL, &pomIoCtx.renderSurface );

    if( res != VK_SUCCESS ){
        LOG_ERR( "Failed to create render surface" );
        return 1;
    }
    pomIoCtx.surfaceInitialised = true;
    return 0;
}

// Get the render surface
VkSurfaceKHR* pomIoGetSurface(){
    if( !pomIoCtx.surfaceInitialised ){
        LOG_ERR( "Attempting to get uninitialised render surface" );
        return NULL;
    }
    return &pomIoCtx.renderSurface;
}

// Destroy the rendering surface
int pomIoDestroySurface(){
    if( !pomIoCtx.surfaceInitialised ){
        LOG_ERR( "Attempting to destroy uninitialised render surface" );
        return 1;
    }
    VkInstance *inst = pomGetVkInstance();
    if( !inst ){
        LOG_ERR( "Cannot destroy surface with uninitialised Vk Instance" );
        return 1;
    }
    vkDestroySurfaceKHR( *inst, pomIoCtx.renderSurface, NULL );
    return 0;
}