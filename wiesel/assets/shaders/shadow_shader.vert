#version 450

// todo: pass via specialization constant
#define SHADOW_MAP_CASCADE_COUNT 4
#define MAX_BONES 256

layout(set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
    uint entityId;
    // implicit 12 bytes padding to vec4 boundary
    vec4 colorTint;
    vec4 materialParams;
} obj;

layout(set = 1, binding = 0, std140) uniform ShadowMapMatrices {
    mat4 viewProjectionMatrix[SHADOW_MAP_CASCADE_COUNT];
    int enableShadows;
} shadowMatrices;

layout(set = 2, binding = 0, std140) uniform BoneMatrices {
    mat4 bones[MAX_BONES];
} boneData;

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
layout(location = 7) in ivec4 inBoneIndices;
layout(location = 8) in vec4 inBoneWeights;

//layout(location = 0) out float outDepth;
layout(location = 0) out vec2 outUV;
layout(location = 1) out uint outFlags;

void main() {
	outUV = inUV;
	outFlags = inFlags;

    vec4 localPos = vec4(inVertexPosition, 1.0);

    // Skeletal animation skinning
    float totalWeight = inBoneWeights.x + inBoneWeights.y
                      + inBoneWeights.z + inBoneWeights.w;
    if (totalWeight > 0.0) {
        mat4 skin = inBoneWeights.x * boneData.bones[inBoneIndices.x]
                  + inBoneWeights.y * boneData.bones[inBoneIndices.y]
                  + inBoneWeights.z * boneData.bones[inBoneIndices.z]
                  + inBoneWeights.w * boneData.bones[inBoneIndices.w];
        localPos = skin * localPos;
    }

    vec4 worldPos4 = obj.modelMatrix * localPos;
    // lightViewProj is projection * viewMatrix of the light
    gl_Position = shadowMatrices.viewProjectionMatrix[cascadeIndex] * worldPos4;
}
