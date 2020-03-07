#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "system_hw.h"
#include "cmore/hashmap.h"
#include "common.h"
#include "vkinstance.h"
#include "vkdevice.h"
#include "vkpipeline.h"
#include "vkpresentation.h"
#include "vkcommands.h"
#include "pomIO.h"
#include "vksynchronisation.h"
#include "cmore/threadpool.h"
#include "pomModelFormat.h"

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, main, log, ##__VA_ARGS__ )
typedef struct VulkanCtx VulkanCtx;
struct VulkanCtx{
    ShaderInfo basicShaders;
    VkRenderPass renderPass;
    PomPipelineCtx pipelineCtx;
    PomSemaphoreCtx imageSemaphore, renderSemaphore;
    bool initialised;
};

// TODO - maybe move this into pomModelFormat.h
typedef struct PomModelCtx PomModelCtx;
struct PomModelCtx{
    PomModelFormat *format;
    uint8_t *dataBlock;
    const char *filePath;
    bool initialised;
};

const char *modelPaths[] = {
    "./res/models/nanosuit.pomf",
    "./res/models/buddha.pomf",
    "./res/models/barrel.pomf"
};
const size_t numModelPaths = sizeof( modelPaths ) / sizeof( char* );

void setupVulkan( void* _userData );
void loadModel( void* _userData );

int main( int argc, char ** argv ){
    LOG( "Program Entry" );

    char * configPath;
    // Single argument pointing to config file,
    // or no argument for default
    if( argc == 2 ){
        configPath = argv[ 1 ];
    }
    else{
        configPath = DEFAULT_CONFIG_PATH;
    }
    LOG( "Config file: %s", configPath );


    if( loadSystemConfig( configPath ) ){
        printf( "Error loading configuration file\n" );
    }

    LOG( "Create Threadpool" );
    uint8_t numThreads = atoi( pomMapGetSet( &systemConfig.mapCtx, CONFIG_NUMTHREAD_KEY, DEFAULT_NUMTHREADS ) );
    PomThreadpoolCtx threadpoolCtx = { 0 };
    pomThreadpoolInit( &threadpoolCtx, numThreads );
    VulkanCtx vCtx = { 0 };
    
    // Keep some of these variables within a single scope to save a bit of stack space.
    // TODO - maybe move the whole setup stuff to a separate function altogether
    {
    // Schedule all models to be loaded
    PomModelCtx models[ sizeof( modelPaths ) / sizeof( char* ) ] = { 0 };
    PomThreadpoolJob jobs[ sizeof( modelPaths ) / sizeof( char* ) ] = { 0 };
    for( uint32_t i = 0; i < numModelPaths; i++ ){
        PomModelCtx *model = &models[ i ];
        PomThreadpoolJob *job = &jobs[ i ];
        model->filePath = modelPaths[ i ];
        //loadModel( &models[ i ] );
        job->args = model;
        job->func = loadModel;
        pomThreadpoolScheduleJob( &threadpoolCtx, job );
    }

    // Schedule vulkan context to be set up
    PomThreadpoolJob vkSetupJob = { .func=setupVulkan, .args=&vCtx };
    pomThreadpoolScheduleJob( &threadpoolCtx, &vkSetupJob );

    // Wait for jobs to complete
    pomThreadpoolJoinAll( &threadpoolCtx );

    // Verify all models were correctly loaded
    for( uint32_t i = 0; i < numModelPaths; i++ ){
        if( !models[ i ].initialised ){
            LOG( "Failed to load model %s", models[ i ].filePath );
            // TODO - error handling here
        }
    }
    } //endscope
    LOG( "Models loaded" );

    uint32_t numCommandBuffers;
    uint32_t gfxQueueIdx, presentQueueIdx;
    VkCommandBuffer *commandBuffers = pomCommandBuffersGet( &numCommandBuffers );
    VkDevice *dev = pomGetLogicalDevice();
    VkSwapchainKHR *swapchain = pomGetSwapchain();
    VkQueue *gfxQueue = pomDeviceGetGraphicsQueue( &gfxQueueIdx );
    VkQueue *presentQueue = pomDeviceGetPresentQueue( &presentQueueIdx );
    // Main loop
    while( !pomIoShouldClose() ){
        pomIoPoll();
        // Draw a frame
        uint32_t imageIndex;
        vkAcquireNextImageKHR( *dev, *swapchain, UINT64_MAX,
                               vCtx.imageSemaphore.semaphore, NULL, &imageIndex );

        if( imageIndex >= numCommandBuffers ){
            LOG( "Discrepency between swapchain image index and command buffer size.\
                  Requesting index %i for buffer size %i", imageIndex, numCommandBuffers );
            break;
        }
        VkSubmitInfo drawSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitSemaphores = (VkSemaphore[]){ vCtx.imageSemaphore.semaphore },
            .waitSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]){ vCtx.renderSemaphore.semaphore },
            .signalSemaphoreCount = 1,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[ imageIndex ],
            .pWaitDstStageMask = (VkPipelineStageFlags[]){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
        };
        
        if( vkQueueSubmit( *gfxQueue, 1, &drawSubmitInfo, NULL ) != VK_SUCCESS ){
            LOG( "Could not submit to graphics queue" );
            break;
        }

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pWaitSemaphores = (VkSemaphore[]){ vCtx.renderSemaphore.semaphore },
            .waitSemaphoreCount = 1,
            .swapchainCount = 1,
            .pSwapchains = swapchain,
            .pImageIndices = &imageIndex,
            .pResults = NULL
        };

        if( vkQueuePresentKHR( *presentQueue, &presentInfo ) != VK_SUCCESS ){
            LOG( "Failed to present swapchain" );
            break;
        }

        vkQueueWaitIdle( *presentQueue );

    }
    
    vkDeviceWaitIdle( *dev );
    LOG( "Destroy semaphores" );
    if( pomSemaphoreDestroy( &vCtx.renderSemaphore ) ||
        pomSemaphoreDestroy( &vCtx.imageSemaphore ) ){
        LOG( "Failed to destroy semaphores" );
    }
    LOG( "Destroy command buffers" );
    if( pomCommandBuffersDestroy() ){
        LOG( "Failed to destroy command buffers" );
    }
    LOG( "Destroy command pool" );
    if( pomCommandPoolDestroy() ){
        LOG( "Failed to destroy command pool" );
    }
    LOG( "Destroy swapchain framebuffers" );
    if( pomSwapchainFramebuffersDestroy() ){
        LOG( "Failed to destroy swapchain framebuffers" );
    }
    LOG( "Destroy swapchain image views" );
    if( pomSwapchainImageViewsDestroy() ){
        LOG( "Failed to destroy swapchain image views" );
    }
    LOG( "Destroy Pipeline" );
    if( pomPipelineDestroy( &vCtx.pipelineCtx ) ){
        LOG( "Failed to destroy pipeline" );
    }
    LOG( "Destroy RenderPass" );
    if( pomRenderPassDestroy( &vCtx.renderPass ) ){
        LOG( "Failed to destroy RenderPass" );
    }
    LOG( "Destroy shaders" );
    if( pomShaderDestroy( &vCtx.basicShaders ) ){
        LOG( "Failed to destroy shaders" );
    }

    LOG( "Destroy logical device" );
    if( pomDestroyLogicalDevice() ){
        LOG( "Failed to create logical device" );
    }
    LOG( "Destroy device" )
    if( pomDestroyPhysicalDevice() ){
        LOG( "Error in destroying device" );
    }
    LOG( "Destroy Vk instance" );
    if( pomDestroyVkInstance() ){
        LOG( "Error in deletion of vk instance" );
    }

    LOG( "Destroy threadpool\n" );
    if( pomThreadpoolClear( &threadpoolCtx ) ){
        LOG( "Failed to destroy threadpool" );
    }

    if( saveSystemConfig( configPath ) ){
        LOG( "Couldn't write new config files" );
    }
    clearSystemConfig();

    LOG( "Program exit" );
    return EXIT_SUCCESS;
}


