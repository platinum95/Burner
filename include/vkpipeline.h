#include <vulkan/vulkan.h>
#include <stdbool.h>

typedef struct ShaderInfo ShaderInfo;

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

int pomShaderCreate( ShaderInfo *_shaderInfo );

int pomShaderDestroy( ShaderInfo *_shaderInfo );