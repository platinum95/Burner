#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "system_hw.h"
#include "cmore/hashmap.h"
#include "common.h"
#include "vkinstance.h"
#include "vkdevice.h"
#include "vkpipeline.h"
#include "vkpresentation.h"
#include "vkcommands.h"
#include "pomIO.h"
#include "vksynchronisation.h"
#include "cmore/threadpool.h"
#include "pomModelFormat.h"
#include "vkbuffer.h"
#include "vkrendergroup.h"
#include "vkmodel.h"
#include "camera.h"


// TODO - remove this once key input is properly implemented in PomIO
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Just going to use debug level for now
#define LOG( log, ... ) LOG_MODULE( DEBUG, main, log, ##__VA_ARGS__ )


// TODO - maybe move this into pomModelFormat.h
typedef struct PomModelCtx PomModelCtx;
struct PomModelCtx{
    PomModelFormat *format;
    uint8_t *dataBlock;
    const char *filePath;
    bool initialised;
};

const char *modelPaths[] = {
    "./res/models/nanosuit.pomf",
    "./res/models/buddha.pomf",
    "./res/models/barrel.pomf"
};
const size_t numModelPaths = sizeof( modelPaths ) / sizeof( char* );

typedef struct VulkanCtx VulkanCtx;
struct VulkanCtx{
    bool initialised;
    ShaderInfo basicShaders;
    VkRenderPass renderPass;
    PomPipelineCtx pipelineCtx;
    PomSemaphoreCtx imageSemaphore, renderSemaphore;
    uint32_t numBuffers;
    PomVkBufferCtx *modelBuffers;
    PomVkBufferViewCtx *modelBufferViews;
    uint32_t numModels;
    PomVkModelCtx *models;
    uint32_t numRenderGroups;
    PomVkRenderGroupCtx *renderGroups;
    PomVkDescriptorPoolCtx descriptorPoolCtx;
    PomCameraCtx camera;
};

PomCameraCtx *camera;

void setupVulkan( void* _userData );
void loadModel( void* _userData );
static int setupCommandBuffers( VulkanCtx *_vCtx );
//static int manualShaderSetup( ShaderInfo *_shaderInfo, VkDevice _device );

static void keyEvent( GLFWwindow* window, int key, int scancode, int action, int mods );

