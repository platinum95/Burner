#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

// Taken from vulkan tutorials
layout( location = 0 ) in vec3 vertexPos;
layout( location = 1 ) in vec3 vertexNorm;
layout( location = 2 ) in vec3 vertexTangent;
layout( location = 3 ) in vec3 vertexBitangent;
layout( location = 4 ) in vec2 uvCoord;

// POM_ATTRIBUTE vertexPos 0 SHADER_VEC3

layout( location = 0 ) out vec3 fragColor;

layout( binding = 0 ) uniform CameraUBO {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 PvMatrix;
} cameraUbo;

layout( binding = 1 ) uniform ModelUBO {
    mat4 modelMatrix;
} modelUbo;

// Can put another UBO here for shading properties

mat4 projectionMatrix = mat4( 
0.05, 0, 0, 0, 
0, 0.05, 0, 0, 
0, 0, -0.05, 0, 
-0, -0, -0, 1
);

void main() {
    gl_Position = cameraUbo.projectionMatrix * cameraUbo.viewMatrix * vec4( vertexPos, 1.0 );
    //gl_Position = projectionMatrix * cameraUbo.viewMatrix * vec4( vertexPos, 1.0 );
    fragColor = vec3( 0.1, 0.1, 0.1 );
}