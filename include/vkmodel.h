#ifndef VK_MODEL_H
#define VK_MODEL_H

#include "common.h"
#include "pomModelFormat.h"
#include "vkbuffer.h"

#include <stdbool.h>

typedef struct PomVkModelCtx PomVkModelCtx;
struct PomVkModelCtx{
    bool initialised;
    bool active;
    PomModelMeshInfo *modelMeshInfo;
    PomModelInfo *modelInfo;
    PomVkBufferCtx modelBuffer;

    float baseTransformationMatrix[ 16 ]; // This may only need to be 12
};

int pomVkModelCreate( PomVkModelCtx *_modelCtx, PomModelMeshInfo *_meshInfo );

int pomVkModelDestroy( PomVkModelCtx *_modelCtx );

// Indicate that model must be available to the GPU.
int pomVkModelActivate( PomVkModelCtx *_modelCtx );

// Indicate that model is not needed by GPU. This does not
// guarantee that the model will be removed, just that it
// may be removed.
int pomVkModelDeactivate( PomVkModelCtx *_modelCtx );

#endif //VK_MODEL_H