int main( int argc, char ** argv ){
    LOG( "Program Entry" );

    char * configPath;
    // Single argument pointing to config file,
    // or no argument for default
    if( argc == 2 ){
        configPath = argv[ 1 ];
    }
    else{
        configPath = DEFAULT_CONFIG_PATH;
    }
    LOG( "Config file: %s", configPath );


    if( loadSystemConfig( configPath ) ){
        printf( "Error loading configuration file\n" );
    }

    LOG( "Create Threadpool" );
    uint8_t numThreads = atoi( pomMapGetSet( &systemConfig.mapCtx, CONFIG_NUMTHREAD_KEY, DEFAULT_NUMTHREADS ) );
    PomThreadpoolCtx threadpoolCtx = { 0 };
    pomThreadpoolInit( &threadpoolCtx, numThreads );
    VulkanCtx vCtx = { 0 };

    // TODO - maybe move the whole setup stuff to a separate function altogether
    
    // Schedule all models to be loaded
    PomModelCtx models[ sizeof( modelPaths ) / sizeof( char* ) ] = { 0 };
    PomThreadpoolJob jobs[ sizeof( modelPaths ) / sizeof( char* ) ] = { 0 };
    for( uint32_t i = 0; i < numModelPaths; i++ ){
        PomModelCtx *model = &models[ i ];
        PomThreadpoolJob *job = &jobs[ i ];
        model->filePath = modelPaths[ i ];
        //loadModel( &models[ i ] );
        job->args = model;
        job->func = loadModel;
        pomThreadpoolScheduleJob( &threadpoolCtx, job );
    }

    // Schedule vulkan context to be set up
    PomThreadpoolJob vkSetupJob = { .func=setupVulkan, .args=&vCtx };
    pomThreadpoolScheduleJob( &threadpoolCtx, &vkSetupJob );

    // Wait for jobs to complete
    pomThreadpoolJoinAll( &threadpoolCtx );
    if( !vCtx.initialised ){
        LOG( "Vulkan failed to set up" );
        return 1;
    }
    VkDevice *device = pomGetLogicalDevice();

    // Verify all models were correctly loaded, and find number of required model ctxs
    uint32_t numModels = 0;
    for( uint32_t i = 0; i < numModelPaths; i++ ){
        if( !models[ i ].initialised ){
            LOG( "Failed to load model %s", models[ i ].filePath );
            return 1;
            // TODO - error handling here
        }
        numModels += models[ i ].format->numMeshInfo;
    }
    // Create models
    vCtx.models = (PomVkModelCtx*) calloc( numModels, sizeof( PomVkModelCtx ) );
    vCtx.numModels = numModels;
    
    uint32_t modelIdx = 0;
    for( uint32_t i = 0; i < numModelPaths; i++ ){
        PomModelCtx *modelCtx = &models[ i ];
        uint32_t numMesh = modelCtx->format->numMeshInfo;
        for( uint32_t modelMeshIdx = 0; modelMeshIdx <  numMesh; modelMeshIdx++ ){
            PomModelMeshInfo *meshInfo = &modelCtx->format->meshInfoOffset[ modelMeshIdx ];
            PomVkModelCtx *vkModelCtx = &vCtx.models[ modelIdx++ ];
            pomVkModelCreate( vkModelCtx, meshInfo );
            pomVkModelActivate( vkModelCtx );
        }
    }

    // Create camera
    const VkExtent2D *windowExtent = pomIoGetWindowExtent();
    PomCameraCreateInfo cameraInfo = {
        .far = 100.0f,
        .near = 0.1f,
        .height = windowExtent->height,
        .width = windowExtent->width,
        .fovRad = degToRad( 90 )
    };

    if( pomCameraCreate( &vCtx.camera, &cameraInfo ) ){
        LOG( "Failed to create camera" );
        return 1;
    }
    
    LOG( "Models loaded" );
    PomVkDescriptorCtx *cameraDescriptor = pomCameraGetDescriptor( &vCtx.camera );
    if( !cameraDescriptor ){
        LOG( "Failed to get camera descriptor" );
        // TODO - cleanup
        return 1;
    }

    // Create descriptor pool
    PomVkDescriptorPoolInfo poolInfo = { 0 };
    //5 swapchain images, 1 RGL per SCI, 1 ML per Model per SCI. (7 + 1) * 5 = 40;
    poolInfo.descriptorTypeSizes[ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ] = 40;
    if( pomVkDescriptorPoolCreate( &vCtx.descriptorPoolCtx, &poolInfo, *device ) ){
        LOG( "Failed to create descriptor pool " );
        // TODO - error handling
        return 1;
    }
    VkDescriptorPool descriptorPool = pomVkGetDescriptorPool( &vCtx.descriptorPoolCtx );
    if( !descriptorPool ){
        LOG( "Failed to get descriptor pool" );
        return 1;
    }

    PomVkModelCtx *renderGroupModels[] = { &vCtx.models[ 0 ], &vCtx.models[ 1 ],
                                           &vCtx.models[ 2 ], &vCtx.models[ 3 ],
                                           &vCtx.models[ 4 ], &vCtx.models[ 5 ],
                                           &vCtx.models[ 7 ] };
    // Set up renderpass
    PomVkRenderGroupCtx renderGroupCtx = { 0 };
    if( pomVkRenderGroupCreate( &renderGroupCtx, &vCtx.pipelineCtx,
                                1, cameraDescriptor,
                                vCtx.numModels - 1, renderGroupModels ) ){
        LOG( "Failed to create rendergroup" );
        return 1;
    }
    // Allocate descriptor sets for rendergroup
    pomVkRenderGroupAllocateDescriptorSets( &renderGroupCtx, descriptorPool );
    vCtx.renderGroups = &renderGroupCtx;
    vCtx.numRenderGroups = 1;
    setupCommandBuffers( &vCtx );

    // TODO - get rid of this
    camera = &vCtx.camera;
    glfwSetKeyCallback( getWindow(), keyEvent );

    uint32_t numCommandBuffers;
    uint32_t gfxQueueIdx, presentQueueIdx;
    VkCommandBuffer *commandBuffers = pomCommandBuffersGet( &numCommandBuffers );
    VkSwapchainKHR *swapchain = pomGetSwapchain();
    VkQueue *gfxQueue = pomDeviceGetGraphicsQueue( &gfxQueueIdx );
    VkQueue *presentQueue = pomDeviceGetPresentQueue( &presentQueueIdx );
    // Main loop
    while( !pomIoShouldClose() ){
        // Poll for IO events
        pomIoPoll();
        
        // Update Camera UBO
        if( pomCameraUpdateUBO( &vCtx.camera, *device ) ){
            LOG( "Failed to update camera UBO" );
            break;
        }

        // Draw a frame
        uint32_t imageIndex;
        vkAcquireNextImageKHR( *device, *swapchain, UINT64_MAX,
                               vCtx.imageSemaphore.semaphore, NULL, &imageIndex );

        if( imageIndex >= numCommandBuffers ){
            LOG( "Discrepency between swapchain image index and command buffer size.\
                  Requesting index %i for buffer size %i", imageIndex, numCommandBuffers );
            break;
        }
        VkSubmitInfo drawSubmitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitSemaphores = (VkSemaphore[]){ vCtx.imageSemaphore.semaphore },
            .waitSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]){ vCtx.renderSemaphore.semaphore },
            .signalSemaphoreCount = 1,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[ imageIndex ],
            .pWaitDstStageMask = (VkPipelineStageFlags[]){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
        };
        
        if( vkQueueSubmit( *gfxQueue, 1, &drawSubmitInfo, NULL ) != VK_SUCCESS ){
            LOG( "Could not submit to graphics queue" );
            break;
        }

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pWaitSemaphores = (VkSemaphore[]){ vCtx.renderSemaphore.semaphore },
            .waitSemaphoreCount = 1,
            .swapchainCount = 1,
            .pSwapchains = swapchain,
            .pImageIndices = &imageIndex,
            .pResults = NULL
        };

        if( vkQueuePresentKHR( *presentQueue, &presentInfo ) != VK_SUCCESS ){
            LOG( "Failed to present swapchain" );
            break;
        }

        vkQueueWaitIdle( *presentQueue );

    }
    
    vkDeviceWaitIdle( *device );
    // TODO - Destroy buffers

    free( vCtx.modelBuffers );
    LOG( "Destroy semaphores" );
    if( pomSemaphoreDestroy( &vCtx.renderSemaphore ) ||
        pomSemaphoreDestroy( &vCtx.imageSemaphore ) ){
        LOG( "Failed to destroy semaphores" );
    }
    LOG( "Destroy command buffers" );
    if( pomCommandBuffersDestroy() ){
        LOG( "Failed to destroy command buffers" );
    }
    LOG( "Destroy command pool" );
    if( pomCommandPoolDestroy() ){
        LOG( "Failed to destroy command pool" );
    }
    LOG( "Destroy swapchain framebuffers" );
    if( pomSwapchainFramebuffersDestroy() ){
        LOG( "Failed to destroy swapchain framebuffers" );
    }
    LOG( "Destroy swapchain image views" );
    if( pomSwapchainImageViewsDestroy() ){
        LOG( "Failed to destroy swapchain image views" );
    }
    LOG( "Destroy Pipeline" );
    if( pomPipelineDestroy( &vCtx.pipelineCtx ) ){
        LOG( "Failed to destroy pipeline" );
    }
    LOG( "Destroy RenderPass" );
    if( pomRenderPassDestroy( &vCtx.renderPass ) ){
        LOG( "Failed to destroy RenderPass" );
    }
    LOG( "Destroy shaders" );
    if( pomShaderDestroy( &vCtx.basicShaders ) ){
        LOG( "Failed to destroy shaders" );
    }

    LOG( "Destroy logical device" );
    if( pomDestroyLogicalDevice() ){
        LOG( "Failed to create logical device" );
    }
    LOG( "Destroy device" )
    if( pomDestroyPhysicalDevice() ){
        LOG( "Error in destroying device" );
    }
    LOG( "Destroy Vk instance" );
    if( pomDestroyVkInstance() ){
        LOG( "Error in deletion of vk instance" );
    }

    LOG( "Destroy threadpool\n" );
    if( pomThreadpoolClear( &threadpoolCtx ) ){
        LOG( "Failed to destroy threadpool" );
    }

    if( saveSystemConfig( configPath ) ){
        LOG( "Couldn't write new config files" );
    }
    clearSystemConfig();

    LOG( "Program exit" );
    return EXIT_SUCCESS;
}


