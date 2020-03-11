#ifndef CAMERA_H
#define CAMERA_H
#include "common.h"
#include "pomMaths.h"
#include <stdbool.h>

typedef struct PomCameraCtx PomCameraCtx;
typedef struct PomCameraCreateInfo PomCameraCreateInfo;

struct PomCameraCtx{
    bool initialised;
    Mat4x4 projectionMatrix;
    Mat4x4 viewMatrix;
    Mat4x4 projectionViewMatrix;
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


#endif // CAMERA_H