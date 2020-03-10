#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "common.h"
#include "vulkan/vulkan.h"
#include <stdbool.h>
#include <stdatomic.h>
#include "cmore/linkedlist.h"
#include "vkmemory.h"

/**************
** Buffer decs
***************/
typedef struct PomVkBufferCtx PomVkBufferCtx;
struct PomVkBufferCtx{
    bool initialised;
    VkBuffer buffer;
    PomLinkedListCtx childViews; // TODO - this should be thread-safe
    _Atomic bool inUse; // TODO - this may need to be an "inUse counter"
    _Atomic bool bound;
    PomVkMemoryCtx memCtx;
    VkMemoryRequirements memoryRequirements;
    VkMemoryPropertyFlags memoryFlags;
    VkBufferCreateInfo bufferInfo;
};

int pomVkBufferCreate( PomVkBufferCtx *_buffCtx, VkBufferUsageFlags _usage,
                       VkDeviceSize _bSizeBytes, uint32_t _numQueueFamilies,
                       const uint32_t* _queueFamilies,
                       VkMemoryPropertyFlags memoryFlags );

int pomVkBufferDestroy( PomVkBufferCtx *_buffCtx );

int pomVkBufferBind( PomVkBufferCtx *_buffCtx, VkDeviceSize _offset );

int pomVkBufferUnbind( PomVkBufferCtx *_buffCtx );

// TODO - functions here for memory allocation/binding

/******************
 * BufferView decs
 ******************/
typedef struct PomVkBufferViewCtx PomVkBufferViewCtx;
struct PomVkBufferViewCtx{
    bool initialised;
    VkBufferView bufferView;
    PomVkBufferCtx *parentBuffer;
    _Atomic bool inUse;
};

int pomVkBufferViewCreate( PomVkBufferViewCtx *_buffViewCtx, PomVkBufferCtx *_buffCtx,
                           VkDeviceSize _offset, VkDeviceSize _range, VkFormat _format );

int pomVkBufferViewDestroy( PomVkBufferViewCtx *_buffViewCtx );

#endif // VK_BUFFER_H