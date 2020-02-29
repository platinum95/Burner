#include <vulkan/vulkan.h>

int pomSwapchainImageViewsCreate();

int pomSwapchainImageViewsDestroy();

int pomSwapchainFramebuffersCreate( VkRenderPass *_renderPass );

int pomSwapchainFramebuffersDestroy();

VkFramebuffer *pomSwapchainFramebuffersGet( uint32_t *numBuffers );