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

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, main, log, ##__VA_ARGS__ )

// Allow default config path to be overruled by compile option
#ifndef DEFAULT_CONFIG_PATH
#define DEFAULT_CONFIG_PATH "./config.ini"
#endif

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

    LOG( "Create Vk instance" );
    if( pomCreateVkInstance() ){
        LOG( "Error in instance creation" );
    }
    LOG( "Find device" );
    if( pomPickPhysicalDevice() ){
        LOG( "Error in physical device creation" );
    }
    LOG( "Create logical device" );
    if( pomCreateLogicalDevice() ){
        LOG( "Failed to create logical device" );
    }

    LOG( "Create shaders" );
    ShaderInfo basicShaders = {
        .vertexShaderPath = "./res/shaders/basicV.vert.spv",
        .fragmentShaderPath = "./res/shaders/basicF.frag.spv"
    };
    if( pomShaderCreate( &basicShaders ) ){
        LOG( "Failed to create shaders" );
    }
    VkRenderPass renderPass;
    LOG( "Create RenderPass" );
    if( pomRenderPassCreate( &renderPass ) ){
        LOG( "Failed to create RenderPass" );
    }
    PomPipelineCtx pipelineCtx={
        .pipelineType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };
    LOG( "Create Pipeline" );
    if( pomPipelineCreate( &pipelineCtx, &basicShaders, &renderPass ) ){
        LOG( "Failed to create Pipeline" );
    }
    LOG( "Create swapchain image views" );
    if( pomSwapchainImageViewsCreate() ){
        LOG( "Failed to create swapchain image views" );
    }
    LOG( "Create swapchain framebuffers" );
    if( pomSwapchainFramebuffersCreate( &renderPass ) ){
        LOG( "Failed to create swapchain framebuffers" );
    }
    LOG( "Create command pool" );
    if( pomCommandPoolCreate() ){
        LOG( "Failed to create command pool" );
    }
    LOG( "Create command buffers" );
    if( pomCommandBuffersCreate() ){
        LOG( "Failed to create command buffers" );
    }
    LOG( "Record default renderpass" );
    if( pomRecordDefaultCommands( &renderPass, &pipelineCtx ) ){
        LOG( "Failed to record default renderpass" );
    }

    PomSemaphoreCtx imageSemaphore, renderSemaphore;
    LOG( "Create semaphores" );
    if( pomSemaphoreCreate( &imageSemaphore ) ||
        pomSemaphoreCreate( &renderSemaphore ) ){
        LOG( "Failed to create semaphores");
    }
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
                               imageSemaphore.semaphore, NULL, &imageIndex );

        if( imageIndex >= numCommandBuffers ){
            LOG( "Discrepency between swapchain image index and command buffer size.\
                  Requesting index %i for buffer size %i", imageIndex, numCommandBuffers );
            break;
        }
        VkSubmitInfo drawSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitSemaphores = (VkSemaphore[]){ imageSemaphore.semaphore },
            .waitSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]){ renderSemaphore.semaphore },
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
            .pWaitSemaphores = (VkSemaphore[]){ renderSemaphore.semaphore },
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
    if( pomSemaphoreDestroy( &renderSemaphore ) ||
        pomSemaphoreDestroy( &imageSemaphore ) ){
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
    if( pomPipelineDestroy( &pipelineCtx ) ){
        LOG( "Failed to destroy pipeline" );
    }
    LOG( "Destroy RenderPass" );
    if( pomRenderPassDestroy( &renderPass ) ){
        LOG( "Failed to destroy RenderPass" );
    }
    LOG( "Destroy shaders" );
    if( pomShaderDestroy( &basicShaders ) ){
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
    LOG( "Program exit" );

    if( saveSystemConfig( configPath ) ){
        LOG( "Couldn't write new config files" );
    }
    clearSystemConfig();
    return EXIT_SUCCESS;
}