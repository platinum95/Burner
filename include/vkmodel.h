#ifndef VK_MODEL_H
#define VK_MODEL_H

#include "common.h"
#include "pomModelFormat.h"
#include "pomMaths.h"
#include "vkbuffer.h"
#include "vkdescriptor.h"

#include <stdbool.h>

typedef struct PomVkModelCtx PomVkModelCtx;
struct PomVkModelCtx{
    bool initialised;
    bool active;
    PomModelMeshInfo *modelMeshInfo;
    PomModelInfo *modelInfo;
    PomVkBufferCtx modelBuffer;

    Mat4x4 transformationMatrix;
    PomVkDescriptorCtx modelDescriptorCtx;

};

int pomVkModelCreate( PomVkModelCtx *_modelCtx, PomModelMeshInfo *_meshInfo );

int pomVkModelDestroy( PomVkModelCtx *_modelCtx );

// Indicate that model must be available to the GPU.
int pomVkModelActivate( PomVkModelCtx *_modelCtx );

// Indicate that model is not needed by GPU. This does not
// guarantee that the model will be removed, just that it
// may be removed.
int pomVkModelDeactivate( PomVkModelCtx *_modelCtx );

// Update model descriptor data in VRAM
int pomVkModelUpdateDescriptors( PomVkModelCtx *_modelCtx, VkDevice _device );

// Get the main model descriptor (UBO for now, maybe more later?)
PomVkDescriptorCtx* pomVkModelGetDescriptor( PomVkModelCtx *_modelCtx );

#endif //VK_MODEL_H