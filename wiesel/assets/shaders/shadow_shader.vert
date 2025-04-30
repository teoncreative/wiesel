#version 450

// todo: pass via specialization constant
#define SHADOW_MAP_CASCADE_COUNT 4

layout(set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
} obj;

layout(set = 1, binding = 0, std140) uniform ShadowMapMatrices {
    mat4 viewProjectionMatrix[SHADOW_MAP_CASCADE_COUNT];
    int enableShadows;
} shadowMatrices;

layout(push_constant) uniform Push {
    int cascadeIndex;
};

layout(location = 0) in vec3 inVertexPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inVertexNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in uint inFlags;

//layout(location = 0) out float outDepth;
layout(location = 0) out vec2 outUV;
layout(location = 1) out uint outFlags;

void main() {
	outUV = inUV;
	outFlags = inFlags;
    vec4 worldPos4 = obj.modelMatrix * vec4(inVertexPosition, 1.0);
    // lightViewProj is projection * viewMatrix of the light
    gl_Position = shadowMatrices.viewProjectionMatrix[cascadeIndex] * worldPos4;
}
