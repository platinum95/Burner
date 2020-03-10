#version 450
#extension GL_ARB_separate_shader_objects : enable

// Taken from vulkan tutorials
layout( location = 0 ) in vec3 vertexPos;
layout( location = 1 ) in vec3 vertexNorm;
layout( location = 2 ) in vec3 vertexTangent;
layout( location = 3 ) in vec3 vertexBitangent;
layout( location = 4 ) in vec2 uvCoord;

layout( location = 0 ) out vec3 fragColor;

void main() {
    gl_Position = vec4( vertexPos, 1.0);
    fragColor = vec3( 0.5, 0.6, 0.7 );
}