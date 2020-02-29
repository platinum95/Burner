#include "vkpresentation.h"
#include "common.h"
#include "vkdevice.h"
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdlib.h>

#define LOG( level, log, ... ) LOG_MODULE( level, vkpresentation, log, ##__VA_ARGS__ )

typedef struct SwapchainImageViews SwapchainImageViews;

struct SwapchainImageViews{
    VkImageView *imageViews;
    uint32_t numViews;

    bool initialised;
};

SwapchainImageViews swapchainImageViews = { 0 };

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