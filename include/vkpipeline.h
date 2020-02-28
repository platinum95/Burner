#include <vulkan/vulkan.h>
#include <stdbool.h>

typedef struct ShaderInfo ShaderInfo;
typedef struct PomPipelineCtx PomPipelineCtx;

struct ShaderInfo{
    const char * vertexShaderPath;
    const char * fragmentShaderPath;
    const char * geometryShaderPath;
    const char * tesselationShaderPath;

    // Max 4 stages (vertex, geometry, tesselation, fragment)
    VkPipelineShaderStageCreateInfo shaderStages[ 4 ];
    uint8_t numStages;

    bool initialised;
};

struct PomPipelineCtx{
    union{
        VkGraphicsPipelineCreateInfo graphicsPipelineInfo;
        VkComputePipelineCreateInfo computePipelineInfo;
    };

    VkPipeline pipeline;
    // Should be either VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    // or VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    VkStructureType pipelineType;
    bool initialised;
};


int pomShaderCreate( ShaderInfo *_shaderInfo );

int pomShaderDestroy( ShaderInfo *_shaderInfo );

int pomRenderPassCreate( VkRenderPass *_renderPass );
int pomRenderPassDestroy( VkRenderPass *_renderPass );

int pomPipelineCreate( PomPipelineCtx *_pipelineCtx, const ShaderInfo *_shaderInfo, VkRenderPass *_renderPass );
int pomPipelineDestroy( PomPipelineCtx *_pipeline );