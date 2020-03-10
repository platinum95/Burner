#ifndef VK_RENDERGOUP_H
#define VK_RENDERGOUP_H

#include "common.h"
#include <stdbool.h>
#include "cmore/linkedlist.h"
#include "vkpipeline.h"
#include "vkmodel.h"
#include <stdint.h>

typedef struct PomVkRenderGroupCtx PomVkRenderGroupCtx;

struct PomVkRenderGroupCtx{
    bool initialised;
    PomPipelineCtx *pipelineCtx;
    uint32_t numModels;
    PomVkModelCtx **modelList;
};

int pomVkRenderGroupCreate( PomVkRenderGroupCtx *_renderGroupCtx, PomPipelineCtx *_pipelineCtx,
                            uint32_t numModels, PomVkModelCtx *_models[] );

int pomVkRenderGroupDestroy( PomVkRenderGroupCtx *_renderGroupCtx );

// Require command buffer to be begun before calling this, and to be finished after calling
int pomVkRenderGroupRecord( PomVkRenderGroupCtx *_renderGroupCtx, VkCommandBuffer _cmdBuffer );

#endif // VK_RENDERGOUP_H