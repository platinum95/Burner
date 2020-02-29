#ifndef VK_SYNCHRONISATION_H
#define VK_SYNCHRONISATION_H

#include "common.h"
#include <vulkan/vulkan.h>
#include <stdbool.h>

typedef struct PomSemaphoreCtx PomSemaphoreCtx;

struct PomSemaphoreCtx{
    VkSemaphore semaphore;
    bool initialised;
};

int pomSemaphoreCreate( PomSemaphoreCtx *_semaphoreCtx );

int pomSemaphoreDestroy( PomSemaphoreCtx *_semaphoreCtx );

#endif //VK_SYNCHRONISATION_H