#include "vkcommands.h"
#include "vkdevice.h"
#include "vkpresentation.h"
#include <stdlib.h>

#define LOG( level, log, ... ) LOG_MODULE( level, vkcommands, log, ##__VA_ARGS__ )

typedef struct PomCommandsCtx PomCommandsCtx;

struct PomCommandsCtx{
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    uint32_t numBuffers;

    bool initialisedPool;
    bool initialisedBuffers;
};

PomCommandsCtx commandsCtx = { 0 };

int pomCommandPoolCreate(){

    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create command pool without available logical device" );
        return 1;
    }
    uint32_t gfxIdx;
    
    VkQueue *gfxQueue = pomDeviceGetGraphicsQueue( &gfxIdx );
    if( !gfxQueue ){
        LOG( ERR, "Attempting to create command pool without a suitable queue" );
        return 1;
    }
    
    VkCommandPoolCreateInfo commandPoolInfo ={
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0
    };

    if( vkCreateCommandPool( *dev, &commandPoolInfo, NULL, &commandsCtx.commandPool ) != VK_SUCCESS ){
        LOG( ERR, "Could not create command pool" );
        return 0;
    }

    commandsCtx.initialisedPool = true;
    return 0;
}

int pomCommandPoolDestroy(){
    if( !commandsCtx.initialisedPool ){
        LOG( WARN, "Attempting to destroy uninitialised command pool" );
        return 1;
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy command pool without available logical device" );
        return 1;
    }
    vkDestroyCommandPool( *dev, commandsCtx.commandPool, NULL );
    commandsCtx.initialisedPool = false;
    return 0;
}

int pomCommandBuffersCreate(){
    if( commandsCtx.initialisedBuffers ){
        LOG( WARN, "Command buffers already initilaised" );
        return 1;
    }
    if( !commandsCtx.initialisedPool ){
        if( pomCommandPoolCreate() ){
            return 1;
        }
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create command buffers without available logical device" );
        return 1;
    }
    uint32_t numFramebuffers;
    VkFramebuffer *swapchainBuffers = pomSwapchainFramebuffersGet( &numFramebuffers );
    if( !swapchainBuffers ){
        LOG( ERR, "Attempting to create command buffers without valid framebuffers" );
        return 1;
    }
    commandsCtx.commandBuffers = (VkCommandBuffer*) malloc( sizeof( VkCommandBuffer ) * numFramebuffers );
    commandsCtx.numBuffers = numFramebuffers;

    VkCommandBufferAllocateInfo commandBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandsCtx.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = commandsCtx.numBuffers
    };

    if( vkAllocateCommandBuffers( *dev, &commandBufferInfo, commandsCtx.commandBuffers ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create command buffers" );
        return 1;
    }

    commandsCtx.initialisedBuffers = true;

    return 0;
}

int pomCommandBuffersDestroy(){
    if( !commandsCtx.initialisedBuffers ){
        LOG( WARN, "Attempting to destroy uninitialised command pool" );
        return 1;
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy command pool without available logical device" );
        return 1;
    }
    pomCommandPoolDestroy();
    free( commandsCtx.commandBuffers );
    commandsCtx.initialisedBuffers = false;
    return 0;
}

VkCommandBuffer *pomCommandBuffersGet( uint32_t *numBuffers ){
    if( !commandsCtx.initialisedBuffers ){
        LOG( ERR, "Attempting to get uninitialised command buffers" );
        return NULL;
    }
    *numBuffers = commandsCtx.numBuffers;
    return commandsCtx.commandBuffers;
}

int pomRecordDefaultCommands( VkRenderPass *_renderPass, PomPipelineCtx *_pipelineCtx ){
    if( !commandsCtx.initialisedBuffers ){
        LOG( ERR, "Attempting to record commands with uninitialised command buffers" );
        return 1;
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to record commands without available logical device" );
        return 1;
    }
    uint32_t numFramebuffers;
    VkFramebuffer *swapchainBuffers = pomSwapchainFramebuffersGet( &numFramebuffers );
    if( !swapchainBuffers ){
        LOG( ERR, "Attempting to record commands without valid framebuffers" );
        return 1;
    }
    VkExtent2D *swapchainExtent = pomGetSwapchainExtent();
    if( !swapchainExtent ){
        LOG( ERR, "Attempting to record commands without valid swapchain extent" );
        return 1;
    }

    for( uint32_t i = 0; i < commandsCtx.numBuffers; i++ ){
        // Begin command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        if( vkBeginCommandBuffer( commandsCtx.commandBuffers[ i ], &commandBufferBeginInfo ) != VK_SUCCESS ){
            LOG( ERR, "Failed to begin recording command buffer" );
            return 1;
        }

        VkClearValue clearValue = {{{ 0.5f, 0.5f, 0.5f, 1.0f }}};

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .framebuffer = swapchainBuffers[ i ],
            .renderArea.extent = *swapchainExtent,
            .renderArea.offset = (VkOffset2D){ .x = 0, .y = 0 },
            .renderPass = *_renderPass,
            .pClearValues = &clearValue,
            .clearValueCount = 1
        };

        vkCmdBeginRenderPass( commandsCtx.commandBuffers[ i ], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
        vkCmdBindPipeline( commandsCtx.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineCtx->pipeline );
        vkCmdDraw( commandsCtx.commandBuffers[ i ], 3, 1, 0, 0 );
        vkCmdEndRenderPass( commandsCtx.commandBuffers[ i ] );

        if( vkEndCommandBuffer( commandsCtx.commandBuffers[ i ] ) != VK_SUCCESS ){
            LOG( ERR, "Failed to record command" );
            return 1;
        }

    }
    return 0;
}
