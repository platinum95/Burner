#include "vkpipeline.h"
#include "common.h"
#include "vkdevice.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define LOG( level, log, ... ) LOG_MODULE( level, vkpipeline, log, ##__VA_ARGS__ )

/**************
 * Shader defs
***************/
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

static VkFormat _pomFormatFromDataType( PomShaderDataTypes _dataType ){
    switch( _dataType ){
        case SHADER_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case SHADER_INT8:
            return VK_FORMAT_R8_SINT;
        case SHADER_INT16:
            return VK_FORMAT_R16_SINT;
        case SHADER_INT32:
            return VK_FORMAT_R32_SINT;
        case SHADER_INT64:
            return VK_FORMAT_R64_SINT;
        case SHADER_VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
        case SHADER_VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case SHADER_VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
    return VK_FORMAT_UNDEFINED;
}

static size_t _pomSizeFromDataType( PomShaderDataTypes _dataType ){
    // TODO - verify these values; alignment may be an issue
    switch( _dataType ){
        case SHADER_FLOAT:
            return sizeof( float );
        case SHADER_INT8:
            return sizeof( int8_t );
        case SHADER_INT16:
            return sizeof( int16_t );
        case SHADER_INT32:
            return sizeof( int32_t );
        case SHADER_INT64:
            return sizeof( int64_t );
        case SHADER_VEC2:
            return 2 * sizeof( float );
        case SHADER_VEC3:
            return 3 * sizeof( float );
        case SHADER_VEC4:
            return 4 * sizeof( float );
        default:
            LOG( ERR, "Failed to get format data size" );
            return 0;
    }
    LOG( ERR, "Failed to get format data size" );
    return 0;
}

static int pomCreateShaderModule2( VkShaderModule *_module, VkDevice _dev, PomShaderFormat *_format ){
    uint32_t *shaderData = _format->shaderBytecodeOffset;
    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = _format->shaderBytecodeSizeBytes,
        .pCode = shaderData
    };

    if( vkCreateShaderModule( _dev, &moduleInfo, NULL, _module ) != VK_SUCCESS ){
        LOG( ERR, "Failed to create shader module for %s", _format->shaderNameOffset );
        return 1;
    }
    return 0;
}

