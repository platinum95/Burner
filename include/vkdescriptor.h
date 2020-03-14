#ifndef VK_DESCRIPTOR_H
#define VK_DESCRIPTOR_H

#include "common.h"
#include "vkbuffer.h"
#include <vulkan/vulkan.h>

typedef struct PomVkDescriptorCtx PomVkDescriptorCtx;
typedef struct PomVkUniformBufferObject PomVkUniformBufferObject;
typedef struct PomVkDescriptorMemoryInfo PomVkDescriptorMemoryInfo;

// Represents information on the descriptor memory
struct PomVkDescriptorMemoryInfo{
    
    PomVkBufferCtx *bufferCtx; // Pointer to existing buffer context
    VkDeviceSize uboOffset; // TODO - put this somewhere else
    PomVkBufferCtx bufferCtxOwned; // Copy of buffer context owned by this descriptor
        // TODO - imageCtx will go here, if applicable
};

// Represents data in main memory (not VRAM)
struct PomVkUniformBufferObject{
    size_t dataSize;
    // TODO - Data here should probably be atomic
    void *data;
};

struct PomVkDescriptorCtx{
    bool initialised;
    VkDescriptorBufferInfo descriptorBufferInfo;
    
    PomVkUniformBufferObject uboMainMemory; // TODO - better name here
    PomVkDescriptorMemoryInfo uboDeviceMemory; // TODO - ditto

};

// TODO - we're only using 1 buffer per descriptor for now. This should probably be 1
// buffer per swapchain image.



// _ubo and _memoryInfo are copied so can be caller scope-limited
int pomVkDescriptorCreate( PomVkDescriptorCtx *_descriptorCtx, 
                           const PomVkUniformBufferObject *_ubo, // TODO - extend beyond just UBOs
                           PomVkDescriptorMemoryInfo *_memoryInfo );

int pomVkDescriptorDestroy( PomVkDescriptorCtx *_descriptorCtx );

// Update the memory in VRAM with the memory in main memory
int pomVkDescriptorUpdate( PomVkDescriptorCtx *_descriptorCtx, VkDevice _device );

VkDescriptorBufferInfo* pomVkDescriptorGetBufferInfo( PomVkDescriptorCtx *_descriptorCtx );


/****
* Descriptor Pool declarations
*****/
typedef struct PomVkDescriptorPoolCtx PomVkDescriptorPoolCtx;
typedef struct PomVkDescriptorPoolInfo PomVkDescriptorPoolInfo;

struct PomVkDescriptorPoolInfo{
    uint32_t descriptorTypeSizes[ VK_DESCRIPTOR_TYPE_RANGE_SIZE ];
};

struct PomVkDescriptorPoolCtx{
    bool initialised;
    VkDescriptorPool descriptorPool;
    PomVkDescriptorPoolInfo poolInfo;
};



int pomVkDescriptorPoolCreate( PomVkDescriptorPoolCtx *_poolCtx,
                               const PomVkDescriptorPoolInfo *_poolInfo, VkDevice _device );

int pomVkDescriptorPoolDestroy( PomVkDescriptorPoolCtx *_poolCtx, VkDevice _device );

VkDescriptorPool pomVkGetDescriptorPool( PomVkDescriptorPoolCtx *_poolCtx );

#endif // VK_DESCRIPTOR_H