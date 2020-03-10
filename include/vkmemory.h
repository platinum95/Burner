#ifndef VK_MEMORY_H
#define VK_MEMORY_H

#include "common.h"
#include <vulkan/vulkan.h>
#include <stdbool.h>

typedef struct PomVkMemoryCtx PomVkMemoryCtx;
struct PomVkMemoryCtx{
    VkDeviceMemory memory;
    bool initialised;
};

int pomVkAllocateMemory( PomVkMemoryCtx *_memCtx,
                         VkMemoryPropertyFlags _memFlags, 
                         VkMemoryRequirements *_memReq,
                         VkMemoryRequirements *_memDesired );

int pomVkFreeMemory( PomVkMemoryCtx *_memCtx );

#endif // VK_MEMORY_H