int pomShaderCreate( ShaderInfo *_shaderInfo ){
    // TODO - add input attribute/binding description loading from shader
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

    if( !pomShaderFormatLoad( _shaderInfo->vertexShaderPath, &_shaderInfo->shaderFormats[ 0 ] ) ){
        LOG( ERR, "Failed to load vertex shader blob" );
        return 1;
    }
    // Create vertex module
    VkShaderModule vertexModule;
    if( pomCreateShaderModule2( &vertexModule, *dev, _shaderInfo->shaderFormats[ 0 ] ) ){
        LOG( ERR, "Could not create vertex module of shader" );
        return 1;
    }
    VkPipelineShaderStageCreateInfo *vertexStageInfo = &_shaderInfo->shaderStages[ currStage++ ];

    if( !pomShaderFormatLoad( _shaderInfo->fragmentShaderPath, &_shaderInfo->shaderFormats[ 1 ] ) ){
        LOG( ERR, "Failed to load fragment shader blob" );
        return 1;
    }
    // Create fragment module
    VkShaderModule fragmentModule;
    if( pomCreateShaderModule2( &fragmentModule, *dev, _shaderInfo->shaderFormats[ 1 ] ) ){
        LOG( ERR, "Could not create fragment module of shader" );
        vkDestroyShaderModule( *dev, fragmentModule, NULL );
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

    // Set up interface
    // TODO - extend to other stages, not just Vertex
    ShaderInterfaceInfo *shaderInterface = &_shaderInfo->shaderInputAttributes;
    size_t attributeStride = 0;
    for( uint32_t i = 0; i < _shaderInfo->shaderFormats[ 0 ]->numAttributeInfo; i++ ){
        PomShaderAttributeInfo *attrInfo = &_shaderInfo->shaderFormats[ 0 ]->attributeInfoOffset[ i ];
        attributeStride += _pomSizeFromDataType( attrInfo->dataType );
        shaderInterface->inputAttribs[ i ].binding = 0;
        shaderInterface->inputAttribs[ i ].location = attrInfo->location;
        shaderInterface->inputAttribs[ i ].offset = 0;
        shaderInterface->inputAttribs[ i ].format = _pomFormatFromDataType( attrInfo->dataType );
        
    }
    shaderInterface->inputBinding.binding = 0;
    shaderInterface->inputBinding.stride = attributeStride;
    shaderInterface->inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    shaderInterface->numInputs = _shaderInfo->shaderFormats[ 0 ]->numAttributeInfo;
    shaderInterface->totalStride = attributeStride;

    // Set up descriptors
    // TODO - Set destinction handling. For now, descriptors put in different sets
    // TODO - For now, RG-local descriptor is set 0, model-local is 1+
    uint32_t numDescriptors = _shaderInfo->shaderFormats[ 0 ]->numDescriptorInfo;
    ShaderDescriptorSetCtx *setCtx = &shaderInterface->descriptorSetLayoutCtx;
    if( numDescriptors > 0 ){
        VkDescriptorSetLayout *setLayouts = 
            (VkDescriptorSetLayout*) malloc( numDescriptors * sizeof( VkDescriptorSetLayout ) );
        setCtx->layouts = setLayouts;
        setCtx->numLayouts = numDescriptors;
        setCtx->numRenderGroupLocalLayouts = 1;
        setCtx->renderGroupSetLayouts = setLayouts;
        PomShaderDescriptorInfo *rgLocalDescInfo = 
            &_shaderInfo->shaderFormats[ 0 ]->descriptorInfoOffset[ 0 ];
        VkDescriptorSetLayoutBinding renderGroupLocalLayoutBinding = {
            .binding = rgLocalDescInfo->binding,
            .descriptorType = rgLocalDescInfo->type,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL
        };
        VkDescriptorSetLayoutCreateInfo rgLayoutCreateInfo = {
            .bindingCount = 1,
            .pBindings = &renderGroupLocalLayoutBinding,
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
        };
        if( vkCreateDescriptorSetLayout( *dev, &rgLayoutCreateInfo, NULL, &setLayouts[ 0 ] ) !=
                VK_SUCCESS ){
            LOG( ERR, "Failed to create descriptor set layout for descriptor %s",\
                 rgLocalDescInfo->nameOffset );
            // TODO - cleanup here
            return 1;
        }

        uint32_t modelLocalDescriptorCount = numDescriptors - 1;
        setCtx->numModelLocalLayouts = modelLocalDescriptorCount;
        if( modelLocalDescriptorCount > 0 ){
            setCtx->modelSetLayouts = &setLayouts[ 1 ];
            VkDescriptorSetLayoutCreateInfo modelLayoutCreateInfo = {
                .bindingCount = 1,
                .pBindings = NULL,
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
            };
            for( uint32_t i = 0; i < modelLocalDescriptorCount; i++ ){
                PomShaderDescriptorInfo *modelLocalDescInfo = 
                    &_shaderInfo->shaderFormats[ 0 ]->descriptorInfoOffset[ 1 + i ];
                VkDescriptorSetLayoutBinding modelUboLayoutBinding = {
                    .binding = modelLocalDescInfo->binding,
                    .descriptorType = modelLocalDescInfo->type,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .pImmutableSamplers = NULL
                };
                modelLayoutCreateInfo.pBindings = &modelUboLayoutBinding;
                if( vkCreateDescriptorSetLayout( *dev, &modelLayoutCreateInfo,
                                                 NULL, &setLayouts[ i + 1 ] ) != VK_SUCCESS ){
                    LOG( ERR, "Failed to create descriptor set layout for descriptor %s",\
                         modelLocalDescInfo->nameOffset );
                    // TODO - resource cleanup here
                    return 1;
                }

            }
        }
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
    free( _shaderInfo->shaderInputAttributes.descriptorSetLayoutCtx.layouts );
    _shaderInfo->initialised = false;

    return 0;
}


const ShaderDescriptorSetCtx* pomShaderGetDescriptorSetLayoutCtx( const ShaderInfo *_shaderInfo ){
    if( !_shaderInfo->initialised ){
        LOG( WARN, "Trying to get descriptor set for uninitialised shader" );
        return NULL;
    }
    return &_shaderInfo->shaderInputAttributes.descriptorSetLayoutCtx;
}

/******************
 * RenderPass defs
*******************/

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

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colourAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
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

/***************
 * Pipeline defs
****************/

int pomPipelineCreate( PomPipelineCtx *_pipelineCtx, const ShaderInfo *_shaderInfo, VkRenderPass *_renderPass ){
    if( !_shaderInfo->initialised ){
        LOG( ERR, "Attempting to create pipeline with uninitialised shaders" );
        return 1;
    }
    // Create vertex input info.

    // For now, we have only 1 binding description per pipeline
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pVertexAttributeDescriptions = _shaderInfo->shaderInputAttributes.inputAttribs,
        .pVertexBindingDescriptions = &_shaderInfo->shaderInputAttributes.inputBinding,
        .vertexAttributeDescriptionCount = _shaderInfo->shaderInputAttributes.numInputs,
        .vertexBindingDescriptionCount = 1
    };
    // Make a copy of the shader info for our pipeline context.
    // Could take a reference to in the input shaderInfo parameter
    // but we don't know what the scope of that is.
    _pipelineCtx->shaderInfo = *_shaderInfo;
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
    
    const ShaderDescriptorSetCtx *shaderLayouts = pomShaderGetDescriptorSetLayoutCtx( _shaderInfo );
    if( !shaderLayouts ){
        LOG( ERR, "Failed to get shader description set layouts for pipeline" );
        return 1;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.setLayoutCount = shaderLayouts->numLayouts;
    pipelineLayoutInfo.pSetLayouts = shaderLayouts->layouts;

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