#include "common.h"
#include <vkpipeline.h>
#include <vulkan/vulkan.h>

int pomCommandPoolCreate();

int pomCommandPoolDestroy();

int pomCommandBuffersCreate();

int pomCommandBuffersDestroy();

int pomRecordDefaultCommands( VkRenderPass *_renderPass, PomPipelineCtx *_pipelineCtx );

VkCommandBuffer *pomCommandBuffersGet( uint32_t *numBuffers );