void loadModel( void *_userData ){
    // TODO - eventually cache the loaded models
    PomModelCtx *modelCtx = (PomModelCtx*) _userData;
    if( modelCtx->initialised ){
        // Model already loaded
        LOG( "Attempting to reload model %s", modelCtx->filePath );
        return;
    }

    if( loadBakedModel( modelCtx->filePath, &modelCtx->format, &modelCtx->dataBlock ) ){
        // Failed to load model
        LOG( "Failed to load model %s", modelCtx->filePath );
        modelCtx->initialised = false;
        return;
    }
    modelCtx->initialised = true;
    return;
}

void setupVulkan( void* _userData ){
    VulkanCtx *vCtx = (VulkanCtx*) _userData;
    vCtx->initialised = false;
    LOG( "Create Vk instance" );
    if( pomCreateVkInstance() ){
        LOG( "Error in instance creation" );
        return;
    }
    LOG( "Find device" );
    if( pomPickPhysicalDevice() ){
        LOG( "Error in physical device creation" );
        return;
    }
    LOG( "Create logical device" );
    if( pomCreateLogicalDevice() ){
        LOG( "Failed to create logical device" );
        return;
    }

    VkDevice *dev;
    dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( "Could not get logical device" );
        return;
    }

    LOG( "Create shaders" );
    vCtx->basicShaders = (ShaderInfo){
        .vertexShaderPath = "./res/shaders/basicV.vert.psf",
        .fragmentShaderPath = "./res/shaders/basicF.frag.psf"
    };
    if( pomShaderCreate( &vCtx->basicShaders ) ){
        LOG( "Failed to create shaders" );
        return;
    }
    // Need to manually set shader attrib info for now
