#include "vkpipeline.h"
#include "common.h"
#include "vkdevice.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define LOG( level, log, ... ) LOG_MODULE( level, vkpipeline, log, ##__VA_ARGS__ )

static uint32_t * pomLoadShaderData( const char * _path, size_t *_size ){
    FILE *shaderFile = fopen( _path, "rb" );
    if( !shaderFile ){
        LOG( ERR, "Could not open shader file" );
        return NULL;
    }

    fseek( shaderFile, 0, SEEK_END );
    size_t fSize = ftell( shaderFile );
    fseek( shaderFile, 0, SEEK_SET );

    if( !fSize ){
        LOG( WARN, "Shader file read as empty" );
        fclose( shaderFile );
        return NULL;
    }
    *_size = fSize;
    uint32_t * data = (uint32_t*) malloc( fSize );
    // Make sure we have a multiple of sizeof( uint32_t )
    if( fSize % sizeof( uint32_t ) != 0 ){
        LOG( WARN, "Shader file size not as expected. Shader compilation may fail" );
    }
    size_t numBlocks = fSize / sizeof( uint32_t );

    size_t nRead = fread( data, sizeof( uint32_t ), numBlocks, shaderFile );
    if( nRead != numBlocks ){
        LOG( WARN, "Shader file load did not read as expected. Compilation may fail" );
    }

    fclose( shaderFile );
    return data;
}

static int pomCreateShaderModule( VkShaderModule *_module, const char *_shaderPath, VkDevice *_dev ){
    size_t shaderSize;
    uint32_t *shaderData = pomLoadShaderData( _shaderPath, &shaderSize );
    if( !shaderData ){
        LOG( ERR, "Shader stage creation failed" );
        return 1;
    }

    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderSize,
        .pCode = shaderData
    };

    int err = 0;
    if( vkCreateShaderModule( *_dev, &moduleInfo, NULL, _module ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create shader module for %s", _shaderPath );
        err = 1;
    }

    free( shaderData );
    return err;
}

int pomShaderCreate( ShaderInfo *_shaderInfo ){
    if( !_shaderInfo->vertexShaderPath || !_shaderInfo->fragmentShaderPath ){
        LOG( ERR, "Shader requires vertex and fragment stages" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for shader creation" );
        return 1;
    }
    uint8_t currStage = 0;

    // Create vertex module
    VkShaderModule vertexModule;
    if( pomCreateShaderModule( &vertexModule, _shaderInfo->vertexShaderPath, dev ) ){
        LOG( ERR, "Could not create vertex module of shader" );
        return 1;
    }
    VkPipelineShaderStageCreateInfo *vertexStageInfo = &_shaderInfo->shaderStages[ currStage++ ];

    // Create fragment module
    VkShaderModule fragmentModule;
    if( pomCreateShaderModule( &fragmentModule, _shaderInfo->fragmentShaderPath, dev ) ){
        LOG( ERR, "Could not create fragment module of shader" );
        vkDestroyShaderModule( *dev, vertexModule, NULL );
        return 1;
    }
    VkPipelineShaderStageCreateInfo *fragmentStageInfo = &_shaderInfo->shaderStages[ currStage++ ];

    // Create vertex/fragment stages
    // Limit ourselves to `main` entry point for now
    // TODO - add support for specifying entry point
    *vertexStageInfo = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexModule,
        .pName = "main"
    };
    *fragmentStageInfo = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragmentModule,
        .pName = "main"
    };
    
    // Check if we should create the tesselation module/stage
    if( _shaderInfo->tesselationShaderPath ){
        VkShaderModule tessModule;
        if( pomCreateShaderModule( &tessModule, _shaderInfo->tesselationShaderPath, dev ) ){
            LOG( ERR, "Could not create tesselation module of shader" );
            vkDestroyShaderModule( *dev, vertexModule, NULL );
            vkDestroyShaderModule( *dev, fragmentModule, NULL );
            return 1;
        }
        VkPipelineShaderStageCreateInfo *tessStageInfo = &_shaderInfo->shaderStages[ currStage++ ];
        *tessStageInfo = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            .module = tessModule,
            .pName = "main"
        };
    }

    // Now check if we should create the geometry module/stage
    // Check if we should create the tesselation module/stage
    if( _shaderInfo->geometryShaderPath ){
        VkShaderModule geometryModule;
        if( pomCreateShaderModule( &geometryModule, _shaderInfo->geometryShaderPath, dev ) ){
            LOG( ERR, "Could not create tesselation module of shader" );
            for( uint8_t i = 0; i < currStage; i++ ){
                vkDestroyShaderModule( *dev, _shaderInfo->shaderStages[ i ].module, NULL );
            }
            return 1;
        }
        VkPipelineShaderStageCreateInfo *geometryStageInfo = &_shaderInfo->shaderStages[ currStage++ ];
        *geometryStageInfo = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            .module = geometryModule,
            .pName = "main"
        };
    }

    _shaderInfo->numStages = currStage;
    _shaderInfo->initialised = true;
    return 0;
}

