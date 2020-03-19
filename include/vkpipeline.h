#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include "pomShaderFormat.h"

typedef struct ShaderDescriptorSetCtx ShaderDescriptorSetCtx;
typedef struct ShaderAttributeInfo ShaderAttributeInfo;
typedef struct ShaderInterfaceInfo ShaderInterfaceInfo;
typedef struct ShaderInfo ShaderInfo;
typedef struct PomPipelineCtx PomPipelineCtx;

struct ShaderAttributeInfo{
    uint32_t bindingLocation;
    size_t dataSize;
    uint32_t dataType; // Placeholder for datatype enum
};

struct ShaderDescriptorSetCtx{
    uint32_t numLayouts;
    VkDescriptorSetLayout *layouts;

    // The rest of the members just alias the above array
    uint32_t numModelLocalLayouts;
    uint32_t numRenderGroupLocalLayouts;

    VkDescriptorSetLayout *modelSetLayouts;
    VkDescriptorSetLayout *renderGroupSetLayouts;
};

struct ShaderInterfaceInfo{
    size_t totalStride;
    uint32_t numInputs;
    //ShaderAttributeInfo attributes[ 16 ]; // Limit ourselves to 16 inputs for now
    VkVertexInputBindingDescription inputBinding;
    VkVertexInputAttributeDescription inputAttribs[ 16 ];
    ShaderDescriptorSetCtx descriptorSetLayoutCtx;
};

struct ShaderInfo{
    const char * vertexShaderPath;
    const char * fragmentShaderPath;
    const char * geometryShaderPath;
    const char * tesselationShaderPath;

    // Max 4 stages (vertex, geometry, tesselation, fragment)
    VkPipelineShaderStageCreateInfo shaderStages[ 4 ];
    uint8_t numStages;

    ShaderInterfaceInfo shaderInputAttributes;
    PomShaderFormat *shaderFormats[ 4 ];    

    bool initialised;
};

struct PomPipelineCtx{
    union{
        VkGraphicsPipelineCreateInfo graphicsPipelineInfo;
        VkComputePipelineCreateInfo computePipelineInfo;
    };
    ShaderInfo shaderInfo;
    VkPipeline pipeline;
    // Should be either VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    // or VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    VkStructureType pipelineType;
    bool initialised;
};


int pomShaderCreate( ShaderInfo *_shaderInfo );

int pomShaderDestroy( ShaderInfo *_shaderInfo );
const ShaderDescriptorSetCtx* pomShaderGetDescriptorSetLayoutCtx( const ShaderInfo *_shaderInfo );

int pomRenderPassCreate( VkRenderPass *_renderPass );
int pomRenderPassDestroy( VkRenderPass *_renderPass );

int pomPipelineCreate( PomPipelineCtx *_pipelineCtx, const ShaderInfo *_shaderInfo, VkRenderPass *_renderPass );
int pomPipelineDestroy( PomPipelineCtx *_pipeline );

#endif //VK_PIPELINE_H