//    if( manualShaderSetup( &vCtx->basicShaders, *dev ) ){
//        LOG( "Failed to setup shader interface info" );
//        return;
 //   }

    LOG( "Create RenderPass" );
    if( pomRenderPassCreate( &vCtx->renderPass ) ){
        LOG( "Failed to create RenderPass" );
        return;
    }
    vCtx->pipelineCtx=(PomPipelineCtx){
        .pipelineType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    };
    LOG( "Create Pipeline" );
    if( pomPipelineCreate( &vCtx->pipelineCtx, &vCtx->basicShaders, &vCtx->renderPass ) ){
        LOG( "Failed to create Pipeline" );
        return;
    }
    LOG( "Create swapchain image views" );
    if( pomSwapchainImageViewsCreate() ){
        LOG( "Failed to create swapchain image views" );
        return;
    }
    LOG( "Create swapchain framebuffers" );
    if( pomSwapchainFramebuffersCreate( &vCtx->renderPass ) ){
        LOG( "Failed to create swapchain framebuffers" );
        return;
    }
    LOG( "Create command pool" );
    if( pomCommandPoolCreate() ){
        LOG( "Failed to create command pool" );
        return;
    }
    LOG( "Create command buffers" );
    if( pomCommandBuffersCreate() ){
        LOG( "Failed to create command buffers" );
        return;
    }
    /*
    LOG( "Record default renderpass" );
    if( pomRecordDefaultCommands( &vCtx->renderPass, &vCtx->pipelineCtx ) ){
        LOG( "Failed to record default renderpass" );
        return;
    }
    */

    LOG( "Create semaphores" );
    if( pomSemaphoreCreate( &vCtx->imageSemaphore ) ||
        pomSemaphoreCreate( &vCtx->renderSemaphore ) ){
        LOG( "Failed to create semaphores");
        return;
    }
    LOG( "Vulkan set up successfully" );
    vCtx->initialised = true;
}

