#ifndef VK_RENDERGOUP_H
#define VK_RENDERGOUP_H

#include "common.h"
#include <stdbool.h>
#include "cmore/linkedlist.h"
#include "vkpipeline.h"
#include "vkmodel.h"
#include "vkdescriptor.h"
#include <stdint.h>

typedef struct PomVkRenderGroupCtx PomVkRenderGroupCtx;
typedef struct PomVkRenderGroupDescriptorSets PomVkRenderGroupDescriptorSets;

struct PomVkRenderGroupCtx{
    bool initialised;
    PomPipelineCtx *pipelineCtx;
    uint32_t numModels;
    PomVkModelCtx **modelList;
    uint32_t numModelDescriptorSets;
    uint32_t numRenderGroupDescriptorSets;
    uint32_t numDescriptorCopies;

    VkDescriptorSet *allDescriptorSets;
    
    uint32_t numLocalDescriptors;
    PomVkDescriptorCtx *localDescriptors;

    size_t setsPerSwapchainImage;
};

int pomVkRenderGroupCreate( PomVkRenderGroupCtx *_renderGroupCtx, PomPipelineCtx *_pipelineCtx,
                            uint32_t _numLocalDescriptors, PomVkDescriptorCtx *_localDescriptorsCtx,
                            uint32_t numModels, PomVkModelCtx *_models[] );

int pomVkRenderGroupDestroy( PomVkRenderGroupCtx *_renderGroupCtx );

// Require command buffer to be begun before calling this, and to be finished after calling
int pomVkRenderGroupRecord( PomVkRenderGroupCtx *_renderGroupCtx, VkCommandBuffer _cmdBuffer,
                            uint32_t bufferIdx );

int pomVkRenderGroupAllocateDescriptorSets( PomVkRenderGroupCtx *_renderGroupCtx,
                                            VkDescriptorPool _descriptorPool );

#endif // VK_RENDERGOUP_H