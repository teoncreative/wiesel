#version 450

// Minimal shadow vertex shader for opaque casters: position + skinning only.
// Paired with no fragment shader so the pipeline runs at 2x depth rate.

#define SHADOW_MAP_CASCADE_COUNT 4
#define MAX_BONES 256

layout(set = 0, binding = 0, std140) uniform Matrices {
    mat4 modelMatrix;
    mat3 normalMatrix;
    uint entityId;
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
layout(location = 7) in ivec4 inBoneIndices;
layout(location = 8) in vec4 inBoneWeights;

void main() {
    vec4 localPos = vec4(inVertexPosition, 1.0);

    float totalWeight = inBoneWeights.x + inBoneWeights.y
                      + inBoneWeights.z + inBoneWeights.w;
    if (totalWeight > 0.0) {
        mat4 skin = inBoneWeights.x * boneData.bones[inBoneIndices.x]
                  + inBoneWeights.y * boneData.bones[inBoneIndices.y]
                  + inBoneWeights.z * boneData.bones[inBoneIndices.z]
                  + inBoneWeights.w * boneData.bones[inBoneIndices.w];
        localPos = skin * localPos;
    }

    gl_Position = shadowMatrices.viewProjectionMatrix[cascadeIndex]
                * obj.modelMatrix * localPos;
}