static int setupCommandBuffers( VulkanCtx *_vCtx ){
    uint32_t numCmdBuffers;
    VkCommandBuffer * cmdBuffers = pomCommandBuffersGet( &numCmdBuffers );
    if( !cmdBuffers ){
        LOG( "Failed to get command buffers" );
        return 1;
    }
    VkDevice *dev = pomGetLogicalDevice();
    if( !dev ){
        LOG( "Attempting to record commands without available logical device" );
        return 1;
    }
    uint32_t numFramebuffers;
    VkFramebuffer *swapchainBuffers = pomSwapchainFramebuffersGet( &numFramebuffers );
    if( !swapchainBuffers ){
        LOG( "Attempting to record commands without valid framebuffers" );
        return 1;
    }
    VkExtent2D *swapchainExtent = pomGetSwapchainExtent();
    if( !swapchainExtent ){
        LOG( "Attempting to record commands without valid swapchain extent" );
        return 1;
    }

    for( uint32_t i = 0; i < numCmdBuffers; i++ ){
        // Begin command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        VkCommandBuffer cmdBuffer = cmdBuffers[ i ];
        if( vkBeginCommandBuffer( cmdBuffer, &commandBufferBeginInfo ) != VK_SUCCESS ){
            LOG( "Failed to begin recording command buffer" );
            return 1;
        }

        VkClearValue clearValue = {{{ 0.5f, 0.5f, 0.5f, 1.0f }}};
        
        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .framebuffer = swapchainBuffers[ i ],
            .renderArea.extent = *swapchainExtent,
            .renderArea.offset = (VkOffset2D){ .x = 0, .y = 0 },
            .renderPass = _vCtx->renderPass,
            .pClearValues = &clearValue,
            .clearValueCount = 1
        };
        vkCmdBeginRenderPass( cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

        for( uint32_t renderGroupIdx = 0; renderGroupIdx < _vCtx->numRenderGroups; renderGroupIdx++ ){
            pomVkRenderGroupRecord( &_vCtx->renderGroups[ renderGroupIdx ], cmdBuffer, i );
        }
        vkCmdEndRenderPass( cmdBuffer );

        if( vkEndCommandBuffer( cmdBuffer ) != VK_SUCCESS ){
            LOG( "Failed to record command" );
            return 1;
        }
    
    }
    return 0;
}


// static int manualShaderSetup( ShaderInfo *shaderInfo, VkDevice _device ){
//     // Set up VBO attributes
//     ShaderInterfaceInfo *shaderInterface = &shaderInfo->shaderInputAttributes;
//     shaderInterface->inputBinding.binding = 0;
//     shaderInterface->inputBinding.stride = sizeof( float ) * 14;
//     shaderInterface->inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//     size_t pOffset = 0;
//     // Vertex Position
//     shaderInterface->inputAttribs[ 0 ].binding = 0;
//     shaderInterface->inputAttribs[ 0 ].location = 0;
//     shaderInterface->inputAttribs[ 0 ].format = VK_FORMAT_R32G32B32_SFLOAT;
//     shaderInterface->inputAttribs[ 0 ].offset = 0;
//     pOffset += sizeof( float ) * 3;
//     // Vertex Normal
//     shaderInterface->inputAttribs[ 1 ].binding = 0;
//     shaderInterface->inputAttribs[ 1 ].location = 1;
//     shaderInterface->inputAttribs[ 1 ].format = VK_FORMAT_R32G32B32_SFLOAT;
//     shaderInterface->inputAttribs[ 1 ].offset = 0;
//     pOffset += sizeof( float ) * 3;
//     // Vertex tangent
//     shaderInterface->inputAttribs[ 2 ].binding = 0;
//     shaderInterface->inputAttribs[ 2 ].location = 2;
//     shaderInterface->inputAttribs[ 2 ].format = VK_FORMAT_R32G32B32_SFLOAT;
//     shaderInterface->inputAttribs[ 2 ].offset = 0;
//     pOffset += sizeof( float ) * 3;
//     // Vertex Bitangent
//     shaderInterface->inputAttribs[ 3 ].binding = 0;
//     shaderInterface->inputAttribs[ 3 ].location = 3;
//     shaderInterface->inputAttribs[ 3 ].format = VK_FORMAT_R32G32B32_SFLOAT;
//     shaderInterface->inputAttribs[ 3 ].offset = 0;
//     pOffset += sizeof( float ) * 3;
//     // UV Coord
//     shaderInterface->inputAttribs[ 4 ].binding = 0;
//     shaderInterface->inputAttribs[ 4 ].location = 4;
//     shaderInterface->inputAttribs[ 4 ].format = VK_FORMAT_R32G32_SFLOAT;
//     shaderInterface->inputAttribs[ 4 ].offset = 0;
//     pOffset += sizeof( float ) * 2;
    
