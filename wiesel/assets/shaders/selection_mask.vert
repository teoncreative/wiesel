#version 450

#define MAX_BONES 256

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in vec3 inBiTangent;
layout (location = 6) in uint inFlags;
layout (location = 7) in ivec4 inBoneIndices;
layout (location = 8) in vec4 inBoneWeights;

layout (push_constant) uniform PushConstants {
    mat4 mvp;
    uint useSkinning;
};

layout (set = 0, binding = 0, std140) uniform BoneMatrices {
    mat4 bones[MAX_BONES];
} boneData;

void main() {
    vec4 pos = vec4(inPosition, 1.0);

    if (useSkinning != 0) {
        float totalWeight = inBoneWeights.x + inBoneWeights.y
        + inBoneWeights.z + inBoneWeights.w;
        if (totalWeight > 0.0) {
            mat4 skin = inBoneWeights.x * boneData.bones[inBoneIndices.x]
            + inBoneWeights.y * boneData.bones[inBoneIndices.y]
            + inBoneWeights.z * boneData.bones[inBoneIndices.z]
            + inBoneWeights.w * boneData.bones[inBoneIndices.w];
            pos = skin * pos;
        }
    }

    gl_Position = mvp * pos;
}