int pomShaderDestroy( ShaderInfo *_shaderInfo ){
    if( !_shaderInfo->initialised ){
        LOG( WARN, "Trying to destroy uninitialised shader" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "No logical device available for shader deletion" );
        return 1;
    }
    for( uint8_t i = 0; i < _shaderInfo->numStages; i++ ){
        vkDestroyShaderModule( *dev, _shaderInfo->shaderStages[ i ].module, NULL );
    }
    _shaderInfo->initialised = false;

    return 0;
}


int pomRenderPassCreate( VkRenderPass *_renderPass ){
    // TODO - make this more flexible

    // Get the swapchain images
    VkAttachmentDescription colourAttachment = { 
        .format = *pomGetSwapchainImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    /*
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    */

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colourAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    //  .dependencyCount = 1,
    //  .pDependencies = &dependency
    };

    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create RenderPass with no available logical device" );
        return 1;
    }
    if( vkCreateRenderPass( *dev, &renderPassInfo, NULL, _renderPass ) != VK_SUCCESS ) {
        LOG( ERR, "Failed to create RenderPass" );
        return 1;
    }
    

    return 0;
}

int pomRenderPassDestroy( VkRenderPass *_renderPass ){
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy RenderPass with no available logical device" );
        return 1;
    }
    vkDestroyRenderPass( *dev, *_renderPass, NULL );
    return 0;
}

int pomPipelineCreate( PomPipelineCtx *_pipelineCtx, const ShaderInfo *_shaderInfo, VkRenderPass *_renderPass ){
    if( !_shaderInfo->initialised ){
        LOG( ERR, "Attempting to create pipeline with uninitialised shaders" );
        return 1;
    }
    // Create vertex input info.
    // TODO - add VBO support
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pVertexAttributeDescriptions = NULL,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .vertexBindingDescriptionCount = 0
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .primitiveRestartEnable = VK_FALSE,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    // Need swapchain extent here
    VkExtent2D *swapchainExtent = pomGetSwapchainExtent();
    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = swapchainExtent->width,
        .height = swapchainExtent->height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissors;
    scissors.extent = *swapchainExtent;
    scissors.offset = (VkOffset2D) { .x=0, .y=0 };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .scissorCount = 1,
        .pScissors = &scissors,
        .viewportCount = 1,
        .pViewports = &viewport
    };

    VkPipelineRasterizationStateCreateInfo rasteriserInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .cullMode = VK_CULL_MODE_NONE,
        .depthBiasClamp = 0.0f,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasEnable = VK_FALSE,
        .depthBiasSlopeFactor = 0.0f,
        .depthClampEnable = VK_FALSE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .rasterizerDiscardEnable = VK_FALSE
    };
    
    VkPipelineMultisampleStateCreateInfo multisamplingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };
    
    VkPipelineColorBlendAttachmentState colourBlendAttachment = {
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .blendEnable = VK_FALSE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE
    };

    VkPipelineColorBlendStateCreateInfo colourBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colourBlendAttachment,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY
    };
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.setLayoutCount = 0;

    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to create pipeline with no available logical device" );
        return 1;
    }
    VkGraphicsPipelineCreateInfo *pipelineInfo = &_pipelineCtx->graphicsPipelineInfo;
    *pipelineInfo = (VkGraphicsPipelineCreateInfo){
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
        .pColorBlendState = &colourBlending,
        .pDepthStencilState = NULL,
        .pDynamicState = NULL,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pMultisampleState = &multisamplingInfo,
        .pRasterizationState = &rasteriserInfo,
        .stageCount = _shaderInfo->numStages,
        .pStages = _shaderInfo->shaderStages,
        .pTessellationState = NULL,
        .pVertexInputState = &vertexInputInfo,
        .pViewportState = &viewportState,
        .renderPass = *_renderPass,
        .subpass = 0,
        .layout = VK_NULL_HANDLE
    };
    VkPipelineLayout layout;

    if( vkCreatePipelineLayout( *dev, &pipelineLayoutInfo, NULL,
                                &layout ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create pipeline layout" );
        return 1;
    }
    pipelineInfo->layout = layout;

    if( vkCreateGraphicsPipelines( *dev, VK_NULL_HANDLE, 1,
                                   pipelineInfo, NULL, &_pipelineCtx->pipeline ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create graphics pipeline" );
        vkDestroyPipelineLayout( *dev, pipelineInfo->layout, NULL );
        return 1;
    }

    _pipelineCtx->initialised = true;
    return 0;
}



int pomPipelineDestroy( PomPipelineCtx *_pipeline ){
    if( !_pipeline->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised pipeline" );
        return 1;
    }
    VkDevice * dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( ERR, "Attempting to destroy pipeline with no available logical device" );
        return 1;
    }
    if( _pipeline->pipelineType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO ){
        vkDestroyPipelineLayout( *dev, _pipeline->graphicsPipelineInfo.layout, NULL );
    }else{
        vkDestroyPipelineLayout( *dev, _pipeline->computePipelineInfo.layout, NULL );
    }
    vkDestroyPipeline( *dev, _pipeline->pipeline, NULL );

    return 0;
}