void loadModel( void *_userData ){
    // TODO - eventually cache the loaded models
    PomModelCtx *modelCtx = (PomModelCtx*) _userData;
    if( modelCtx->initialised ){
        // Model already loaded
        LOG( "Attempting to reload model %s", modelCtx->filePath );
        return;
    }

    if( loadBakedModel( modelCtx->filePath, &modelCtx->format, &modelCtx->dataBlock ) ){
        // Failed to load model
        LOG( "Failed to load model %s", modelCtx->filePath );
        modelCtx->initialised = false;
        return;
    }
    modelCtx->initialised = true;
    return;
}

void setupVulkan( void* _userData ){
    VulkanCtx *vCtx = (VulkanCtx*) _userData;
    vCtx->initialised = false;
    LOG( "Create Vk instance" );
    if( pomCreateVkInstance() ){
        LOG( "Error in instance creation" );
        return;
    }
    LOG( "Find device" );
    if( pomPickPhysicalDevice() ){
        LOG( "Error in physical device creation" );
        return;
    }
    LOG( "Create logical device" );
    if( pomCreateLogicalDevice() ){
        LOG( "Failed to create logical device" );
        return;
    }

    LOG( "Create shaders" );
    vCtx->basicShaders = (ShaderInfo){
        .vertexShaderPath = "./res/shaders/basicV.vert.spv",
        .fragmentShaderPath = "./res/shaders/basicF.frag.spv"
    };
    if( pomShaderCreate( &vCtx->basicShaders ) ){
        LOG( "Failed to create shaders" );
        return;
    }
    LOG( "Create RenderPass" );
    if( pomRenderPassCreate( &vCtx->renderPass ) ){
        LOG( "Failed to create RenderPass" );
        return;
    }
    vCtx->pipelineCtx=(PomPipelineCtx){
        .pipelineType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };
    LOG( "Create Pipeline" );
    if( pomPipelineCreate( &vCtx->pipelineCtx, &vCtx->basicShaders, &vCtx->renderPass ) ){
        LOG( "Failed to create Pipeline" );
        return;
    }
    LOG( "Create swapchain image views" );
    if( pomSwapchainImageViewsCreate() ){
        LOG( "Failed to create swapchain image views" );
        return;
    }
    LOG( "Create swapchain framebuffers" );
    if( pomSwapchainFramebuffersCreate( &vCtx->renderPass ) ){
        LOG( "Failed to create swapchain framebuffers" );
        return;
    }
    LOG( "Create command pool" );
    if( pomCommandPoolCreate() ){
        LOG( "Failed to create command pool" );
        return;
    }
    LOG( "Create command buffers" );
    if( pomCommandBuffersCreate() ){
        LOG( "Failed to create command buffers" );
        return;
    }
    LOG( "Record default renderpass" );
    if( pomRecordDefaultCommands( &vCtx->renderPass, &vCtx->pipelineCtx ) ){
        LOG( "Failed to record default renderpass" );
        return;
    }

    LOG( "Create semaphores" );
    if( pomSemaphoreCreate( &vCtx->imageSemaphore ) ||
        pomSemaphoreCreate( &vCtx->renderSemaphore ) ){
        LOG( "Failed to create semaphores");
        return;
    }
    LOG( "Vulkan set up successfully" );
    vCtx->initialised = true;
}
