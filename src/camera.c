#include "camera.h"
#define LOG( lvl, log, ... ) LOG_MODULE( lvl, camera, log, ##__VA_ARGS__ )

int pomCameraCreate( PomCameraCtx *_cameraCtx, const PomCameraCreateInfo *_createInfo ){
    if( _cameraCtx->initialised ){
        LOG( WARN, "Attempting to reinitialised camera" );
        return 1;
    }
    _cameraCtx->projectionMatrix = createProjectionMatrix( _createInfo->fovRad,
                                                           _createInfo->near,
                                                           _createInfo->far,
                                                           _createInfo->width,
                                                           _createInfo->height );
    _cameraCtx->viewMatrix = mat4x4Identity();
    _cameraCtx->initialised = true;
    return 0;
}