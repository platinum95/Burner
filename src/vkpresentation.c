#include "vkpresentation.h"
#include "common.h"
#include "vkdevice.h"
#include <stdbool.h>
#include <stdlib.h>

// TODO - move swapchain creation from Device module to this module

#define LOG( level, log, ... ) LOG_MODULE( level, vkpresentation, log, ##__VA_ARGS__ )

typedef struct SwapchainImageViews SwapchainImageViews;
typedef struct SwapchainFramebuffers SwapchainFramebuffers;

struct SwapchainImageViews{
    VkImageView *imageViews;
    uint32_t numViews;

    bool initialised;
};

struct SwapchainFramebuffers{
    VkFramebuffer * framebuffers;
    uint32_t numFramebuffers;

    bool initialised;
};



SwapchainImageViews swapchainImageViews = { 0 };
SwapchainFramebuffers swapchainFramebuffers = { 0 };

int pomSwapchainImageViewsCreate(){
    if( swapchainImageViews.initialised ){
        LOG( WARN, "Swap chain image views already initialised" );
        return 1;
    }
    uint32_t numImages;
    VkImage *swapchainImages = pomGetSwapchainImages( &numImages );
    if( !swapchainImages ){
        LOG( ERR, "Could not get swapchain images" );
        return 1;
    }

    swapchainImageViews.numViews = numImages;
    swapchainImageViews.imageViews = (VkImageView*) malloc( sizeof( VkImageView ) * numImages );

    VkFormat *format = pomGetSwapchainImageFormat();
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for swapchain image view creation" );
        free( swapchainImageViews.imageViews );
        return 1;
    }
    for( uint32_t i = 0; i < numImages; i++ ){
        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[ i ],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = *format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };
        if( vkCreateImageView( *dev, &createInfo, NULL, &swapchainImageViews.imageViews[ i ] ) != VK_SUCCESS ){
            LOG( ERR, "Could not create swapchain imageviews" );
            for( uint32_t j = 0; j < i; j++ ){
                vkDestroyImageView( *dev, swapchainImageViews.imageViews[ j ], NULL );
            }
            free( swapchainImageViews.imageViews );
            return 1;
        }
    }

    swapchainImageViews.initialised = true;
    return 0;
}

int pomSwapchainImageViewsDestroy(){
    if( !swapchainImageViews.initialised ){
        LOG( WARN, "Attempting to destroy uninitialised swap chain images" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Logical device destroyed before swapchain image views" );
        free( swapchainImageViews.imageViews );
        return 1;
    }
    for( uint32_t i = 0; i < swapchainImageViews.numViews; i++ ){
        vkDestroyImageView( *dev, swapchainImageViews.imageViews[ i ], NULL );
    }

    free( swapchainImageViews.imageViews );
    swapchainImageViews.initialised = false;
    return 0;
}


int pomSwapchainFramebuffersCreate( VkRenderPass *_renderPass ){
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create framebuffer with no available logical device" );
        return 1;
    }
    if( swapchainFramebuffers.initialised ){
        LOG( WARN, "Swapchain framebuffer already initialised" );
        return 1;

    }
    if( !swapchainImageViews.initialised ){
        // Initialise the swapchain imageviews before we create the framebuffers
        if( pomSwapchainImageViewsCreate() ){
            return 1;
        }
    }

    VkExtent2D *swapchainExtent = pomGetSwapchainExtent();
    uint32_t width = swapchainExtent->width;
    uint32_t height = swapchainExtent->height;

    uint32_t numFramebuffers = swapchainImageViews.numViews;
    swapchainFramebuffers.numFramebuffers = numFramebuffers;
    swapchainFramebuffers.framebuffers = (VkFramebuffer*) malloc( sizeof( VkFramebuffer ) * numFramebuffers );

    for( uint32_t i = 0; i < swapchainImageViews.numViews; i++ ){
        VkFramebufferCreateInfo framebufferInfo;
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = (VkImageView[]){ swapchainImageViews.imageViews[ i ] };
        framebufferInfo.height = height;
        framebufferInfo.width = width;
        framebufferInfo.renderPass = *_renderPass;
        framebufferInfo.layers = 1;

        if( vkCreateFramebuffer( *dev, &framebufferInfo, NULL, &swapchainFramebuffers.framebuffers[ i ] ) != VK_SUCCESS ){
            LOG( ERR, "Could not create swapchain framebuffer" );
            for( uint32_t j = 0; j < i; j++ ){
                vkDestroyFramebuffer( *dev, swapchainFramebuffers.framebuffers[ j ], NULL );
            }
            free( swapchainFramebuffers.framebuffers );
            return 1;
        }
    }

    swapchainFramebuffers.initialised = true;
    return 0;
}

int pomSwapchainFramebuffersDestroy(){
    if( !swapchainFramebuffers.initialised ){
        LOG( WARN, "Attempting to destroy uninitialised swapchain framebuffer" );
        return 1;
    }
    // Destroy the swapchain image views since we may have created them
    pomSwapchainImageViewsDestroy();

    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy framebuffer with no available logical device" );
        return 1;
    }

    for( uint32_t i = 0; i < swapchainFramebuffers.numFramebuffers; i++ ){
        vkDestroyFramebuffer( *dev, swapchainFramebuffers.framebuffers[ i ], NULL );
    }
    swapchainFramebuffers.initialised = false;
    return 0;
}