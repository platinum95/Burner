#include "camera.h"
#define LOG( lvl, log, ... ) LOG_MODULE( lvl, camera, log, ##__VA_ARGS__ )

int pomCameraCreate( PomCameraCtx *_cameraCtx, const PomCameraCreateInfo *_createInfo ){
    if( _cameraCtx->initialised ){
        LOG( WARN, "Attempting to reinitialised camera" );
        return 1;
    }
    _cameraCtx->cameraUboData.projectionMatrix = createProjectionMatrix( _createInfo->fovRad,
                                                           _createInfo->near,
                                                           _createInfo->far,
                                                           _createInfo->width,
                                                           _createInfo->height );
    _cameraCtx->cameraUboData.viewMatrix = mat4x4Identity();
    mat4x4ColumnVector( _cameraCtx->cameraUboData.viewMatrix, 3 ) = Vec4Gen( 0, 0, -10, 1 );

    _cameraCtx->cameraUboData.viewMatrix = mat4Translate( _cameraCtx->cameraUboData.viewMatrix,
                                                          Vec4Gen( 0, 0, -20, 1 ) );


    _cameraCtx->cameraUboData.projectionViewMatrix =
        mat4Mult( _cameraCtx->cameraUboData.projectionMatrix, 
                  _cameraCtx->cameraUboData.viewMatrix );
    
    // Set up camera UBo descriptor
    PomVkUniformBufferObject ubo;
    ubo.data = (void*) &_cameraCtx->cameraUboData;
    ubo.dataSize = sizeof( PomCameraUBO );
    if( pomVkDescriptorCreate( &_cameraCtx->cameraUboDescriptor, &ubo, NULL ) ){
        LOG( ERR, "Failed to create camera descriptor" );
        return 1;
    }
    _cameraCtx->initialised = true;
    return 0;
}

int pomCameraTranslate( PomCameraCtx *_cameraCtx, Vec4 _translation ){
    _cameraCtx->cameraUboData.viewMatrix = mat4Translate( _cameraCtx->cameraUboData.viewMatrix,
                                                          _translation );
    return 0;
}


int pomCameraDestroy( PomCameraCtx *_cameraCtx ){
    if( !_cameraCtx->initialised ){
        LOG( WARN, "Attempting to destroy uninitialised camera" );
        return 1;
    }
    if( pomVkDescriptorDestroy( &_cameraCtx->cameraUboDescriptor ) ){
        LOG( ERR, "Failed to destroy camera UBO descriptor" );
        return 1;
    }
    LOG( DEBUG, "Camera destroyed successfully" );
    return 0;
}

PomVkDescriptorCtx* pomCameraGetDescriptor( PomCameraCtx *_cameraCtx ){
    if( !_cameraCtx->initialised ){
        LOG( ERR, "Attempting to get descriptor of uninitialised camera" );
        return NULL;
    }
    return &_cameraCtx->cameraUboDescriptor;
}


int pomCameraUpdateUBO( PomCameraCtx *_cameraCtx, VkDevice _device ){
    return pomVkDescriptorUpdate( &_cameraCtx->cameraUboDescriptor, _device );
}