//     shaderInterface->numInputs = 5;
//     shaderInterface->totalStride = pOffset;

//     // Set up Descriptor Set Layouts
//     VkDescriptorSetLayoutBinding cameraUboLayoutBinding = {
//         .binding = 0,
//         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//         .descriptorCount = 1,
//         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//         .pImmutableSamplers = NULL
//     };
    
//     VkDescriptorSetLayoutBinding modelUboLayoutBinding = {
//         .binding = 1,
//         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//         .descriptorCount = 1,
//         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//         .pImmutableSamplers = NULL
//     };
//     //VkDescriptorSetLayoutBinding bindings[] = { cameraUboLayoutBinding, modelUboLayoutBinding };
//     VkDescriptorSetLayoutCreateInfo rgLayoutCreateInfo = {
//         .bindingCount = 1,
//         .pBindings = &cameraUboLayoutBinding,
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
//     };
//     VkDescriptorSetLayoutCreateInfo modelLayoutCreateInfo = {
//         .bindingCount = 1,
//         .pBindings = &modelUboLayoutBinding,
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
//     };
//     VkDescriptorSetLayout *setLayouts;

//     setLayouts = (VkDescriptorSetLayout*) malloc( sizeof( VkDescriptorSetLayout ) * 2 );
//     if( vkCreateDescriptorSetLayout( _device, &rgLayoutCreateInfo, NULL, setLayouts ) !=
//             VK_SUCCESS ){
//         LOG( "Failed to create shader layout create info" );
//         return 1;
//     }
//     if( vkCreateDescriptorSetLayout( _device, &modelLayoutCreateInfo, NULL, &setLayouts[ 1 ] ) !=
//             VK_SUCCESS ){
//         LOG( "Failed to create shader layout create info" );
//         return 1;
//     }
//     shaderInterface->descriptorSetLayoutCtx.numLayouts = 2;
//     shaderInterface->descriptorSetLayoutCtx.numRenderGroupLocalLayouts = 1;
//     shaderInterface->descriptorSetLayoutCtx.numModelLocalLayouts = 1;
//     shaderInterface->descriptorSetLayoutCtx.renderGroupSetLayouts = setLayouts;
//     shaderInterface->descriptorSetLayoutCtx.modelSetLayouts = &setLayouts[ 1 ];
//     shaderInfo->shaderInputAttributes.descriptorSetLayoutCtx.layouts = setLayouts;

//     return 0;


// }

static void keyEvent( GLFWwindow* UNUSED(window), int key, int UNUSED(scancode), int action, int UNUSED(mods) ){
    BaseType xT = 0.0f, yT = 0.0f, zT = 0.0f;
    BaseType moveAmount = 1.5f;
    if( action != GLFW_PRESS ){
        return;
    }
    if( key == GLFW_KEY_W ){
        zT = moveAmount;
    }else if( key == GLFW_KEY_S ){
        zT = -moveAmount;
    }

    if( key == GLFW_KEY_A ){
        xT = moveAmount;
    }else if( key == GLFW_KEY_D ){
        xT = -moveAmount;
    }

    if( key == GLFW_KEY_LEFT_SHIFT ){
        yT = moveAmount;
    }else if( key == GLFW_KEY_LEFT_CONTROL ){
        yT = -moveAmount;
    }

    Vec4 tVector = Vec4Gen( xT, yT, zT, 1.0f );
    pomCameraTranslate( camera, tVector );
}