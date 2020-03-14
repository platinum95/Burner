#ifndef CAMERA_H
#define CAMERA_H
#include "common.h"
#include "pomMaths.h"
#include "vkdescriptor.h"

#include <stdbool.h>

typedef struct PomCameraCtx PomCameraCtx;
typedef struct PomCameraCreateInfo PomCameraCreateInfo;
typedef struct PomCameraUBO PomCameraUBO;

struct PomCameraUBO{
    Mat4x4 projectionMatrix;
    Mat4x4 viewMatrix;
    Mat4x4 projectionViewMatrix;
};

struct PomCameraCtx{
    bool initialised;
    PomVkDescriptorCtx cameraUboDescriptor;
    PomCameraUBO cameraUboData;
};

struct PomCameraCreateInfo{
    Vec4 location;
    float width;
    float height;
    float near;
    float far;
    float fovRad;
};


int pomCameraCreate( PomCameraCtx *_cameraCtx, const PomCameraCreateInfo *_createInfo );

int pomCameraDestroy( PomCameraCtx *_cameraCtx );

PomVkDescriptorCtx* pomCameraGetDescriptor( PomCameraCtx *_cameraCtx );

int pomCameraTranslate( PomCameraCtx *_cameraCtx, Vec4 _translation );
int pomCameraUpdateUBO( PomCameraCtx *_cameraCtx, VkDevice _device );

#endif // CAMERA_H